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

#include <gnuradio/io_signature.h>
#include "m17_decoder_impl.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#include "m17.h"
#include "m17_log.h"

namespace gr
{
	namespace m17
	{

		m17_decoder::sptr
		m17_decoder::make(bool debug_data, bool debug_ctrl, float sw_threshold,
						  float vt_threshold, bool callsign, bool signed_str, int encr_type,
						  std::string key, std::string seed, std::string pub_key)
		{
			return gnuradio::get_initial_sptr(new m17_decoder_impl(debug_data, debug_ctrl, sw_threshold, vt_threshold, callsign,
																   signed_str, encr_type, key, seed, pub_key));
		}

		/*
		 * The private constructor
		 */
		m17_decoder_impl::m17_decoder_impl(bool debug_data, bool debug_ctrl,
										   float sw_threshold, float vt_threshold,
										   bool callsign, bool signed_str,
										   int encr_type,
										   std::string key, std::string seed, std::string pub_key) : gr::block("m17_decoder",
																						  gr::io_signature::make(1, 1, sizeof(float)),
																						  gr::io_signature::make(1, 1, sizeof(char))),
																				_debug_data(debug_data), _debug_ctrl(debug_ctrl),
																				_sw_threshold(sw_threshold), _vt_threshold(vt_threshold),
																				_callsign(callsign), _signed_str(signed_str)
		{
			set_debug_data(debug_data);
			set_debug_ctrl(debug_ctrl);
			set_sw_threshold(sw_threshold);
			set_vt_threshold(vt_threshold);
			set_callsign(callsign);
			set_signed(signed_str);
			set_key(key);
			set_pub_key(pub_key);
			set_seed(seed);
			set_encr_type(encr_type);
			_expected_next_fn = 0;

			message_port_register_out(pmt::mp("fields"));
		}

		// tag for console output: the block alias if set in GRC, otherwise M17_DEC
		std::string m17_decoder_impl::tag() const
		{
			return alias_set() ? alias() : std::string("M17_DEC");
		}

		bool m17_decoder_impl::start()
		{
			uint8_t zero[64] = {0};
			std::string aes = memcmp(_key, zero, sizeof(_key)) ? "AES key set" : "no AES key";
			std::string scr = _scrambler_key ? "scrambler seed " + std::to_string(8 * (_scrambler_subtype + 1)) + "-bit" : "no scrambler seed";
			std::string pub = memcmp(_pub_key, zero, sizeof(_pub_key)) ? "public key set" : "no public key";
			std::string dbg;
			if (_debug_data)
				dbg += ", debug data on";
			if (_debug_ctrl)
				dbg += ", debug control on";

			m17_log(tag(), "Ready: syncword threshold %.1f, Viterbi threshold %.1f, %s, %s, %s%s",
					_sw_threshold, _vt_threshold, aes.c_str(), scr.c_str(), pub.c_str(), dbg.c_str());
			_started = true;
			return gr::block::start();
		}

		// a new transmission (or the end of one): clear the reception summary
		void m17_decoder_impl::rx_reset(void)
		{
			_rx_frames = 0;
			_rx_max_e = 0.0f;
			_rx_sig.clear();
			_rx_lsf_seen = false;
		}

		// publish the LSF fields on the 'fields' message port
		void m17_decoder_impl::publish_fields(void)
		{
			char dst[10] = {0}, src[10] = {0};
			decode_callsign_bytes(dst, _lsf.dst);
			decode_callsign_bytes(src, _lsf.src);

			pmt::pmt_t dict = pmt::make_dict();
			dict = pmt::dict_add(dict, pmt::mp("src"), pmt::intern(src));
			dict = pmt::dict_add(dict, pmt::mp("dst"), pmt::intern(dst));
			dict = pmt::dict_add(dict, pmt::mp("type"), pmt::init_u8vector(2, _lsf.type));
			dict = pmt::dict_add(dict, pmt::mp("meta"), pmt::init_u8vector(14, _lsf.meta));
			message_port_pub(pmt::mp("fields"), dict);
		}

		/*
		 * Our virtual destructor.
		 */
		m17_decoder_impl::~m17_decoder_impl()
		{
		}

		void m17_decoder_impl::set_sw_threshold(float sw_threshold)
		{
			_sw_threshold = sw_threshold;
			if (_started)
				m17_log(tag(), "Syncword threshold changed: %.1f", _sw_threshold);
		}

