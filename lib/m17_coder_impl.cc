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
						std::string priv_key, bool debug, bool signed_str, std::string seed, int eot_cnt)
		{
			return gnuradio::get_initial_sptr(new m17_coder_impl(src_id, dst_id, data, encr_type, encr_subtype,
																 aes_subtype, can, meta, key, priv_key, debug, signed_str, seed, eot_cnt));
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
									   int eot_cnt) : gr::block("m17_coder", gr::io_signature::make(1, 1, sizeof(char)),
																gr::io_signature::make(1, 1, sizeof(float))),
													  _mode(M17_TYPE_STREAM), _data(data), _encr_subtype(encr_subtype), _aes_subtype(aes_subtype), _can(can), _meta(meta), _debug(debug),
													  _signed_str(signed_str), _eot_cnt(eot_cnt)
		{
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


			/*
			uint16_t ccrc = LSF_CRC (&_lsf);
			_lsf.crc[0] = ccrc >> 8;
			_lsf.crc[1] = ccrc & 0xFF;
			*/
			init_state();
			message_port_register_in(pmt::mp("transmission_control"));
			set_msg_handler(
				pmt::mp("transmission_control"),
				boost::bind(&m17_coder_impl::switch_state, this,
							boost::placeholders::_1));

			if (_debug == true && _got_lsf != 0)
			{
				// destination set to "@ALL"
				encode_callsign_bytes(_lsf.dst, "@ALL");

				// source set to "N0CALL"
				encode_callsign_bytes(_lsf.src, "N0CALL");

				// no enc or subtype field, normal 3200 voice
				_type = M17_TYPE_STREAM | M17_TYPE_VOICE | M17_TYPE_CAN(0);

				if (_encr_type == ENCR_AES) // AES ENC, 3200 voice
				{
					_type |= M17_TYPE_ENCR_AES;
					if (_aes_subtype == 0)
						_type |= M17_TYPE_ENCR_AES128;
					else if (_aes_subtype == 1)
						_type |= M17_TYPE_ENCR_AES192;
					else if (_aes_subtype == 2)
						_type |= M17_TYPE_ENCR_AES256;
				}
				else

					if (_encr_type == ENCR_SCRAM) // Scrambler ENC, 3200 Voice
				{
					_type |= M17_TYPE_ENCR_SCRAM;
					if (_scrambler_subtype == 0)
						_type |= M17_TYPE_ENCR_SCRAM_8;
					else if (_scrambler_subtype == 1)
						_type |= M17_TYPE_ENCR_SCRAM_16;
					else if (_scrambler_subtype == 2)
						_type |= M17_TYPE_ENCR_SCRAM_24;
				}

				// a signature key is loaded, OR this bit
				if (_priv_key_loaded)
				{
					_signed_str = 1;
					_type |= M17_TYPE_SIGNED;
				}

				_lsf.type[0] = (uint16_t)_type >> 8;
				_lsf.type[1] = (uint16_t)_type & 0xFF;

				// calculate LSF CRC (unclear whether or not this is only
				// needed here for debug, or if this is missing on every initial LSF)
				update_LSF_CRC(&_lsf);
			}

			if (_encr_type == ENCR_AES)
			{
				memcpy(&(_lsf.meta), _iv, 14);
				_iv[14] = (_fn >> 8) & 0x7F;
				_iv[15] = (_fn >> 0) & 0xFF;

				// re-calculate LSF CRC with IV insertion
				update_LSF_CRC(&_lsf);
			}

			// srand(time(NULL));	//random number generator (for IV rand() seed value)
			// memset(_key, 0, 32 * sizeof(uint8_t));
			// memset(_iv, 0, 16 * sizeof(uint8_t));
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

			time_t now = time(NULL);
			struct tm t;
			localtime_r(&now, &t);

			if (cmd == "SOT")
			{
				if (_active.load(std::memory_order_acquire))
				{
					fprintf(stderr, "[%02d:%02d:%02d] SOT ignored (stream already active)\n", t.tm_hour, t.tm_min, t.tm_sec);
					return;
				}

				_active.store(true, std::memory_order_release);
				_finished.store(false, std::memory_order_relaxed);
				set_mode(M17_TYPE_STREAM);
				fprintf(stderr, "[%02d:%02d:%02d] Start of Stream transmission\n", t.tm_hour, t.tm_min, t.tm_sec);
				return;
			}

			if (cmd == "EOT")
			{
				if (!_active.load(std::memory_order_acquire))
				{
					fprintf(stderr, "[%02d:%02d:%02d] EOT ignored (no active stream)\n", t.tm_hour, t.tm_min, t.tm_sec);
					return;
				}

				_finished.store(true, std::memory_order_release);
				fprintf(stderr, "[%02d:%02d:%02d] End of Stream transmission\n", t.tm_hour, t.tm_min, t.tm_sec);
				return;
			}

			if (cmd == "SMS")
			{
				// never interleave a packet transmission with an active stream - and never queue it:
				// the TX path of the hardware may not be ready for another transmission
				if (_active.load(std::memory_order_acquire))
				{
					fprintf(stderr, "[%02d:%02d:%02d] SMS ignored (stream active)\n", t.tm_hour, t.tm_min, t.tm_sec);
					return;
				}

				if (_pkt_pend.load(std::memory_order_acquire))
				{
					fprintf(stderr, "[%02d:%02d:%02d] SMS ignored (last transmission pending)\n", t.tm_hour, t.tm_min, t.tm_sec);
					return;
				}

				if (val.size())
				{
					fprintf(stderr, "[%02d:%02d:%02d] Start of text message transmission:\n%s\n", t.tm_hour, t.tm_min, t.tm_sec, val.c_str());
					size_t n = std::min(val.size(), sizeof(_text_msg) - 1);
					memcpy(_text_msg, val.c_str(), n);
					_text_msg[n] = 0;
					_text_len.store(n, std::memory_order_relaxed);
					_pkt_pend.store(true, std::memory_order_release);
				}
				else
					fprintf(stderr, "[%02d:%02d:%02d] Empty packet data\n", t.tm_hour, t.tm_min, t.tm_sec);
				return;
			}

			fprintf(stderr, "[%02d:%02d:%02d] Strange message received\n", t.tm_hour, t.tm_min, t.tm_sec);
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
		}

		void m17_coder_impl::set_encr_type(int encr_type)
		{
			switch (encr_type)
			{
			case 0:
				_encr_type = ENCR_NONE;
				fprintf(stderr, "Encryption type: none\n");
				break;
			case 1:
				_encr_type = ENCR_SCRAM;
				fprintf(stderr, "Encryption type: scrambler\n");
				break;
			case 2:
				_encr_type = ENCR_AES;
				fprintf(stderr, "Encryption type: AES\n");
				break;
			case 3:
				_encr_type = ENCR_RES;
				fprintf(stderr, "Encryption type: reserved\n");
				break;
			default:
				_encr_type = ENCR_NONE;
				fprintf(stderr, "Encryption type: none\n");
			}
		}

		void m17_coder_impl::set_signed(bool signed_str)
		{
			_signed_str = signed_str;
			if (_signed_str == true)
				fprintf(stderr, "Signed stream\n");
			set_type(_mode, _data, _encr_type, _encr_subtype, _can); // update the SIGNED bit in TYPE
		}

		void m17_coder_impl::set_debug(bool debug)
		{
			_debug = debug;
			if (_debug == true)
				fprintf(stderr, "Debug: true\n");
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
				if (_debug)
				{
					fprintf(stderr, "Public key (derived from the private key): ");
					for (i = 0; i < (int)sizeof(pub_key); i++)
						fprintf(stderr, "%02X", pub_key[i]);
					fprintf(stderr, "\n");
				}
			}
			else
				fprintf(stderr, "WARNING: invalid private key - signatures will not verify\n");

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
			fprintf(stderr, "AES key loaded (%d bytes)\n", length);

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

			fprintf(stderr, "Scrambler seed: 0x%0*X (%d-bit)\n", 2 * length, _scrambler_key, 8 * length);
			if (_scrambler_key == 0)
				fprintf(stderr, "WARNING: an all-zero scrambler seed produces no scrambling\n");

			_encr_type = ENCR_SCRAM; // Scrambler key was passed
		}

		void m17_coder_impl::set_eot_cnt(int arg)
		{
			if (arg > 0)
				_eot_cnt = arg;
			else
				_eot_cnt = 1;
		}

		void m17_coder_impl::set_meta(std::string meta) // Text Data (as-is) if encr_subtype==0, otherwise a *UTF-8* encoded byte array
		{
			int length = 0;

			if (_encr_type == ENCR_AES) // with AES, META carries the nonce (set at the start of every transmission)
				return;

			memset(_lsf.meta, 0, sizeof(_lsf.meta));

			fprintf(stderr, "META: ");

			if (!meta.length())
			{
				fprintf(stderr, "0000000000000000000000000000\n");
				uint16_t ccrc = LSF_CRC(&_lsf);
				_lsf.crc[0] = ccrc >> 8;
				_lsf.crc[1] = ccrc & 0xFF;
				return;
			}

			if (_encr_subtype == ENCR_NONE) // Text Data: Control Byte + UTF-8 text, copied as-is
			{
				length = meta.size() < sizeof(_lsf.meta) ? meta.size() : sizeof(_lsf.meta);
				memcpy(_lsf.meta, meta.data(), length);

				for (int i = 0; i < length; i++)
					fprintf(stderr, "%02X ", _lsf.meta[i]);
				fprintf(stderr, "\n");
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

				for (uint_fast8_t i = 0; i < length; i++)
					fprintf(stderr, "%02X ", _lsf.meta[i]);
				fprintf(stderr, "\n");
			}

			fflush(stdout);

			uint16_t ccrc = LSF_CRC(&_lsf);
			_lsf.crc[0] = ccrc >> 8;
			_lsf.crc[1] = ccrc & 0xFF;
		}

		void m17_coder_impl::set_mode(int mode)
		{
			_mode = mode;
			fprintf(stderr, "Mode: %s\n", _mode==M17_TYPE_STREAM ? "stream" : "packet");
			set_type(_mode, _data, _encr_type, _encr_subtype, _can);
		}

		void m17_coder_impl::set_data(int data)
		{
			_data = data;
			fprintf(stderr, "Payload type: %d\n", _data);
			set_type(_mode, _data, _encr_type, _encr_subtype, _can);
		}

		void m17_coder_impl::set_encr_subtype(int encr_subtype)
		{
			_encr_subtype = encr_subtype;
			fprintf(stderr, "Encryption subtype: %d\n", _encr_subtype);
			set_type(_mode, _data, _encr_type, _encr_subtype, _can);
		}

		void m17_coder_impl::set_aes_subtype(int aes_subtype, int encr_type)
		{
			if (encr_type == ENCR_NONE)
				return;

			_aes_subtype = aes_subtype;

			fprintf(stderr, "Using AES");

			if (encr_type == ENCR_AES) // AES ENC, 3200 voice
			{
				_type |= M17_TYPE_ENCR_AES;
				if (_aes_subtype == 0)
				{
					_type |= M17_TYPE_ENCR_AES128;
					fprintf(stderr, "128\n");
				}
				else if (_aes_subtype == 1)
				{
					_type |= M17_TYPE_ENCR_AES192;
					fprintf(stderr, "192\n");
				}
				else if (_aes_subtype == 2)
				{
					_type |= M17_TYPE_ENCR_AES256;
					fprintf(stderr, "256\n");
				}
			}
		}

		void m17_coder_impl::set_can(int can)
		{
			_can = can;
			fprintf(stderr, "CAN: %d\n", _can);
			set_type(_mode, _data, _encr_type, _encr_subtype, _can);
		}

		void m17_coder_impl::set_type(int mode, int data, encr_t encr_type,
									  int encr_subtype, int can)
		{
			short tmptype;
			tmptype =
				mode | (data << 1) | (encr_type << 3) | (encr_subtype << 5) | (can << 7) | (_signed_str ? M17_TYPE_SIGNED : 0);
			_lsf.type[0] = tmptype >> 8;   // MSB
			_lsf.type[1] = tmptype & 0xFF; // LSB
			uint16_t ccrc = LSF_CRC(&_lsf);
			_lsf.crc[0] = ccrc >> 8;
			_lsf.crc[1] = ccrc & 0xFF;
			fprintf(stderr, "Transmission type: 0x%02X%02X\n", _lsf.type[0], _lsf.type[1]);
			fflush(stdout);
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
				if (_active.load(std::memory_order_acquire) && _finished.load(std::memory_order_acquire))
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
			if (_debug == true)
			{
				fprintf(stderr,
						"\nScrambler Key: 0x%06X; Seed: 0x%06X; Subtype: %02d;",
						_scrambler_seed, lfsr, _scrambler_subtype);
				fprintf(stderr, "\n PN: ");
			}

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

			if (_debug == true)
			{
				// debug packed bytes
				for (i = 0; i < 16; i++)
					fprintf(stderr, " %02X", _scr_bytes[i]);
				fprintf(stderr, "\n");
			}
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
			{
				fprintf(stderr, "AES nonce: ");
				for (uint8_t i = 0; i < 14; i++)
					fprintf(stderr, "%02X", _lsf.meta[i]);
				fprintf(stderr, "\n");
			}
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

			///-------packet mode------- TODO: this is only a test!! this needs a proper state machine
			if (_pkt_pend.load(std::memory_order_acquire))
			{
				// emit the whole packet transmission (preamble, LSF, packet frame, EoT) in one go, or wait
				if (noutput_items < (3 + _eot_cnt) * SYM_PER_FRA)
				{
					consume_each(0);
					return 0;
				}

				// packet mode uses its own LSF: the stream LSF (_lsf) is left untouched;
				// in packet mode only the Packet/Stream bit (0 = packet) and CAN are defined in TYPE
				lsf_t pkt_lsf = _lsf;
				uint16_t pkt_type = (uint16_t)(_can & 0xF) << 7;
				pkt_lsf.type[0] = pkt_type >> 8;
				pkt_lsf.type[1] = pkt_type & 0xFF;
				update_LSF_CRC(&pkt_lsf);
				fprintf(stderr, "Packet LSF TYPE: 0x%04X\n", pkt_type);

				int avbl = noutput_items;

				if (avbl >= SYM_PER_FRA)
				{
					gen_preamble(out, &countout, PREAM_LSF);
					avbl -= SYM_PER_FRA;
				}

				if (avbl >= SYM_PER_FRA)
				{
					gen_frame(out + countout, NULL, FRAME_LSF, &pkt_lsf, 0, 0);
					countout += SYM_PER_FRA;
					avbl -= SYM_PER_FRA;
				}

				if (avbl >= SYM_PER_FRA)
				{
					size_t len = _text_len.load(std::memory_order_acquire);
					if (len > 21)
						len = 21;
					uint8_t pkt_pld[26] = {0}; // TODO: TEST ONLY!
					pkt_pld[0] = 0x05;		   // text message
					memcpy(&pkt_pld[1], _text_msg, len);
					uint16_t crc = CRC_M17(pkt_pld, 1 + len + 1);
					pkt_pld[1 + len + 1] = crc >> 8;
					pkt_pld[1 + len + 2] = crc & 0xFF;
					pkt_pld[25] = 0x80 | ((1 + len + 1 + 2)<<2); // TODO: TEST ONLY fixed, 1-payload-frame packet
					gen_frame(out + countout, pkt_pld, FRAME_PKT, &pkt_lsf, 0, 0);
					countout += SYM_PER_FRA;
					avbl -= SYM_PER_FRA;
				}

				for (uint8_t i = 0; i < _eot_cnt; i++)
				{
					if (avbl >= SYM_PER_FRA)
					{
						uint32_t tmp = 0;
						gen_eot(out + countout, &tmp);
						countout += SYM_PER_FRA;
						avbl -= SYM_PER_FRA;
					}
					else
						break;
				}

				_pkt_pend.store(false, std::memory_order_relaxed);

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

				// check the SIGNED STREAM flag
				_signed_str = (_lsf.type[0] >> 3) & 1;
				if (_signed_str && !_priv_key_loaded)
					fprintf(stderr, "WARNING: signed stream without a private key - the signature will not verify\n");

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

				// update LSF every 6 frames (superframe boundary)
				if (_fn > 0 && _lich_cnt == 0)
				{
					// TODO: fix the _next_lsf contents before uncommenting lines below
					//_lsf = _next_lsf;
					// update_LSF_CRC(&_lsf);
				}
			}

			// end of stream: last frame, signature (if signed), EoT(s) - all in one go
			if (finished)
			{
				if (!_finalizing)
				{
					fprintf(stderr, "Sending last frame(s) plus EoT(s)\n");
					_finalizing = true; // print only once
				}

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
					{
						fprintf(stderr, "Signature: ");
						for (uint8_t i = 0; i < sizeof(_sig); i++)
						{
							if (i == 16 || i == 32 || i == 48)
								fprintf(stderr, "\n           ");
							fprintf(stderr, "%02X", _sig[i]);
						}

						fprintf(stderr, "\n");
					}
				}

				// send EOT frame(s)
				for (uint8_t i = 0; i < _eot_cnt; i++)
				{
					uint32_t tmp = 0;
					gen_eot(out + countout, &tmp);
					countout += tmp; // tmp should equal SYM_PER_FRA (192)
				}

				fprintf(stderr, "Stopping symbol generation\n");
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
