/* -*- c++ -*- */
/*
 * Copyright 2023 jmfriedt.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#include <gnuradio/io_signature.h>
#include "m17_coder_impl.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <algorithm>
#include <random>
#include <unistd.h>

#include "m17.h"
#include "m17_log.h"
#include <gnuradio/block_detail.h>
#include <gnuradio/buffer_reader.h>
#include "aes.h"
#include "uECC.h"

namespace gr
{
	namespace m17
	{

		m17_coder::sptr
		m17_coder::make(std::string src_id, std::string dst_id,
						int data, int encr_type, int encr_subtype, int aes_subtype, int can,
						std::string meta, std::string key,
						std::string priv_key, bool debug, bool signed_str, std::string seed, int eot_cnt,
						bool continuous)
		{
			return gnuradio::get_initial_sptr(new m17_coder_impl(src_id, dst_id, data, encr_type, encr_subtype,
																 aes_subtype, can, meta, key, priv_key, debug, signed_str, seed, eot_cnt, continuous));
		}

		/*
		 * The private constructor
		 */
		m17_coder_impl::m17_coder_impl(std::string src_id, std::string dst_id,
									   int data, int encr_type,
									   int encr_subtype, int aes_subtype, int can,
									   std::string meta, std::string key,
									   std::string priv_key, bool debug,
									   bool signed_str, std::string seed,
									   int eot_cnt, bool continuous) : gr::block("m17_coder", gr::io_signature::make(1, 1, sizeof(char)),
																gr::io_signature::make(1, 1, sizeof(float))),
													  _mode(M17_TYPE_STREAM), _data(data), _encr_subtype(encr_subtype), _aes_subtype(aes_subtype), _can(can), _meta(meta), _debug(debug),
													  _signed_str(signed_str), _eot_cnt(eot_cnt)
		{
			_continuous = continuous;
			set_encr_type(encr_type); // overwritten by set_seed()
			set_type(M17_TYPE_STREAM, data, _encr_type, encr_subtype, can); // default mode: STREAM
			set_aes_subtype(aes_subtype, encr_type);
			set_meta(meta); // depends on   ^^^ encr_subtype
			set_seed(seed); // depends on   ^^^ encr_subtype
			set_eot_cnt(eot_cnt);
			set_src_id(src_id);
			set_dst_id(dst_id);
			set_key(key);		   // AES key
			set_priv_key(priv_key); // signing key
			set_signed(signed_str);
			set_debug(debug);
			set_output_multiple(SYM_PER_FRA);

			init_state();
			message_port_register_in(pmt::mp("transmission_control"));
			set_msg_handler(
				pmt::mp("transmission_control"),
				boost::bind(&m17_coder_impl::switch_state, this,
							boost::placeholders::_1));
		}

		void m17_coder_impl::switch_state(const pmt::pmt_t &msg)
		{
			std::string cmd = "", val = "";

			if (pmt::is_symbol(msg))
			{
				cmd = pmt::symbol_to_string(msg);
			}
			else if (pmt::is_pair(msg))
			{
				const pmt::pmt_t &car = pmt::car(msg);
				if (pmt::is_symbol(car))
					cmd = pmt::symbol_to_string(car);
				const pmt::pmt_t &cdr = pmt::cdr(msg);
				if (pmt::is_symbol(cdr))
					val = pmt::symbol_to_string(cdr);
			}

			if (_continuous)
			{
				m17_log(tag(), "%s ignored (continuous mode)", cmd.size() ? cmd.c_str() : "Control message");
				return;
			}

			if (cmd == "SOT")
			{
				if (_active.load(std::memory_order_acquire))
				{
					m17_log(tag(), "SOT ignored (stream already active)");
					return;
				}

				_active.store(true, std::memory_order_release);
				_finished.store(false, std::memory_order_relaxed);
				set_mode(M17_TYPE_STREAM);
				m17_log(tag(), "TX start: stream");
				return;
			}

			if (cmd == "EOT")
			{
				if (!_active.load(std::memory_order_acquire))
				{
					m17_log(tag(), "EOT ignored (no active stream)");
					return;
				}

				_finished.store(true, std::memory_order_release);
				return;
			}

			if (cmd == "SMS")
			{
				// never interleave a packet transmission with an active stream - and never queue it:
				// the TX path of the hardware may not be ready for another transmission
				if (_active.load(std::memory_order_acquire))
				{
					m17_log(tag(), "SMS ignored (stream active)");
					return;
				}

				if (_pkt_pend.load(std::memory_order_acquire))
				{
					m17_log(tag(), "SMS ignored (previous SMS still being sent)");
					return;
				}

				if (val.size() > SMS_MAX_LEN)
				{
					m17_log(tag(), "SMS ignored (%zu bytes, the maximum is %d)", val.size(), SMS_MAX_LEN);
					return;
				}

				if (val.size())
				{
					m17_log(tag(), "TX start: SMS, %zu bytes: %s", val.size(), val.c_str());
					size_t n = val.size();
					memcpy(_text_msg, val.c_str(), n);
					_text_msg[n] = 0;
					_text_len.store(n, std::memory_order_relaxed);
					_pkt_pend.store(true, std::memory_order_release);
				}
				else
					m17_log(tag(), "SMS ignored (empty)");
				return;
			}

			m17_log(tag(), "Unknown control message ignored");
		}

		void m17_coder_impl::init_state(void)
		{
			_got_lsf = 0; // have we filled the LSF struct yet?
			_fn = 0;	  // 16-bit Frame Number (for the stream mode)
			_active.store(false, std::memory_order_relaxed);
			_finished.store(false, std::memory_order_relaxed);
			_send_preamble = true; // send preamble once in the work function
			_stale_flushed = false; // drop stale input on the first work call after SOT
			memset(_digest, 0, sizeof(_digest)); // every stream starts with an all-zero digest
			_scrambler_seed = _scrambler_key;	  // every stream restarts the scrambler keystream from the seed
			if (_meta_blocks > 1)				  // multi-block Text Data starts with block 1
			{
				_meta_idx = 0;
				memcpy(_lsf.meta, _meta_block[0], 14);
				update_LSF_CRC(&_lsf);
			}
		}

		void m17_coder_impl::set_encr_type(int encr_type)
		{
			switch (encr_type)
			{
			case 0:
				_encr_type = ENCR_NONE;
				break;
			case 1:
				_encr_type = ENCR_SCRAM;
				break;
			case 2:
				_encr_type = ENCR_AES;
				break;
			case 3:
				_encr_type = ENCR_RES;
				break;
			default:
				_encr_type = ENCR_NONE;
			}
		}

		void m17_coder_impl::set_signed(bool signed_str)
		{
			_signed_str = signed_str;
			set_type(_mode, _data, _encr_type, _encr_subtype, _can); // update the SIGNED bit in TYPE
		}

		void m17_coder_impl::set_debug(bool debug)
		{
			_debug = debug;
		}

		void m17_coder_impl::set_src_id(std::string src_id)
		{
			int length;

			memset(_src_id, 0, sizeof(_src_id));

			if (src_id.length() > 9)
				length = 9;
			else
				length = src_id.length();

			for (int i = 0; i < length; i++)
			{
				_src_id[i] = toupper(src_id.c_str()[i]);
			}

			encode_callsign_bytes(_lsf.src, _src_id); // 6 byte ID <- 9 char callsign

			uint16_t ccrc = LSF_CRC(&_lsf);
			_lsf.crc[0] = ccrc >> 8;
			_lsf.crc[1] = ccrc & 0xFF;
		}

		void m17_coder_impl::set_dst_id(std::string dst_id)
		{
			int length;

			memset(_dst_id, 0, sizeof(_dst_id));

			if (dst_id.length() > 9)
				length = 9;
			else
				length = dst_id.length();

			for (int i = 0; i < length; i++)
			{
				_dst_id[i] = toupper(dst_id.c_str()[i]);
			}

			encode_callsign_bytes(_lsf.dst, _dst_id); // 6 byte ID <- 9 char callsign

			uint16_t ccrc = LSF_CRC(&_lsf);
			_lsf.crc[0] = ccrc >> 8;
			_lsf.crc[1] = ccrc & 0xFF;
		}

		void m17_coder_impl::set_priv_key(std::string arg) // *UTF-8* encoded byte array
		{
			int length = arg.size();

			if (!length)
				return;

			_priv_key_loaded = true;

			int i = 0, j = 0;
			while ((j < 32) && (i < length))
			{
				if ((unsigned int)arg.data()[i] < 0xC2) // https://www.utf8-chartable.de/ TODO: why 0xC2?
				{
					_priv_key[j] = arg.data()[i];
					i++;
					j++;
				}
				else
				{
					_priv_key[j] =
						(arg.data()[i] - 0xC2) * 0x40 + arg.data()[i + 1];
					i += 2;
					j++;
				}
			}

			length = j; // index from 0 to length-1

			// the private key is never printed; with Debug on, show the derived public key
			// (the encoder itself does not need it - it is meant for the receiving side)
			uint8_t pub_key[64];
			if (length == 32 && uECC_compute_public_key(_priv_key, pub_key, _curve))
			{
				if (_debug && _started)
					m17_log(tag(), "Public key (derived from the private key): %s", m17_hex(pub_key, sizeof(pub_key)).c_str());
			}
			else
				m17_log(tag(), "WARNING: invalid private key - signatures will not verify");

			fflush(stdout);
		}

		void m17_coder_impl::set_key(std::string arg) // *UTF-8* encoded byte array
		{
			int length = arg.size();

			if (!length)
				return;

			int i = 0, j = 0;
			while ((j < 32) && (i < length))
			{
				if ((unsigned int)arg.data()[i] < 0xC2) // https://www.utf8-chartable.de/
				{
					_key[j] = arg.data()[i];
					i++;
					j++;
				}
				else
				{
					_key[j] = (arg.data()[i] - 0xC2) * 0x40 + arg.data()[i + 1];
					i += 2;
					j++;
				}
			}

			length = j; // index from 0 to length-1

			// the key itself is never printed
			if (_started)
				m17_log(tag(), "AES key changed (%d bytes)", length);

			fflush(stdout);
		}

		void m17_coder_impl::set_seed(std::string arg) // *UTF-8* encoded byte array
		{
			int length = arg.size();

			if (!length)
				return;

			int i = 0, j = 0;
			while ((j < 3) && (i < length))
			{
				if ((unsigned int)arg.data()[i] < 0xC2) // https://www.utf8-chartable.de/
				{
					_seed[j] = arg.data()[i];
					i++;
					j++;
				}
				else
				{
					_seed[j] = (arg.data()[i] - 0xC2) * 0x40 + arg.data()[i + 1];
					i += 2;
					j++;
				}
			}

			length = j; // index from 0 to length-1

			// the seed is the initial LFSR value; its length selects the LFSR (spec 2.0.x):
			// 1 byte = 8-bit, 2 bytes = 16-bit, 3 bytes = 24-bit
			_scrambler_key = 0;
			for (i = 0; i < length; i++)
				_scrambler_key = (_scrambler_key << 8) | _seed[i];
			_scrambler_subtype = length - 1;
			_scrambler_seed = _scrambler_key;

			if (_started)
				m17_log(tag(), "Scrambler seed changed (%d-bit)", 8 * length);
			if (_scrambler_key == 0)
				m17_log(tag(), "WARNING: an all-zero scrambler seed produces no scrambling");

			_encr_type = ENCR_SCRAM; // Scrambler key was passed
		}

		void m17_coder_impl::set_eot_cnt(int arg)
		{
			if (arg > 0)
				_eot_cnt = arg;
			else
				_eot_cnt = 1;
		}

		void m17_coder_impl::set_meta(std::string meta) // plain UTF-8 text (up to 52 bytes) if encr_subtype==0, otherwise a *UTF-8* encoded byte array
		{
			int length = 0;

			if (_encr_type == ENCR_AES) // with AES, META carries the nonce (set at the start of every transmission)
				return;

			memset(_lsf.meta, 0, sizeof(_lsf.meta));
			_meta_blocks = 0;
			_meta_text.clear();

			if (!meta.length())
			{
				uint16_t ccrc = LSF_CRC(&_lsf);
				_lsf.crc[0] = ccrc >> 8;
				_lsf.crc[1] = ccrc & 0xFF;
				return;
			}

			if (_encr_subtype == ENCR_NONE) // Text Data (spec 2.0.x): up to 4 blocks of 13 bytes, each with a Control Byte
			{
				if (meta.size() > 4 * 13)
				{
					m17_log(tag(), "WARNING: META text is %zu bytes, the maximum is 52 - not sent", meta.size());
				}
				else
				{
					// blocks are filled to 13 bytes; a multi-byte UTF-8 character may span two blocks
					_meta_text = meta;
					_meta_blocks = (meta.size() + 12) / 13;
					uint8_t used = (1 << _meta_blocks) - 1; // bit map of the used blocks
					for (int b = 0; b < _meta_blocks; b++)
					{
						_meta_block[b][0] = (used << 4) | (1 << b);
						memset(&_meta_block[b][1], ' ', 13); // padded with spaces
						size_t n = std::min<size_t>(13, meta.size() - 13 * b);
						memcpy(&_meta_block[b][1], meta.data() + 13 * b, n);
					}
					_meta_idx = 0;
					memcpy(_lsf.meta, _meta_block[0], 14);
				}
			}
			else
			{
				length = meta.size();

				int i = 0, j = 0;
				while ((j < 14) && (i < length))
				{
					if ((unsigned int)meta.data()[i] < 0xC2) // https://www.utf8-chartable.de/
					{
						_lsf.meta[j] = meta.data()[i];
						i++;
						j++;
					}
					else
					{
						_lsf.meta[j] =
							(meta.data()[i] - 0xC2) * 0x40 + meta.data()[i + 1];
						i += 2;
						j++;
					}
				}

				// length = j; // index from 0 to length-1
				length = j;
			}

			uint16_t ccrc = LSF_CRC(&_lsf);
			_lsf.crc[0] = ccrc >> 8;
			_lsf.crc[1] = ccrc & 0xFF;

			if (_started)
				m17_log(tag(), "META changed: %s", meta_desc(((uint16_t)_lsf.type[0] << 8) | _lsf.type[1]).c_str());
		}

		void m17_coder_impl::set_mode(int mode)
		{
			_mode = mode;
			set_type(_mode, _data, _encr_type, _encr_subtype, _can);
		}

		void m17_coder_impl::set_data(int data)
		{
			_data = data;
			set_type(_mode, _data, _encr_type, _encr_subtype, _can);
		}

		void m17_coder_impl::set_encr_subtype(int encr_subtype)
		{
			_encr_subtype = encr_subtype;
			set_type(_mode, _data, _encr_type, _encr_subtype, _can);
		}

		void m17_coder_impl::set_aes_subtype(int aes_subtype, int encr_type)
		{
			if (encr_type == ENCR_NONE)
				return;

			_aes_subtype = aes_subtype; // the key size reaches TYPE through the encryption subtype (see set_type())
		}

		void m17_coder_impl::set_can(int can)
		{
			_can = can;
			set_type(_mode, _data, _encr_type, _encr_subtype, _can);
		}

		void m17_coder_impl::set_type(int mode, int data, encr_t encr_type,
									  int encr_subtype, int can)
		{
			uint16_t prev = ((uint16_t)_lsf.type[0] << 8) | _lsf.type[1];
			uint16_t tmptype;
			tmptype =
				mode | (data << 1) | (encr_type << 3) | (encr_subtype << 5) | (can << 7) | (_signed_str ? M17_TYPE_SIGNED : 0);
			_lsf.type[0] = tmptype >> 8;   // MSB
			_lsf.type[1] = tmptype & 0xFF; // LSB
			uint16_t ccrc = LSF_CRC(&_lsf);
			_lsf.crc[0] = ccrc >> 8;
			_lsf.crc[1] = ccrc & 0xFF;
			if (_started && tmptype != prev)
				m17_log(tag(), "TYPE changed: %04X (%s)", tmptype, m17_type_str(tmptype).c_str());
		}

		// has the upstream block finished (no more input will ever arrive)?
		bool m17_coder_impl::input_done()
		{
			return detail() && detail()->input(0) && detail()->input(0)->done();
		}

		bool m17_coder_impl::stop()
		{
			if (_continuous && _active.load(std::memory_order_acquire))
				m17_log(tag(), "TX end: flowgraph stopped during the stream - no EoT sent (%d frames)", (int)(_fn & 0x7FFF));
			return gr::block::stop();
		}

		// tag for console output: the block alias if set in GRC, otherwise M17_ENC
		std::string m17_coder_impl::tag() const
		{
			return alias_set() ? alias() : std::string("M17_ENC");
		}

		// readable META description; multi-block Text Data is shown as the complete text
		std::string m17_coder_impl::meta_desc(uint16_t type)
		{
			if (_encr_type == ENCR_AES)
				return "AES nonce per transmission";
			if (_encr_type == ENCR_NONE && _encr_subtype == 0 && _meta_blocks > 1)
				return "META text \"" + _meta_text + "\" (" + std::to_string(_meta_blocks) + " blocks)";
			return m17_meta_str(_lsf.meta, type);
		}

		bool m17_coder_impl::start()
		{
			uint16_t type = ((uint16_t)_lsf.type[0] << 8) | _lsf.type[1];
			char src[10] = {0}, dst[10] = {0};
			decode_callsign_bytes(src, _lsf.src);
			decode_callsign_bytes(dst, _lsf.dst);

			std::string extra;
			if (_encr_type == ENCR_SCRAM && _scrambler_key == 0)
				extra += ", NO SCRAMBLER SEED";
			if (_encr_type == ENCR_AES)
			{
				uint8_t zero[32] = {0};
				if (!memcmp(_key, zero, sizeof(_key)))
					extra += ", NO AES KEY";
			}
			if (_signed_str && !_priv_key_loaded)
				extra += ", NO PRIVATE KEY";
			if (_debug)
				extra += ", debug on";

			m17_log(tag(), "Ready: %s -> %s, TYPE %04X (%s), %s%s", src, dst, type, m17_type_str(type).c_str(),
					meta_desc(type).c_str(), extra.c_str());

			if (_debug && _signed_str && _priv_key_loaded)
			{
				uint8_t pub_key[64];
				if (uECC_compute_public_key(_priv_key, pub_key, _curve))
					m17_log(tag(), "Public key (derived from the private key): %s", m17_hex(pub_key, sizeof(pub_key)).c_str());
			}

			_started = true;

			if (_continuous)
			{
				init_state();
				_stale_flushed = true; // all input belongs to the stream
				_active.store(true, std::memory_order_release);
				m17_log(tag(), "TX start: stream (continuous mode)");
			}

			return gr::block::start();
		}

		/*
		 * Our virtual destructor.
		 */
		m17_coder_impl::~m17_coder_impl()
		{
		}

		void
		m17_coder_impl::forecast(int noutput_items,
								 gr_vector_int &ninput_items_required)
		{
			if (_pkt_pend.load(std::memory_order_acquire))
			{
				// packet emission does not require stream input
				ninput_items_required[0] = 0;
			}
			else
			{
				// stream mode: the end of a stream (last frame, signature, EoT) needs no new input
				if (_active.load(std::memory_order_acquire) &&
					(_finished.load(std::memory_order_acquire) || (_continuous && input_done())))
					ninput_items_required[0] = 0;
				else
					ninput_items_required[0] = noutput_items / 12; // 16 in -> 192 out
			}
		}

		// scrambler PN sequence generation
		void m17_coder_impl::scrambler_sequence_generator()
		{
			int i = 0;
			uint32_t lfsr, bit;
			lfsr = _scrambler_seed;

			// the LFSR size (_scrambler_subtype) comes from the seed length or the received TYPE, never from the value
			// TODO: Set Frame Type based on scrambler_subtype value
			// run PN sequence with taps specified
			for (i = 0; i < 128; i++)
			{
				// get feedback bit with specified taps, depending on the scrambler_subtype
				if (_scrambler_subtype == 0)
					bit = (lfsr >> 7) ^ (lfsr >> 5) ^ (lfsr >> 4) ^ (lfsr >> 3);
				else if (_scrambler_subtype == 1)
					bit = (lfsr >> 15) ^ (lfsr >> 14) ^ (lfsr >> 12) ^ (lfsr >> 3);
				else if (_scrambler_subtype == 2)
					bit = (lfsr >> 23) ^ (lfsr >> 22) ^ (lfsr >> 21) ^ (lfsr >> 16);
				else
					bit = 0; // should never get here, but just in case

				bit &= 1;				  // truncate bit to 1 bit (required since I didn't do it above)
				lfsr = (lfsr << 1) | bit; // shift LFSR left once and OR bit onto LFSR's LSB
				lfsr &= 0xFFFFFF;		  // truncate lfsr to 24-bit (really doesn't matter)
				_scrambler_pn[i] = bit;
			}
			// pack bit array into byte array for easy data XOR
			pack_bit_array_into_byte_array(_scrambler_pn, _scr_bytes, 16);

			// save scrambler seed for next round
			_scrambler_seed = lfsr;

			// truncate seed so subtype will continue to set properly on subsequent passes
			if (_scrambler_subtype == 0)
				_scrambler_seed &= 0xFF;
			else if (_scrambler_subtype == 1)
				_scrambler_seed &= 0xFFFF;
			else if (_scrambler_subtype == 2)
				_scrambler_seed &= 0xFFFFFF;
		}

		// convert a user string (as hex octets) into a uint8_t array for key
		void m17_coder_impl::parse_raw_key_string(uint8_t *dest,
												  const char *inp)
		{
			uint16_t len = strlen(inp);

			if (len == 0)
				return; // return silently and pretend nothing happened

			memset(dest, 0, len / 2); // one character represents half of a byte

			if (!(len % 2)) // length even?
			{
				for (uint8_t i = 0; i < len; i += 2)
				{
					if (inp[i] >= 'a')
						dest[i / 2] |= (inp[i] - 'a' + 10) * 0x10;
					else if (inp[i] >= 'A')
						dest[i / 2] |= (inp[i] - 'A' + 10) * 0x10;
					else if (inp[i] >= '0')
						dest[i / 2] |= (inp[i] - '0') * 0x10;

					if (inp[i + 1] >= 'a')
						dest[i / 2] |= inp[i + 1] - 'a' + 10;
					else if (inp[i + 1] >= 'A')
						dest[i / 2] |= inp[i + 1] - 'A' + 10;
					else if (inp[i + 1] >= '0')
						dest[i / 2] |= inp[i + 1] - '0';
				}
			}
			else
			{
				if (inp[0] >= 'a')
					dest[0] |= inp[0] - 'a' + 10;
				else if (inp[0] >= 'A')
					dest[0] |= inp[0] - 'A' + 10;
				else if (inp[0] >= '0')
					dest[0] |= inp[0] - '0';

				for (uint8_t i = 1; i < len - 1; i += 2)
				{
					if (inp[i] >= 'a')
						dest[i / 2 + 1] |= (inp[i] - 'a' + 10) * 0x10;
					else if (inp[i] >= 'A')
						dest[i / 2 + 1] |= (inp[i] - 'A' + 10) * 0x10;
					else if (inp[i] >= '0')
						dest[i / 2 + 1] |= (inp[i] - '0') * 0x10;

					if (inp[i + 1] >= 'a')
						dest[i / 2 + 1] |= inp[i + 1] - 'a' + 10;
					else if (inp[i + 1] >= 'A')
						dest[i / 2 + 1] |= inp[i + 1] - 'A' + 10;
					else if (inp[i + 1] >= '0')
						dest[i / 2 + 1] |= inp[i + 1] - '0';
				}
			}
		}

		// AES nonce (spec 2.0.x): 32-bit timestamp (seconds since 2020-01-01 UTC) + 80 random bits,
		// stored in META; regenerated for every transmission
		void m17_coder_impl::new_nonce(void)
		{
			uint32_t ts = (uint32_t)(time(NULL) - epoch);
			_lsf.meta[0] = ts >> 24;
			_lsf.meta[1] = ts >> 16;
			_lsf.meta[2] = ts >> 8;
			_lsf.meta[3] = ts;
			std::random_device rd; // non-deterministic source (/dev/urandom on Linux)
			for (uint8_t i = 4; i < 14; i++)
				_lsf.meta[i] = rd() & 0xFF;
			update_LSF_CRC(&_lsf);

			if (_debug)
				m17_log(tag(), "AES nonce: %s", m17_hex(_lsf.meta, 14).c_str());
		}

		// encrypt one payload block in place, according to the selected encryption type
		void m17_coder_impl::encrypt_payload(uint8_t *data)
		{
			if (_encr_type == ENCR_AES)
			{
				// 128-bit counter: 112-bit nonce (META) followed by the 16-bit FN (EOS bit cleared)
				memcpy(_iv, _lsf.meta, 14);
				_iv[14] = (_fn >> 8) & 0x7F;
				_iv[15] = (_fn >> 0) & 0xFF;
				aes_ctr_bytewise_payload_crypt(_iv, _key, data, _aes_subtype);
			}
			else if (_encr_type == ENCR_SCRAM)
			{
				scrambler_sequence_generator();
				for (uint8_t i = 0; i < PAYLOAD_BYTES; i++)
					data[i] ^= _scr_bytes[i];
			}
		}

		// fold one payload block into the stream digest (signed streams)
		void m17_coder_impl::update_digest(const uint8_t *data)
		{
			for (uint8_t i = 0; i < sizeof(_digest); i++)
				_digest[i] ^= data[i];
			uint8_t tmp = _digest[0];
			for (uint8_t i = 0; i < sizeof(_digest) - 1; i++)
				_digest[i] = _digest[i + 1];
			_digest[sizeof(_digest) - 1] = tmp;
		}

		int
		m17_coder_impl::general_work(int noutput_items,
									 gr_vector_int &ninput_items,
									 gr_vector_const_void_star &input_items,
									 gr_vector_void_star &output_items)
		{
			const char *in = (const char *)input_items[0];
			float *out = (float *)output_items[0];
			int countin = 0;
			uint32_t countout = 0;

			//-------packet mode-------
			// preamble, LSF, 1..33 Packet Frames, EoT(s); continued across work calls if the output buffer is short
			if (_pkt_pend.load(std::memory_order_acquire))
			{
				if (_pkt_stage < 0) // start of a packet transmission: assemble the Packet Data
				{
					size_t n = _text_len.load(std::memory_order_acquire);
					_pkt_data[0] = 0x05; // protocol: SMS (null-terminated, UTF-8 encoded string)
					memcpy(&_pkt_data[1], _text_msg, n);
					_pkt_data[1 + n] = 0; // terminating null byte
					uint16_t crc = CRC_M17(_pkt_data, 1 + n + 1);
					_pkt_data[1 + n + 1] = crc >> 8;
					_pkt_data[1 + n + 2] = crc & 0xFF;
					_pkt_len = (int)n + 4;
					_pkt_frames = (_pkt_len + PKT_CHUNK - 1) / PKT_CHUNK;

					// packet mode uses its own LSF: the stream LSF (_lsf) is left untouched;
					// in packet mode only the Packet/Stream bit (0 = packet) and CAN are defined in TYPE
					_pkt_lsf = _lsf;
					uint16_t pkt_type = (uint16_t)(_can & 0xF) << 7;
					_pkt_lsf.type[0] = pkt_type >> 8;
					_pkt_lsf.type[1] = pkt_type & 0xFF;
					update_LSF_CRC(&_pkt_lsf);
					if (_debug)
						m17_log(tag(), "Packet: TYPE %04X, %d bytes in %d frame(s)", pkt_type, _pkt_len, _pkt_frames);

					_pkt_stage = 0;
				}

				while (countout + SYM_PER_FRA <= (uint32_t)noutput_items)
				{
					if (_pkt_stage == 0) // preamble
					{
						gen_preamble(out, &countout, PREAM_LSF); // writes at out[countout], advances countout
					}
					else if (_pkt_stage == 1) // LSF
					{
						gen_frame(out + countout, NULL, FRAME_LSF, &_pkt_lsf, 0, 0);
						countout += SYM_PER_FRA;
					}
					else if (_pkt_stage < 2 + _pkt_frames) // Packet Frames
					{
						int k = _pkt_stage - 2;					 // frame index
						int rem = _pkt_len - k * PKT_CHUNK;		 // bytes left, including this chunk
						int take = rem < PKT_CHUNK ? rem : PKT_CHUNK; // valid bytes in this frame
						uint8_t pkt_pld[PKT_CHUNK + 1] = {0};	 // 25-byte chunk (null-padded) + metadata byte
						memcpy(pkt_pld, &_pkt_data[k * PKT_CHUNK], take);
						if (rem > PKT_CHUNK)
							pkt_pld[PKT_CHUNK] = (k & 0x1F) << 2;			  // EOF = 0, frame counter
						else
							pkt_pld[PKT_CHUNK] = 0x80 | ((take & 0x1F) << 2); // EOF = 1, bytes in this frame
						gen_frame(out + countout, pkt_pld, FRAME_PKT, &_pkt_lsf, 0, 0);
						countout += SYM_PER_FRA;
					}
					else if (_pkt_stage < 2 + _pkt_frames + _eot_cnt) // EoT frame(s)
					{
						uint32_t tmp = 0;
						gen_eot(out + countout, &tmp);
						countout += tmp;
					}

					_pkt_stage++;
					if (_pkt_stage >= 2 + _pkt_frames + _eot_cnt) // done
					{
						_pkt_stage = -1;
						_pkt_pend.store(false, std::memory_order_release);
						m17_log(tag(), "TX end: SMS, %d frame(s)", _pkt_frames);
						break;
					}
				}

				consume_each(0); // packet mode transmission does not consume any input samples - all the data comes from the Message
				return countout;
			}

			//-------stream mode-------
			if (!_active.load(std::memory_order_acquire))
			{
				usleep(10e3);				   // TODO: fix this
				consume_each(ninput_items[0]); // consume input at idle to prevent buffer from filling with a lot of data
				return 0;
			}

			// first work call after SOT: the input present now arrived before SOT - drop it and return,
			// the stream (preamble, LSF, frames) starts with the next input
			if (!_stale_flushed)
			{
				_stale_flushed = true;
				consume_each(ninput_items[0]);
				return 0;
			}

			// continuous mode: when the source has ended and less than a full frame is left, end the stream
			if (_continuous && !_finished.load(std::memory_order_acquire) && input_done() && ninput_items[0] < PAYLOAD_BYTES)
				_finished.store(true, std::memory_order_release);

			const bool finished = _finished.load(std::memory_order_acquire);
			auto room = [&](int frames)
			{ return (uint32_t)noutput_items >= countout + frames * SYM_PER_FRA; };

			// start of stream: preamble and LSF go out together with the first frame
			if (!_got_lsf)
			{
				if ((!finished && ninput_items[0] < PAYLOAD_BYTES) || !room(3))
				{
					consume_each(0);
					return 0;
				}

				if (_send_preamble == true)
				{
					gen_preamble(out, &countout, PREAM_LSF); // 0 - LSF preamble, as opposed to 1 - BERT preamble
					_send_preamble = false;
				}

				// AES: a fresh nonce for every transmission, carried in META
				if (_encr_type == ENCR_AES)
					new_nonce();

				gen_frame(out + countout, NULL, FRAME_LSF, &_lsf, 0, 0);
				countout += SYM_PER_FRA; // gen frame always writes SYM_PER_FRA symbols = 192

				// multi-block Text Data: the LSF frame carried block 1, so the first superframe continues with block 2
				if (_meta_blocks > 1 && _encr_type != ENCR_AES)
				{
					_meta_idx = 1;
					memcpy(_lsf.meta, _meta_block[1], 14);
					update_LSF_CRC(&_lsf);
				}

				// check the SIGNED STREAM flag
				_signed_str = (_lsf.type[0] >> 3) & 1;
				if (_signed_str && !_priv_key_loaded)
					m17_log(tag(), "WARNING: signed stream without a private key - the signature will not verify");

				_got_lsf = 1;
			}

			// stream frames: exactly one frame per PAYLOAD_BYTES of input, nothing is skipped
			while (!finished && room(1) && (ninput_items[0] - countin) >= PAYLOAD_BYTES)
			{
				uint8_t data[PAYLOAD_BYTES]; // raw payload, packed bits
				memcpy(data, in + countin, PAYLOAD_BYTES);
				countin += PAYLOAD_BYTES;

				encrypt_payload(data);

				gen_frame(out + countout, data, FRAME_STR, &_lsf, _lich_cnt, _fn);
				countout += SYM_PER_FRA;		 // gen frame always writes SYM_PER_FRA symbols = 192
				_fn = (_fn + 1) % 0x8000;		 // increment FN
				_lich_cnt = (_lich_cnt + 1) % 6; // continue with next LICH_CNT

				// update the stream digest if required
				if (_signed_str)
					update_digest(data);

				// superframe boundary: multi-block Text Data moves on to the next block (one block per superframe)
				if (_lich_cnt == 0 && _meta_blocks > 1 && _encr_type != ENCR_AES)
				{
					_meta_idx = (_meta_idx + 1) % _meta_blocks;
					memcpy(_lsf.meta, _meta_block[_meta_idx], 14);
					update_LSF_CRC(&_lsf);
				}
			}

			// end of stream: last frame, signature (if signed), EoT(s) - all in one go
			if (finished)
			{
				_finalizing = true;

				// one final data frame, 4 signature frames if signed, then the EoT frame(s)
				int frames_needed = 1 + (_signed_str ? 4 : 0) + _eot_cnt;
				if (!room(frames_needed))
				{
					// not enough room for the entire remaining sequence, wait for a larger buffer
					consume_each(countin);
					return countout;
				}

				// the last frame carries the next block of input, zero-padded if less is available
				uint8_t data[PAYLOAD_BYTES] = {0};
				int take = std::min((int)ninput_items[0] - countin, (int)PAYLOAD_BYTES);
				if (take > 0)
				{
					memcpy(data, in + countin, take);
					countin += take;
				}

				encrypt_payload(data);

				// prevent re-entry before generating EOT
				_active.store(false, std::memory_order_release);

				_tx_frames = (_fn & 0x7FFF) + 1; // data frames in this transmission, the last one included
				if (!_signed_str)
					_fn |= 0x8000;
				gen_frame(out + countout, data, FRAME_STR, &_lsf, _lich_cnt, _fn);
				countout += SYM_PER_FRA;		 // gen frame always writes SYM_PER_FRA symbols = 192
				_lich_cnt = (_lich_cnt + 1) % 6; // continue with next LICH_CNT

				// if the stream is signed, transmit the signature (4 frames)
				if (_signed_str)
				{
					update_digest(data);

					// sign the digest
					uECC_sign(_priv_key, _digest, sizeof(_digest), _sig, _curve);

					// 4 frames with 512-bit signature
					_fn = 0x7FFC; // signature has to start at 0x7FFC to end at 0x7FFF (0xFFFF with EoT marker set)
					for (uint8_t i = 0; i < 4; i++)
					{
						gen_frame(out + countout, &_sig[i * PAYLOAD_BYTES], FRAME_STR, &_lsf, _lich_cnt, _fn);
						countout += SYM_PER_FRA; // gen frame always writes SYM_PER_FRA symbols = 192
						_fn = (_fn < 0x7FFE) ? _fn + 1 : (0x7FFF | 0x8000);
						_lich_cnt = (_lich_cnt + 1) % 6; // continue with next LICH_CNT
					}

					if (_debug == true)
						m17_log(tag(), "Signature: %s", m17_hex(_sig, sizeof(_sig)).c_str());
				}

				// send EOT frame(s)
				for (uint8_t i = 0; i < _eot_cnt; i++)
				{
					uint32_t tmp = 0;
					gen_eot(out + countout, &tmp);
					countout += tmp; // tmp should equal SYM_PER_FRA (192)
				}

				m17_log(tag(), "TX end: %d frames, %.2f s%s", _tx_frames, 0.04 * (2 + _tx_frames + (_signed_str ? 4 : 0) + _eot_cnt),
						_signed_str ? ", signed" : "");
				init_state();
				_finalizing = false;
			}

			// Tell runtime system how many input items we consumed on
			// each input stream.
			consume_each(countin);
			return countout;

			// https://lists.gnu.org/archive/html/discuss-gnuradio/2016-12/msg00206.html
			// returning -1 (which is the magical value for "there's nothing coming anymore, you can shut down") would normally end a flow graph
		}
	} /* namespace m17 */
} /* namespace gr */
