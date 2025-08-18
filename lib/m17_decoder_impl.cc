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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "m17_decoder_impl.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#include "m17.h"

namespace gr
{
	namespace m17
	{

		m17_decoder::sptr
		m17_decoder::make(bool debug_data, bool debug_ctrl, float threshold,
						  bool callsign, bool signed_str, int encr_type,
						  std::string key, std::string seed)
		{
			return gnuradio::get_initial_sptr(new m17_decoder_impl(debug_data, debug_ctrl, threshold, callsign,
																   signed_str, encr_type, key, seed));
		}

		/*
		 * The private constructor
		 */
		m17_decoder_impl::m17_decoder_impl(bool debug_data, bool debug_ctrl,
										   float threshold, bool callsign,
										   bool signed_str, int encr_type,
										   std::string key, std::string seed) : gr::block("m17_decoder",
																						  gr::io_signature::make(1, 1, sizeof(float)),
																						  gr::io_signature::make(1, 1, sizeof(char))),
																				_debug_data(debug_data), _debug_ctrl(debug_ctrl),
																				_threshold(threshold), _callsign(callsign), _signed_str(signed_str)
		{
			set_debug_data(debug_data);
			set_debug_ctrl(debug_ctrl);
			set_threshold(threshold);
			set_callsign(callsign);
			set_signed(signed_str);
			set_key(key);
			set_encr_type(encr_type);
			_expected_next_fn = 0;

			message_port_register_out(pmt::mp("fields"));
		}

		/*
		 * Our virtual destructor.
		 */
		m17_decoder_impl::~m17_decoder_impl()
		{
		}

		void m17_decoder_impl::set_threshold(float threshold)
		{
			_threshold = threshold;
			printf("Threshold: %f\n", _threshold);
		}

		void m17_decoder_impl::set_debug_data(bool debug)
		{
			_debug_data = debug;
			if (_debug_data == true)
				printf("Data debug: true\n");
			else
				printf("Data debug: false\n");
		}

		void m17_decoder_impl::set_debug_ctrl(bool debug)
		{
			_debug_ctrl = debug;
			if (_debug_ctrl == true)
				printf("Debug control: true\n");
			else
				printf("Debug control: false\n");
		}

		void m17_decoder_impl::set_encr_type(int encr_type)
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
			printf("new encr type: %x -> ", _encr_type);
		}

		void m17_decoder_impl::set_callsign(bool callsign)
		{
			_callsign = callsign;
			if (_callsign == true)
				printf("Display callsign\n");
			else
				printf("Do not display callsign\n");
		}

		void m17_decoder_impl::set_signed(bool signed_str)
		{
			_signed_str = signed_str;
			if (_callsign == true)
				printf("Signed\n");
			else
				printf("Unsigned\n");
		}

		void m17_decoder_impl::set_key(std::string arg) // *UTF-8* encoded byte array
		{
			int length;
			printf("new key: ");
			length = arg.size();
			int i = 0, j = 0;
			while ((j < 32) && (i < length))
			{
				if ((unsigned int)arg.data()[i] < 0xc2) // https://www.utf8-chartable.de/
				{
					_key[j] = arg.data()[i];
					i++;
					j++;
				}
				else
				{
					_key[j] = (arg.data()[i] - 0xc2) * 0x40 + arg.data()[i + 1];
					i += 2;
					j++;
				}
			}
			length = j; // index from 0 to length-1
			printf("%d bytes: ", length);
			for (i = 0; i < length; i++)
				printf("%02X ", _key[i]);
			printf("\n");
			fflush(stdout);
		}

