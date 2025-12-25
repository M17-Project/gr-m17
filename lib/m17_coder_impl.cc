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
#include <unistd.h>

#include "m17.h"
#include "aes.h"
#include "uECC.h"

namespace gr
{
	namespace m17
	{

		m17_coder::sptr
		m17_coder::make(std::string src_id, std::string dst_id, int mode,
						int data, int encr_type, int encr_subtype, int aes_subtype, int can,
						std::string meta, std::string key,
						std::string priv_key, bool debug, bool signed_str, std::string seed, int eot_cnt)
		{
			return gnuradio::get_initial_sptr(new m17_coder_impl(src_id, dst_id, mode, data, encr_type, encr_subtype,
																 aes_subtype, can, meta, key, priv_key, debug, signed_str, seed, eot_cnt));
		}

		/*
		 * The private constructor
		 */
		m17_coder_impl::m17_coder_impl(std::string src_id, std::string dst_id,
									   int mode, int data, int encr_type,
									   int encr_subtype, int aes_subtype, int can,
									   std::string meta, std::string key,
									   std::string priv_key, bool debug,
									   bool signed_str, std::string seed,
									   int eot_cnt) : gr::block("m17_coder", gr::io_signature::make(1, 1, sizeof(char)),
																gr::io_signature::make(1, 1, sizeof(float))),
													  _mode(mode), _data(data), _encr_subtype(encr_subtype), _aes_subtype(aes_subtype), _can(can), _meta(meta), _debug(debug),
													  _signed_str(signed_str), _eot_cnt(eot_cnt)
		{
			set_encr_type(encr_type); // overwritten by set_seed()
			set_type(mode, data, _encr_type, encr_subtype, can);
			set_aes_subtype(aes_subtype, encr_type);
			set_meta(meta); // depends on   ^^^ encr_subtype
			set_seed(seed); // depends on   ^^^ encr_subtype
			set_eot_cnt(eot_cnt);
			set_src_id(src_id);
			set_dst_id(dst_id);
			set_signed(signed_str);
			set_debug(debug);
			set_output_multiple(SYM_PER_FRA);

			if (_encr_type == ENCR_AES)
			{
				for (uint8_t i = 0; i < 4; i++)
					_iv[i] = ((uint32_t)(time(NULL) & 0xFFFFFFFF) - (uint32_t)epoch) >> (24 - (i * 8));
				for (uint8_t i = 3; i < 14; i++)
					_iv[i] = rand() & 0xFF; // 10 random bytes
			}

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
				encode_callsign_bytes(_lsf.dst, (const unsigned char *)"@ALL");

				// source set to "N0CALL"
				encode_callsign_bytes(_lsf.src, (const unsigned char *)"N0CALL");

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
				_active.store(true, std::memory_order_release);
				_finished.store(false, std::memory_order_relaxed);
				set_mode(M17_TYPE_STREAM);
				fprintf(stderr, "[%02d:%02d:%02d] Start of Stream transmission\n", t.tm_hour, t.tm_min, t.tm_sec);
				return;
			}

			if (cmd == "EOT")
			{
				_finished.store(true, std::memory_order_release);
				fprintf(stderr, "[%02d:%02d:%02d] End of Stream transmission\n", t.tm_hour, t.tm_min, t.tm_sec);
				return;
			}