		void m17_decoder_impl::set_vt_threshold(float vt_threshold)
		{
			_vt_threshold = vt_threshold;
			if (_started)
				m17_log(tag(), "Viterbi threshold changed: %.1f", _vt_threshold);
		}

		void m17_decoder_impl::set_debug_data(bool debug)
		{
			_debug_data = debug;
		}

		void m17_decoder_impl::set_debug_ctrl(bool debug)
		{
			_debug_ctrl = debug;
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
		}

		void m17_decoder_impl::set_callsign(bool callsign)
		{
			_callsign = callsign;
		}

		void m17_decoder_impl::set_signed(bool signed_str)
		{
			_signed_str = signed_str;
		}

		void m17_decoder_impl::set_key(std::string arg) // *UTF-8* encoded byte array
		{
			int length = arg.size();

			if (!length)
				return;

			int i = 0, j = 0;
			while ((j < (int)sizeof(_key)) && (i < length))
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

			// the key itself is never printed
			if (_started)
				m17_log(tag(), "AES key changed (%d bytes)", length);

			fflush(stdout);
		}

		void m17_decoder_impl::set_pub_key(std::string arg) // *UTF-8* encoded byte array
		{
			int length = arg.size();

			if (!length)
				return;

			int i = 0, j = 0;
			while ((j < (int)sizeof(_pub_key)) && (i < length))
			{
				if ((unsigned int)arg.data()[i] < 0xc2) // https://www.utf8-chartable.de/
				{
					_pub_key[j] = arg.data()[i];
					i++;
					j++;
				}
				else
				{
					_pub_key[j] = (arg.data()[i] - 0xc2) * 0x40 + arg.data()[i + 1];
					i += 2;
					j++;
				}
			}

			length = j; // index from 0 to length-1

			if (_started)
				m17_log(tag(), "Public key changed (%d bytes)", length);

			fflush(stdout);
		}

