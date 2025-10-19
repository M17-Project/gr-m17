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
			
			// SECURITY FIX: Initialize SHA-256 context
			_sha_ctx = nullptr;

			message_port_register_out(pmt::mp("fields"));
		}

		/*
		 * Our virtual destructor.
		 * SECURITY FIX: Destructor implementation moved to header file
		 * to avoid duplicate definition errors
		 */

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

		void m17_decoder_impl::set_key(std::string arg) // Hex-encoded encryption key
		{
                // CRITICAL SECURITY FIX: Use proper hex parsing instead of UTF-8
                // NEVER log encryption keys, not even in debug mode
                // SECURITY: Removed key logging - keys must never be printed to console
			
			// SECURITY FIX: Parse hex string instead of UTF-8
			// Expect 64 hex characters for 32-byte key
			if (arg.size() != 64) {
				fprintf(stderr, "ERROR: Decoder encryption key must be 64 hex characters (32 bytes)\n");
				return;
			}
			
			// Parse hex string to binary
			for (int i = 0; i < 32; i++) {
				char hex_byte[3] = {arg[i*2], arg[i*2+1], '\0'};
				char *endptr;
				unsigned long val = strtoul(hex_byte, &endptr, 16);
				if (*endptr != '\0' || val > 255) {
					fprintf(stderr, "ERROR: Invalid hex character in decoder encryption key\n");
					return;
				}
				_key[i] = (uint8_t)val;
			}
		}

		void m17_decoder_impl::set_seed(std::string arg) // Hex-encoded seed
		{
                // CRITICAL SECURITY FIX: Use proper hex parsing instead of UTF-8
                // NEVER log seed material, not even in debug mode
                // SECURITY: Removed seed logging - sensitive material must never be printed
			
			// SECURITY FIX: Parse hex string instead of UTF-8
			// Expect 6 hex characters for 3-byte seed
			if (arg.size() != 6) {
				fprintf(stderr, "ERROR: Decoder seed must be 6 hex characters (3 bytes)\n");
				return;
			}
			
			// Parse hex string to binary
			for (int i = 0; i < 3; i++) {
				char hex_byte[3] = {arg[i*2], arg[i*2+1], '\0'};
				char *endptr;
				unsigned long val = strtoul(hex_byte, &endptr, 16);
				if (*endptr != '\0' || val > 255) {
					fprintf(stderr, "ERROR: Invalid hex character in decoder seed\n");
					return;
				}
				_seed[i] = (uint8_t)val;
			}
			
			// SECURITY FIX: Calculate scrambler seed from parsed seed
			_scrambler_seed = (_seed[0] << 16) | (_seed[1] << 8) | _seed[2];
			
                // SECURITY FIX: Don't log scrambler key material
                // SECURITY: Removed seed logging - sensitive material must never be printed

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

			// Set Frame Type based on scrambler_subtype value
			if (_scrambler_subtype == 0) {
				_frame_type = 0; // Standard M17 frame
			} else if (_scrambler_subtype == 1) {
				_frame_type = 1; // Scrambled M17 frame
			} else {
				_frame_type = 2; // Extended M17 frame
			}
			
			if (_debug_ctrl == true)
			{
				fprintf(stderr,
						"\nScrambler Key: 0x%06X; Seed: 0x%06X; Subtype: %02d; Frame Type: %d",
						_scrambler_seed, lfsr, _scrambler_subtype, _frame_type);
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
				// SECURITY FIX: Removed scrambler bytes logging - sensitive material must never be printed
				// for (i = 0; i < 16; i++)
				//   fprintf(stderr, " %02X", _scr_bytes[i]);
				// fprintf(stderr, "\n");
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

		float dist;	  // Euclidean distance for finding syncwords in the symbol stream

		for (int counterin = 0; counterin < ninput_items[0]; counterin++)
		{
			// wait for another symbol
			float sample = in[counterin];  // SECURITY FIX: Reduce variable scope

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
							if (_signed_str && _fn < m17_constants::SIGNATURE_START_FN)
							{
								// CRITICAL SECURITY FIX: Replace broken XOR hash with proper SHA-256
								if (_fn == 0) {
									// Initialize SHA-256 context for new stream
									if (_sha_ctx != nullptr) {
										EVP_MD_CTX_free(_sha_ctx);
									}
									_sha_ctx = EVP_MD_CTX_new();
									if (_sha_ctx == nullptr) {
										fprintf(stderr, "ERROR: Failed to create SHA-256 context\n");
										return -1;
									}
									if (EVP_DigestInit_ex(_sha_ctx, EVP_sha256(), NULL) != 1) {
										fprintf(stderr, "ERROR: Failed to initialize SHA-256\n");
										EVP_MD_CTX_free(_sha_ctx);
										_sha_ctx = nullptr;
										return -1;
									}
								}
								
								// Update SHA-256 hash with frame data
								if (_sha_ctx != nullptr) {
									if (EVP_DigestUpdate(_sha_ctx, _frame_data, 16) != 1) {
										fprintf(stderr, "ERROR: Failed to update SHA-256 hash\n");
										EVP_MD_CTX_free(_sha_ctx);
										_sha_ctx = nullptr;
										return -1;
									}
								}
							}

							// NOTE: Don't attempt decryption when a signed stream is >= SIGNATURE_START_FN
							// The Signature is not encrypted

							// AES
							if (_encr_type == ENCR_AES)
							{
								memcpy(_iv, _lsf.meta, 14);
								_iv[14] = (_fn >> 8) & 0x7F; // High byte of frame number (correct byte order)
								_iv[15] = _fn & 0xFF;  // SECURITY FIX: Remove redundant & 0xFF

								if (_signed_str && (_fn % m17_constants::FRAME_NUMBER_MAX) < m17_constants::SIGNATURE_START_FN) // signed stream
									aes_ctr_bytewise_payload_crypt(_iv, _key, _frame_data, _aes_subtype);
								else if (!_signed_str) // non-signed stream
									aes_ctr_bytewise_payload_crypt(_iv, _key, _frame_data, _aes_subtype);
							}

							// Scrambler
							if (_encr_type == ENCR_SCRAM)
							{
								if (_fn != 0 && (_fn % m17_constants::FRAME_NUMBER_MAX) != _expected_next_fn) // frame skip, etc
									_scrambler_seed = scrambler_seed_calculation(_scrambler_subtype, _scrambler_key, _fn & (m17_constants::FRAME_NUMBER_MAX - 1));
								else if (_fn == 0)
									_scrambler_seed = _scrambler_key; // reset back to key value

								if (_signed_str && (_fn % m17_constants::FRAME_NUMBER_MAX) < m17_constants::SIGNATURE_START_FN) // signed stream
									scrambler_sequence_generator();
								else if (!_signed_str) // non-signed stream
									scrambler_sequence_generator();
								else
									// CRITICAL SECURITY FIX: Use secure memory clearing
									explicit_bzero(_scr_bytes, sizeof(_scr_bytes)); // zero out stale scrambler bytes so they aren't applied to the sig frames

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
								// SECURITY FIX: Removed frame data logging - sensitive material must never be printed
								// if (_debug_data == true)
								// {
								//   printf("%02X", _frame_data[i]);
								// }
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
							if ((_lich_cnt == 0) || ((_fn % m17_constants::FRAME_NUMBER_MAX) != _expected_next_fn && _fn < m17_constants::SIGNATURE_START_FN))
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
							if (_fn >= m17_constants::SIGNATURE_START_FN && _signed_str == true)
							{
								memcpy(&_sig[((_fn & (m17_constants::FRAME_NUMBER_MAX - 1)) - m17_constants::SIGNATURE_START_FN) * 16], _frame_data, 16);

								if (_fn == (m17_constants::SIGNATURE_END_FN | m17_constants::EOT_MARKER))
								{
									// CRITICAL SECURITY FIX: Finalize SHA-256 hash before signature verification
									if (_sha_ctx != nullptr) {
										unsigned int digest_len;
										if (EVP_DigestFinal_ex(_sha_ctx, _digest, &digest_len) != 1) {
											fprintf(stderr, "ERROR: Failed to finalize SHA-256 hash\n");
											EVP_MD_CTX_free(_sha_ctx);
											_sha_ctx = nullptr;
											return -1;
										}
										EVP_MD_CTX_free(_sha_ctx);
										_sha_ctx = nullptr;
									}
									
									// CRITICAL SECURITY FIX: Removed all key material logging
									// Never log digest, public keys, or signatures

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

							_expected_next_fn = (_fn + 1) % m17_constants::FRAME_NUMBER_MAX;
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