			if (cmd == "SMS")
			{
				if (_pkt_pend.load(std::memory_order_acquire))
				{
					fprintf(stderr, "[%02d:%02d:%02d] SMS ignored (last transmission pending)\n", t.tm_hour, t.tm_min, t.tm_sec);
					return;
				}

				if (val.size())
				{
					fprintf(stderr, "[%02d:%02d:%02d] Start of text message transmission:\n%s\n", t.tm_hour, t.tm_min, t.tm_sec, val.c_str());
					set_mode(M17_TYPE_PACKET);
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

			_priv_key_loaded = true;

			fprintf(stderr, "Private key ");

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

			fprintf(stderr, "(%d bytes): ", length);
			for (i = 0; i < length; i++)
				fprintf(stderr, "%02X ", _priv_key[i]);
			fprintf(stderr, "\n");

			fflush(stdout);
		}

		void m17_coder_impl::set_key(std::string arg) // *UTF-8* encoded byte array
		{
			int length = arg.size();

			fprintf(stderr, "Encryption key ");

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

			fprintf(stderr, "(%d bytes): ", length);
			for (i = 0; i < length; i++)
				fprintf(stderr, "%02X ", _key[i]);
			fprintf(stderr, "\n");

			fflush(stdout);
		}

		void m17_coder_impl::set_seed(std::string arg) // *UTF-8* encoded byte array
		{
			int length = arg.size();

			if (!length)
				return;

			fprintf(stderr, "Scrambler seed ");

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

			fprintf(stderr, "(%d bytes): ", length);
			for (i = 0; i < length; i++)
				fprintf(stderr, "%02X ", _seed[i]);
			fprintf(stderr, "\n");

			fflush(stdout);

			if (length <= 2)
			{
				_scrambler_seed = _scrambler_seed >> 16;
				fprintf(stderr, "Scrambler key: 0x%02X (8-bit)\n", _scrambler_seed);
			}
			else if (length <= 4)
			{
				_scrambler_seed = _scrambler_seed >> 8;
				fprintf(stderr, "Scrambler key: 0x%04X (16-bit)\n", _scrambler_seed);
			}
			else
				fprintf(stderr, "Scrambler key: 0x%06X (24-bit)\n", _scrambler_seed);

			_encr_type = ENCR_SCRAM; // Scrambler key was passed
		}

		void m17_coder_impl::set_eot_cnt(int arg)
		{
			if (arg > 0)
				_eot_cnt = arg;
			else
				_eot_cnt = 1;
		}

		void m17_coder_impl::set_meta(std::string meta) // either an ASCII string if encr_subtype==0 or *UTF-8* encoded byte array. TODO: rework this function
		{
			int length = 0;

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

			if (_encr_subtype == ENCR_NONE) // meta is \0-terminated string
			{
				if (meta.length() < 14)
					length = meta.length();
				else
				{
					length = 14;
					meta[13] = 0; // null-terminate
				}

				if (length)
				{
					fprintf(stderr, "\"%s\"\n", meta.c_str());
					memcpy(_lsf.meta, meta.c_str(), length); // I hope this is fine
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

				for (uint_fast8_t i = 0; i < length; i++)
					fprintf(stderr, "%02X ", _lsf.meta[i]);
				fprintf(stderr, "\n");
			}

			fflush(stdout);

			uint16_t ccrc = LSF_CRC(&_lsf);
			_lsf.crc[0] = ccrc >> 8;
			_lsf.crc[1] = ccrc & 0xFF;
		}

		void m17_coder_impl::set_mode(int mode) // TODO: the packet/stream selector is not needed
		{
			_mode = mode;
			fprintf(stderr, "Mode: %d\n", _mode);
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
				mode | (data << 1) | (encr_type << 3) | (encr_subtype << 5) | (can << 7);
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
				// stream mode
				ninput_items_required[0] = noutput_items / 12; // 16 in -> 192 out
			}
		}

		// scrambler PN sequence generation
		void m17_coder_impl::scrambler_sequence_generator()
		{
			int i = 0;
			uint32_t lfsr, bit;
			lfsr = _scrambler_seed;

			// only set if not initially set (first run), it is possible (and observed) that the scrambler_subtype can
			// change on subsequent passes if the current SEED for the LFSR falls below one of these thresholds
			if (_scrambler_subtype == -1)
			{
				if (lfsr > 0 && lfsr <= 0xFF)
					_scrambler_subtype = 0; // 8-bit key
				else if (lfsr > 0xFF && lfsr <= 0xFFFF)
					_scrambler_subtype = 1; // 16-bit key
				else if (lfsr > 0xFFFF && lfsr <= 0xFFFFFF)
					_scrambler_subtype = 2; // 24-bit key
				else
					_scrambler_subtype = 0; // 8-bit key (default)
			}

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
				// fprintf(stderr, "[DBG] noutput_items=%d\n", noutput_items);
				int avbl = noutput_items;

				if (avbl >= SYM_PER_FRA)
				{
					gen_preamble(out, &countout, PREAM_LSF);
					avbl -= SYM_PER_FRA;
				}

				if (avbl >= SYM_PER_FRA)
				{
					gen_frame(out + countout, NULL, FRAME_LSF, &_lsf, 0, 0);
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
					gen_frame(out + countout, pkt_pld, FRAME_PKT, &_lsf, 0, 0);
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
			if (_finalizing)
			{
				consume_each(0);
			}

			// drop any stale input if we just transitioned to active
			if (_active.load(std::memory_order_acquire) && !_got_lsf && ninput_items[0] > 0)
			{
				// first work call after SOT, flush old data
				consume_each(ninput_items[0]);
			}

			if (_active.load(std::memory_order_acquire))
			{
				if (_send_preamble == true)
				{
					gen_preamble(out, &countout, PREAM_LSF); // 0 - LSF preamble, as opposed to 1 - BERT preamble
					_send_preamble = false;
				}

				while (countout < (uint32_t)noutput_items)
				{
					uint8_t data[PAYLOAD_BYTES]; // raw payload, packed bits

					if (!_finalizing)
					{
						if (ninput_items[0] < countin + PAYLOAD_BYTES) // not enough input
						{
							if (_finished.load(std::memory_order_acquire) == false)
							{
								break;
							}
						}

						if (!_got_lsf) // stream frames
						{
							// send LSF
							gen_frame(out + countout, NULL, FRAME_LSF, &_lsf, 0, 0);
							countout += SYM_PER_FRA; // gen frame always writes SYM_PER_FRA symbols = 192

							// check the SIGNED STREAM flag
							_signed_str = (_lsf.type[0] >> 3) & 1;

							// set the flag
							_got_lsf = 1;
						}

						if (ninput_items[0] >= countin + PAYLOAD_BYTES)
						{
							// get new data
							memcpy(data, in + countin, PAYLOAD_BYTES);
							countin += PAYLOAD_BYTES;

							if (countin > PAYLOAD_BYTES)
								continue;
							// else
							// printf("[DBG] Consumed 16 bytes FN=%u, total countin=%d\n", _fn, countin);
						}

						// TODO if debug_mode==1 from lines 520 to 570
						// TODO add aes_subtype as user argument

						if (_encr_type == ENCR_AES)
						{
							memcpy(&(_next_lsf.meta), _iv, 14); // TODO: I suspect that this does not work
							_iv[14] = (_fn >> 8) & 0x7F;
							_iv[15] = (_fn >> 0) & 0xFF;
							aes_ctr_bytewise_payload_crypt(_iv, _key, data, _aes_subtype);
						}
						else
							// Scrambler
							if (_encr_type == ENCR_SCRAM)
							{
								scrambler_sequence_generator();
								for (uint8_t i = 0; i < PAYLOAD_BYTES; i++)
								{
									data[i] ^= _scr_bytes[i];
								}
							}

						/*fprintf(stderr, "Payload FN=%u: ", _fn);
						for (int i = 0; i < PAYLOAD_BYTES; i++)
						  fprintf(stderr, "%02X ", data[i]);
						fprintf(stderr, "\n");*/
					}

					if (_finished.load(std::memory_order_acquire) == false)
					{
						gen_frame(out + countout, data, FRAME_STR, &_lsf, _lich_cnt, _fn);
						countout += SYM_PER_FRA;		 // gen frame always writes SYM_PER_FRA symbols = 192
						_fn = (_fn + 1) % 0x8000;		 // increment FN
						_lich_cnt = (_lich_cnt + 1) % 6; // continue with next LICH_CNT

						// update the stream digest if required
						if (_signed_str)
						{
							for (uint8_t i = 0; i < sizeof(_digest); i++)
								_digest[i] ^= data[i];
							uint8_t tmp = _digest[0];
							for (uint8_t i = 0; i < sizeof(_digest) - 1; i++)
								_digest[i] = _digest[i + 1];
							_digest[sizeof(_digest) - 1] = tmp;
						}

						// update LSF every 6 frames (superframe boundary)
						if (_fn > 0 && _lich_cnt == 0)
						{
							// TODO: fix the _next_lsf contents before uncommenting lines below
							//_lsf = _next_lsf;
							// update_LSF_CRC(&_lsf);
						}
					}
					else // send last frame(s)
					{
						// prevent further input consumption
						countin = 0;

						// enter finalization only once
						if (!_finalizing)
						{
							fprintf(stderr, "Sending last frame(s) plus EoT(s)\n");
							_finalizing = true; // mark that we already printed and started finishing
						}

						/* Determine how many frames we will emit in total:
						   - one final data frame
						   - if signed stream: 4 signature frames
						   - one (or more) EOT frames generated by gen_eot()
						*/
						int frames_needed = 1 + (_signed_str ? 4 : 0) + _eot_cnt;
						int samples_needed = frames_needed * SYM_PER_FRA;

						if ((noutput_items - (int)countout) < samples_needed)
						{
							// Not enough room to emit the entire remaining sequence.
							// Wait for next general_work() with a larger buffer.
							consume_each(0); // wake scheduler to retry
							return countout;
						}

						// prevent re-entry before generating EOT
						_active.store(false, std::memory_order_release);

						if (!_signed_str)
							_fn |= 0x8000;
						gen_frame(out + countout, data, FRAME_STR, &_lsf, _lich_cnt, _fn);
						countout += SYM_PER_FRA;		 // gen frame always writes SYM_PER_FRA symbols = 192
						_lich_cnt = (_lich_cnt + 1) % 6; // continue with next LICH_CNT

						// if we are done, and the stream is signed, so we need to transmit the signature (4 frames)
						if (_signed_str)
						{
							// update digest
							for (uint8_t i = 0; i < sizeof(_digest); i++)
								_digest[i] ^= data[i];
							uint8_t tmp = _digest[0];
							for (uint8_t i = 0; i < sizeof(_digest) - 1; i++)
								_digest[i] = _digest[i + 1];
							_digest[sizeof(_digest) - 1] = tmp;

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
						consume_each(countin);
						init_state();
						_finalizing = false;
						return countout;
					} // finished == true
				} // loop on input data

				// Tell runtime system how many input items we consumed on
				// each input stream.
				consume_each(countin);
			}
			else
			{
				usleep(10e3);				   // TODO: fix this
				consume_each(ninput_items[0]); // consume input at idle to prevent buffer from filling with a lot of data
				return 0;
			}

			return countout;

			// https://lists.gnu.org/archive/html/discuss-gnuradio/2016-12/msg00206.html
			// returning -1 (which is the magical value for "there's nothing coming anymore, you can shut down") would normally end a flow graph
		}
	} /* namespace m17 */
} /* namespace gr */