		void m17_decoder_impl::set_seed(std::string arg) // *UTF-8* encoded byte array
		{
			int length;
			printf("new seed: ");
			length = arg.size();
			int i = 0, j = 0;
			while ((j < 3) && (i < length))
			{
				if ((unsigned int)arg.data()[i] < 0xc2) // https://www.utf8-chartable.de/
				{
					_seed[j] = arg.data()[i];
					i++;
					j++;
				}
				else
				{
					_seed[j] = (arg.data()[i] - 0xc2) * 0x40 + arg.data()[i + 1];
					i += 2;
					j++;
				}
			}
			length = j; // index from 0 to length-1
			printf("%d bytes: ", length);
			for (i = 0; i < length; i++)
				printf("%02X ", _seed[i]);
			printf("\n");
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

		void
		m17_decoder_impl::forecast(int noutput_items,
								   gr_vector_int &ninput_items_required)
		{
			ninput_items_required[0] = 1; // do work only if there is at least one symbol available
		}

		// this is generating a correct seed value based on the fn value,
		// ideally, we would only want to run this under poor signal, frame skips, etc
		// Note: Running this every frame will lag if high fn values (observed with test file)
		uint32_t m17_decoder_impl::scrambler_seed_calculation(int8_t subtype,
															  uint32_t key,
															  int fn)
		{
			int i;
			uint32_t lfsr, bit;

			lfsr = key;
			bit = 0;
			for (i = 0; i < 128 * fn; i++)
			{
				// get feedback bit with specified taps, depending on the subtype
				if (subtype == 0)
					bit = (lfsr >> 7) ^ (lfsr >> 5) ^ (lfsr >> 4) ^ (lfsr >> 3);
				else if (subtype == 1)
					bit = (lfsr >> 15) ^ (lfsr >> 14) ^ (lfsr >> 12) ^ (lfsr >> 3);
				else if (subtype == 2)
					bit = (lfsr >> 23) ^ (lfsr >> 22) ^ (lfsr >> 21) ^ (lfsr >> 16);
				else
					bit = 0; // should never get here, but just in case

				bit &= 1;				  // truncate bit to 1 bit
				lfsr = (lfsr << 1) | bit; // shift LFSR left once and OR bit onto LFSR's LSB
				lfsr &= 0xFFFFFF;		  // truncate lfsr to 24-bit
			}

			// truncate seed so subtype will continue to set properly on subsequent passes
			if (_scrambler_subtype == 0)
				_scrambler_seed &= 0xFF;
			else if (_scrambler_subtype == 1)
				_scrambler_seed &= 0xFFFF;
			else if (_scrambler_subtype == 2)
				_scrambler_seed &= 0xFFFFFF;

			// debug
			// fprintf (stderr, "\nScrambler Key: 0x%06X; Seed: 0x%06X; Subtype: %02d; FN: %05d; ", key, lfsr, subtype, fn);

			return lfsr;
		}

		// scrambler pn sequence generation
		void m17_decoder_impl::scrambler_sequence_generator()
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
			if (_debug_ctrl == true)
			{
				fprintf(stderr,
						"\nScrambler Key: 0x%06X; Seed: 0x%06X; Subtype: %02d;",
						_scrambler_seed, lfsr, _scrambler_subtype);
				fprintf(stderr, "\n pN: ");
			}

			// run pN sequence with taps specified
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

			if (_debug_ctrl == true)
			{
				// debug packed bytes
				for (i = 0; i < 16; i++)
					fprintf(stderr, " %02X", _scr_bytes[i]);
				fprintf(stderr, "\n");
			}
		}