		void m17_decoder_impl::set_seed(std::string arg) // *UTF-8* encoded byte array
		{
			int length = arg.size();

			if (!length)
				return;

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

			// the LFSR size (_scrambler_subtype) comes from the seed length or the received TYPE, never from the value
			// TODO: Set Frame Type based on scrambler_subtype value
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
			pack_bit_array_into_byte_array(_scrambler_pn, _scr_bytes, PAYLOAD_BYTES);

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

			for (int counterin = 0; counterin < ninput_items[0]; counterin++)
			{
				// wait for another symbol
				sample = in[counterin];

				if (!syncd)
				{
					float dist; // Euclidean distance for finding syncwords in the symbol stream

					// push new symbol
					for (uint8_t i = 0; i < 7; i++)
					{
						last[i] = last[i + 1];
					}

					last[7] = sample;

					// calculate euclidean norm against the stream syncword
					dist = eucl_norm(last, str_sync_symbols, 8);

					if (dist < _sw_threshold) // stream frame syncword detected
					{
						// fprintf(stderr, "str_sync_symbols dist: %3.5f\n", dist);
						syncd = 1;
						pushed = 0;
						flp = 0;
						continue;
					}

					// calculate euclidean against the LSF syncword
					dist = eucl_norm(last, lsf_sync_symbols, 8);

					if (dist < _sw_threshold) // LSF syncword
					{
						// fprintf(stderr, "lsf_sync dist: %3.5f\n", dist);
						syncd = 1;
						pushed = 0;
						flp = 1;
						continue;
					}

					// calculate euclidean norm against the packet syncword
					dist = eucl_norm(last, pkt_sync_symbols, 8);

					if (dist < _sw_threshold) // packet frame syncword
					{
						// fprintf(stderr, "lsf_sync dist: %3.5f\n", dist);
						syncd = 1;
						pushed = 0;
						flp = 2;
						continue;
					}
				}
				else
				{
					_pld[pushed++] = sample;

					if (pushed == SYM_PER_PLD)
					{
						// if it is a stream frame
						if (flp == 0)
						{
							// decode
							uint32_t e = decode_str_frame(_stream_frame_data, _lich_b, &_fn, &_lich_cnt, _pld);

							uint16_t type = ((uint16_t)_lsf.type[0] << 8) + _lsf.type[1];
							_signed_str = (type >> 11) & 1;

							// encryption type and subtype are taken from the received TYPE field
							// (only the AES key / scrambler seed come from the block's settings)
							const uint8_t rx_encr = (type >> 3) & 3;
							const uint8_t rx_encr_sub = (type >> 5) & 3;

							/// if the stream is signed (process before decryption)
							if (_signed_str && _fn < 0x7FFC)
							{
								if (_fn == 0)
									memset(_digest, 0, sizeof(_digest));

								for (uint8_t i = 0; i < sizeof(_digest); i++)
									_digest[i] ^= _stream_frame_data[i];
								uint8_t tmp = _digest[0];
								for (uint8_t i = 0; i < sizeof(_digest) - 1; i++)
									_digest[i] = _digest[i + 1];
								_digest[sizeof(_digest) - 1] = tmp;
							}

							// NOTE: Don't attempt decryption when a signed stream is >= 0x7FFC
							// The Signature is not encrypted

							// AES
							if (rx_encr == ENCR_AES)
							{
								_aes_subtype = rx_encr_sub; // key size as signalled by the transmitter
								memcpy(_iv, _lsf.meta, 14);   // 112-bit nonce from META
								_iv[14] = (_fn >> 8) & 0x7F;  // 16-bit FN, EOS bit cleared
								_iv[15] = (_fn & 0xFF) & 0xFF;

								if (_signed_str && (_fn % 0x8000) < 0x7FFC) // signed stream
									aes_ctr_bytewise_payload_crypt(_iv, _key, _stream_frame_data, _aes_subtype);
								else if (!_signed_str) // non-signed stream
									aes_ctr_bytewise_payload_crypt(_iv, _key, _stream_frame_data, _aes_subtype);
							}

							// Scrambler
							if (rx_encr == ENCR_SCRAM)
							{
								_scrambler_subtype = rx_encr_sub; // LFSR size as signalled by the transmitter
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

								for (uint8_t i = 0; i < PAYLOAD_BYTES; i++)
								{
									_stream_frame_data[i] ^= _scr_bytes[i];
								}
							}

							// dump data
							if (_debug_data == true)
								m17_log(tag(), "FN %04X  %s  e=%.1f", _fn, m17_hex(_stream_frame_data, PAYLOAD_BYTES, 4).c_str(), (float)e / 0xFFFF);

							if ((_fn & 0x7FFF) < 0x7FFC) // data frame (not a signature frame)
							{
								_rx_frames++;
								if ((float)e / 0xFFFF > _rx_max_e)
									_rx_max_e = (float)e / 0xFFFF;
							}

							// set a threshold on the Viterbi metric to prevent sound artifacts
							if ((float)e / 0xFFFF <= _vt_threshold)
								memcpy(&out[countout], _stream_frame_data, PAYLOAD_BYTES);
							else
								memset(&out[countout], 0, PAYLOAD_BYTES);
							countout += PAYLOAD_BYTES;

							// send codec2 stream to stdout
							// fwrite(_stream_frame_data, PAYLOAD_BYTES, 1, stdout);

							// If we're at the start of a superframe, or we missed a frame, reset the LICH state
							if ((_lich_cnt == 0) || ((_fn % 0x8000) != _expected_next_fn && _fn < 0x7FFC))
								lich_chunks_rcvd = 0;

							lich_chunks_rcvd |= (1 << _lich_cnt);
							memcpy((uint8_t *)&_lsf + _lich_cnt * 5, _lich_b, 5);

							// complete LSF rebuilt from the LICH
							if (lich_chunks_rcvd == 0x3F) // all 6 chunks received?
							{
								if (CRC_M17((uint8_t *)&_lsf, sizeof(_lsf)))
								{
									if (_debug_ctrl == true)
										m17_log(tag(), "LSF (LICH): CRC error");
								}
								else if (!_rx_lsf_seen || memcmp(&_lsf, &_rx_lsf, sizeof(_lsf)))
								{
									// late entry (no LSF frame received) or the LSF has changed
									m17_log(tag(), "%s %s", _rx_lsf_seen ? "LSF changed:" : "RX start (late entry):", m17_lsf_str(_lsf, _callsign).c_str());
									_rx_lsf = _lsf;
									_rx_lsf_seen = true;
									publish_fields();
								}
								else if (_debug_ctrl == true)
									m17_log(tag(), "LSF (LICH) unchanged");
							}

							// if the contents of the payload is now digital signature, not data/voice
							if (_fn >= 0x7FFC && _signed_str == true)
							{
								memcpy(&_sig[((_fn & 0x7FFF) - 0x7FFC) * PAYLOAD_BYTES], _stream_frame_data, PAYLOAD_BYTES);

								if (_fn == (0x7FFF | 0x8000))
								{

									bool have_key = false;
									for (uint8_t i = 0; i < 64; i++)
										have_key |= (_pub_key[i] != 0);

									if (!have_key)
										_rx_sig = ", signature not verified (no public key set)";
									else if (uECC_verify(_pub_key, _digest, sizeof(_digest), _sig, _curve))
										_rx_sig = ", signature OK";
									else
										_rx_sig = ", signature INVALID";
								}
							}

							_expected_next_fn = (_fn + 1) % 0x8000;

							// end of stream (EOS bit; for signed streams the last signature frame)
							if (_fn & 0x8000)
							{
								m17_log(tag(), "RX end: %d frames, max e=%.1f%s", _rx_frames, _rx_max_e, _rx_sig.c_str());
								rx_reset();
							}
						}

						else if (flp == 1) // lsf
						{
							_pkt_wr_offs = 0; // a new transmission starts - discard any partial packet
							uint32_t e = decode_LSF(&_lsf, _pld);

							if (CRC_M17((uint8_t *)&_lsf, sizeof(_lsf)))
								m17_log(tag(), "LSF received with CRC error, e=%.1f", (float)e / 0xFFFF);
							else
							{
								uint16_t type = ((uint16_t)_lsf.type[0] << 8) + _lsf.type[1];
								_signed_str = (type >> 11) & 1;

								rx_reset();
								if (_debug_ctrl == true)
									m17_log(tag(), "RX start: %s, e=%.1f", m17_lsf_str(_lsf, _callsign).c_str(), (float)e / 0xFFFF);
								else
									m17_log(tag(), "RX start: %s", m17_lsf_str(_lsf, _callsign).c_str());
								_rx_lsf = _lsf;
								_rx_lsf_seen = true;
								publish_fields();
							}
						}

						else // packet frame
						{
							// decode
							uint8_t frame_data[25] = {0};
							uint8_t eof = 0;
							uint8_t pkt_fn = 0;
							uint16_t len = 0;

							uint32_t e = decode_pkt_frame(frame_data, &eof, &pkt_fn, _pld);

							bool frame_ok = true;
							if (!eof)
							{
								// frames must arrive in order (counter = index) and fit into 33 frames
								if (pkt_fn != _pkt_wr_offs / 25 || _pkt_wr_offs + 25 > 33 * 25)
								{
									m17_log(tag(), "Packet frame out of sequence - packet dropped");
									_pkt_wr_offs = 0;
									frame_ok = false;
								}
								else
								{
									memcpy(&rcvd_msg[_pkt_wr_offs], frame_data, 25);
									_pkt_wr_offs += 25;
								}
							}

							else if (pkt_fn < 1 || pkt_fn > 25 || _pkt_wr_offs + pkt_fn > 33 * 25)
							{
								m17_log(tag(), "Invalid last packet frame - packet dropped");
								_pkt_wr_offs = 0;
								frame_ok = false;
							}

							else
							{
								memcpy(&rcvd_msg[_pkt_wr_offs], frame_data, pkt_fn);
								len = _pkt_wr_offs + pkt_fn;
								rcvd_msg[len] = 0; // guard: the SMS text is always null-terminated

								// TODO: we use last LSF data that might be outdated
								if (rcvd_msg[0] == 0x05 && CRC_M17((uint8_t *)rcvd_msg, len) == 0)
								{
									// handle message output (for a text message)
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

									dict = pmt::dict_add(dict, pmt::mp("sms"), pmt::intern((char *)&rcvd_msg[1]));

									message_port_pub(pmt::mp("fields"), dict);

									std::string sms((char *)&rcvd_msg[1]);
									m17_log(tag(), "RX SMS: %s -> %s, %zu bytes: %s", (char *)d_src, (char *)d_dst, sms.size(), sms.c_str());
								}

								else
									m17_log(tag(), "Packet CRC error or unsupported protocol - packet dropped");

								_pkt_wr_offs = 0;
							}

							if (frame_ok && _debug_ctrl == true)
							{
								if (!eof)
									m17_log(tag(), "Packet frame %d, e=%.1f", pkt_fn, (float)e / 0xFFFF);
								else
									m17_log(tag(), "Packet frame last, %d bytes, e=%.1f", pkt_fn, (float)e / 0xFFFF);
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