		// convert a user string (as hex octets) into a uint8_t array for key
		void m17_decoder_impl::parse_raw_key_string(uint8_t *dest,
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
		m17_decoder_impl::general_work(int noutput_items,
									   gr_vector_int &ninput_items,
									   gr_vector_const_void_star &input_items,
									   gr_vector_void_star &output_items)
		{
			const float *in = (const float *)input_items[0];
			char *out = (char *)output_items[0];
			int countout = 0;

			float sample; // last raw sample from the stdin
			float dist;	  // Euclidean distance for finding syncwords in the symbol stream

			for (int counterin = 0; counterin < ninput_items[0]; counterin++)
			{
				// wait for another symbol
				sample = in[counterin];

				if (!syncd)
				{
					// push new symbol
					for (uint8_t i = 0; i < 7; i++)
					{
						last[i] = last[i + 1];
					}

					last[7] = sample;

					// calculate euclidean norm
					dist = eucl_norm(last, str_sync_symbols, 8);

					if (dist < _threshold) // frame syncword detected
					{
						// fprintf(stderr, "str_sync_symbols dist: %3.5f\n", dist);
						syncd = 1;
						pushed = 0;
						fl = 0;
					}
					else
					{
						// calculate euclidean norm again, this time against LSF syncword
						dist = eucl_norm(last, lsf_sync_symbols, 8);

						if (dist < _threshold) // LSF syncword
						{
							// fprintf(stderr, "lsf_sync dist: %3.5f\n", dist);
							syncd = 1;
							pushed = 0;
							fl = 1;
						}
					}
				}
				else
				{
					_pld[pushed++] = sample;

					if (pushed == SYM_PER_PLD)
					{
						// if it is a frame
						if (!fl)
						{
							// decode
							uint32_t e = decode_str_frame(_frame_data, _lich_b, &_fn, &_lich_cnt, _pld);

							uint16_t type = ((uint16_t)_lsf.type[0] << 8) + _lsf.type[1];
							_signed_str = (type >> 11) & 1;

							/// if the stream is signed (process before decryption)
							if (_signed_str && _fn < 0x7FFC)
							{
								if (_fn == 0)
									memset(_digest, 0, sizeof(_digest));

								for (uint8_t i = 0; i < sizeof(_digest); i++)
									_digest[i] ^= _frame_data[i];
								uint8_t tmp = _digest[0];
								for (uint8_t i = 0; i < sizeof(_digest) - 1; i++)
									_digest[i] = _digest[i + 1];
								_digest[sizeof(_digest) - 1] = tmp;
							}

							// NOTE: Don't attempt decryption when a signed stream is >= 0x7FFC
							// The Signature is not encrypted

							// AES
							if (_encr_type == ENCR_AES)
							{
								memcpy(_iv, _lsf.meta, 14);
								_iv[14] = (_fn >> 8) & 0x7F; // TODO: check if this is the right byte order
								_iv[15] = (_fn & 0xFF) & 0xFF;

								if (_signed_str && (_fn % 0x8000) < 0x7FFC) // signed stream
									aes_ctr_bytewise_payload_crypt(_iv, _key, _frame_data, _aes_subtype);
								else if (!_signed_str) // non-signed stream
									aes_ctr_bytewise_payload_crypt(_iv, _key, _frame_data, _aes_subtype);
							}

							// Scrambler
							if (_encr_type == ENCR_SCRAM)
							{
								if (_fn != 0 && (_fn % 0x8000) != _expected_next_fn) // frame skip, etc
									_scrambler_seed = scrambler_seed_calculation(_scrambler_subtype, _scrambler_key, _fn & 0x7FFF);
								else if (_fn == 0)
									_scrambler_seed = _scrambler_key; // reset back to key value

								if (_signed_str && (_fn % 0x8000) < 0x7FFC) // signed stream
									scrambler_sequence_generator();
								else if (!_signed_str) // non-signed stream
									scrambler_sequence_generator();
								else
									memset(_scr_bytes, 0, sizeof(_scr_bytes)); // zero out stale scrambler bytes so they aren't applied to the sig frames

								for (uint8_t i = 0; i < 16; i++)
								{
									_frame_data[i] ^= _scr_bytes[i];
								}
							}

							if (_debug_data == true)
							{ // dump data - first byte is empty
								printf("RX FN: %04X PLD: ", _fn);
							}

							for (uint8_t i = 0; i < 16; i++)
							{
								if (_debug_data == true)
								{
									printf("%02X", _frame_data[i]);
								}
								out[countout] = _frame_data[i];
								countout++;
							}
							if (_debug_data == true)
							{
								printf(" e=%1.1f\n", (float)e / 0xFFFF);
							}
							// send codec2 stream to stdout
							// fwrite(&_frame_data[3], 16, 1, stdout);

							// If we're at the start of a superframe, or we missed a frame, reset the LICH state
							if ((_lich_cnt == 0) || ((_fn % 0x8000) != _expected_next_fn && _fn < 0x7FFC))
								lich_chunks_rcvd = 0;

							lich_chunks_rcvd |= (1 << _lich_cnt);
							memcpy((uint8_t *)&_lsf + _lich_cnt * 5, _lich_b, 5);

							// debug - dump LICH
							if (lich_chunks_rcvd == 0x3F) // all 6 chunks received?
							{
								// handle message output
								pmt::pmt_t msg;
								decode_callsign_bytes(d_dst, _lsf.dst);
								decode_callsign_bytes(d_src, _lsf.src);

								pmt::pmt_t dict = pmt::make_dict();
								dict = pmt::dict_add(dict, pmt::mp("src"), pmt::intern((char *)d_src));
								dict = pmt::dict_add(dict, pmt::mp("dst"), pmt::intern((char *)d_dst));

								msg = pmt::init_u8vector(2, _lsf.type);
								dict = pmt::dict_add(dict, pmt::mp("type"), msg);
								msg = pmt::init_u8vector(14, _lsf.meta);
								dict = pmt::dict_add(dict, pmt::mp("meta"), msg);

								message_port_pub(pmt::mp("fields"), dict);

								// debug data display
								if (_callsign == true)
								{
									if (_debug_ctrl == true)
									{
										printf("DST: %-9s ", d_dst); // DST
										printf("SRC: %-9s ", d_src); // SRC
									}
								}
								else if (_debug_ctrl == true)
								{
									printf("DST: "); // DST
									for (uint8_t i = 0; i < 6; i++)
										printf("%02X", ((uint8_t *)_lsf.dst)[i]);
									printf(" ");
									printf("SRC: "); // SRC
									for (uint8_t i = 0; i < 6; i++)
										printf("%02X", ((uint8_t *)_lsf.src)[i]);
									printf(" ");
								}

								// TYPE
								if (_debug_ctrl == true)
								{
									printf("TYPE: %04X (", type);
									if (type && 1)
										printf("STREAM: ");
									else
										printf("PACKET: "); // shouldn't happen
									if (((type >> 1) & 3) == 1)
										printf("DATA, ");
									else if (((type >> 1) & 3) == 2)
										printf("VOICE, ");
									else if (((type >> 1) & 3) == 3)
										printf("VOICE+DATA, ");
									printf("ENCR: ");
									if (((type >> 3) & 3) == 0)
										printf("PLAIN, ");
									else if (((type >> 3) & 3) == 1)
									{
										printf("SCRAM ");
										if (((type >> 5) & 3) == 1)
											printf("8-bit, ");
										else if (((type >> 5) & 3) == 2)
											printf("16-bit, ");
										else if (((type >> 5) & 3) == 3)
											printf("24-bit, ");
									}
									else if (((type >> 3) & 3) == 2)
										printf("AES, ");
									else
										printf("UNK, ");
									printf("CAN: %d", (type >> 7) & 0xF);
									if ((type >> 11) & 1)
										printf(", SIGNED");
									printf(") ");
								}

								// META
								if (_debug_ctrl == true)
								{
									printf("META: ");
									for (uint8_t i = 0; i < 14; i++)
										printf("%02X", ((uint8_t *)_lsf.meta)[i]);

									if (CRC_M17((uint8_t *)&_lsf, sizeof(_lsf))) // CRC
										printf(" LSF_CRC_ERR");
									else
										printf(" LSF_CRC_OK ");
									printf("\n");
								}
							}

							// if the contents of the payload is now digital signature, not data/voice
							if (_fn >= 0x7FFC && _signed_str == true)
							{
								memcpy(&_sig[((_fn & 0x7FFF) - 0x7FFC) * 16], _frame_data, 16);

								if (_fn == (0x7FFF | 0x8000))
								{
									// dump data
									/*printf("DEC-Digest: ");
									   for(uint8_t i=0; i<sizeof(digest); i++)
									   printf("%02X", digest[i]);
									   printf("\n");

									   printf("Key: ");
									   for(uint8_t i=0; i<sizeof(pub_key); i++)
									   printf("%02X", pub_key[i]);
									   printf("\n");

									   printf("Signature: ");
									   for(uint8_t i=0; i<sizeof(sig); i++)
									   printf("%02X", sig[i]);
									   printf("\n"); */

									if (uECC_verify(_key, _digest, sizeof(_digest), _sig, _curve))
									{
										if (_debug_ctrl == true)
											printf("Signature OK\n");
									}
									else
									{
										if (_debug_ctrl == true)
											printf("Signature invalid\n");
									}
								}
							}

							_expected_next_fn = (_fn + 1) % 0x8000;
						}
						else // lsf
						{
							if (_debug_ctrl == true)
							{
								printf("{LSF} ");
							}
							// decode
							uint32_t e = decode_LSF(&_lsf, _pld);

							// dump data
							if (_callsign == true)
							{
								decode_callsign_bytes(d_dst, _lsf.dst);
								decode_callsign_bytes(d_src, _lsf.src);
								if (_debug_ctrl == true)
								{
									printf("DST: %-9s ", d_dst); // DST
									printf("SRC: %-9s ", d_src); // SRC
								}
							}
							else
							{
								if (_debug_ctrl == true)
								{
									printf("DST: "); // DST
									for (uint8_t i = 0; i < 6; i++)
										printf("%02X", ((uint8_t *)_lsf.dst)[i]);
									printf(" ");

									// SRC
									printf("SRC: ");
									for (uint8_t i = 0; i < 6; i++)
										printf("%02X", ((uint8_t *)_lsf.src)[i]);
									printf(" ");
								}
							}
							// TYPE
							uint16_t type = ((uint16_t)_lsf.type[0] << 8) + _lsf.type[1];
							if (_debug_ctrl == true)
							{
								printf("TYPE: %04X (", type);
								if (type && 1)
									printf("STREAM: ");
								else
									printf("PACKET: "); // shouldn't happen
								if (((type >> 1) & 3) == 1)
									printf("DATA, ");
								else if (((type >> 1) & 3) == 2)
									printf("VOICE, ");
								else if (((type >> 1) & 3) == 3)
									printf("VOICE+DATA, ");
								printf("ENCR: ");
								if (((type >> 3) & 3) == 0)
									printf("PLAIN, ");
								else if (((type >> 3) & 3) == 1)
								{
									printf("SCRAM ");
									if (((type >> 5) & 3) == 0)
										printf("8-bit, ");
									else if (((type >> 5) & 3) == 1)
										printf("16-bit, ");
									else if (((type >> 5) & 3) == 2)
										printf("24-bit, ");
								}
								else if (((type >> 3) & 3) == 2)
								{
									printf("AES");
									if (((type >> 5) & 3) == 0)
										printf("128");
									else if (((type >> 5) & 3) == 1)
										printf("192");
									else if (((type >> 5) & 3) == 2)
										printf("256");

									printf(", ");
								}
								else
									printf("UNK, ");
								printf("CAN: %d", (type >> 7) & 0xF);
								if ((type >> 11) & 1)
								{
									printf(", SIGNED");
									_signed_str = 1;
								}
								else
									_signed_str = 0;
								printf(") ");

								// META
								printf("META: ");
								for (uint8_t i = 0; i < 14; i++)
									printf("%02X", ((uint8_t *)_lsf.meta)[i]);
								printf(" ");
								// CRC
								// printf("CRC: ");
								// for(uint8_t i=0; i<2; i++)
								// printf("%02X", lsf[28+i]);
								if (CRC_M17((uint8_t *)&_lsf, 30))
									printf("LSF_CRC_ERR");
								else
									printf("LSF_CRC_OK ");
								// Viterbi decoder errors
								printf(" e=%1.1f\n", (float)e / 0xFFFF);
							}
						}
						// job done
						syncd = 0;
						pushed = 0;

						for (uint8_t i = 0; i < 8; i++)
							last[i] = 0.0;
					}
				}
			}
			// Tell runtime system how many input items we consumed on
			// each input stream.
			consume_each(ninput_items[0]);

			// Tell runtime system how many output items we produced.
			return countout;
		}

	} /* namespace m17 */
} /* namespace gr */
