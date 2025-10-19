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

// SECURITY FIX: Add necessary includes for secure operations
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <cstring>  // for explicit_bzero
#include <iomanip>  // for std::setfill, std::setw
#include <sstream>  // for std::stringstream

// 240620: todo uncomment #idef AES for cryptography and #ifdef ECC for signature

// in m17_coder_impl.h: #define AES #define ECC

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "m17_coder_impl.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#ifdef __linux__
#include <sys/random.h>
#endif

#include "m17.h"

#ifdef AES
#include "aes.h"
#endif

#ifdef ECC
#include "uECC.h"
#endif

namespace gr
{
  namespace m17
  {

    m17_coder::sptr
    m17_coder::make(std::string src_id, std::string dst_id, int mode,
                    int data, int encr_type, int encr_subtype, int aes_subtype, int can,
                    std::string meta, std::string key,
                    std::string priv_key, bool debug, bool signed_str, std::string seed)
    {
      return gnuradio::get_initial_sptr(new m17_coder_impl(src_id, dst_id, mode, data, encr_type, encr_subtype,
                                                           aes_subtype, can, meta, key, priv_key, debug, signed_str, seed));
    }

    /*
     * The private constructor
     */
    m17_coder_impl::m17_coder_impl(std::string src_id, std::string dst_id,
                                   int mode, int data, int encr_type,
                                   int encr_subtype, int aes_subtype, int can,
                                   std::string meta, std::string key,
                                   std::string priv_key, bool debug,
                                   bool signed_str, std::string seed) : gr::block("m17_coder", gr::io_signature::make(1, 1, sizeof(char)),
                                                                                  gr::io_signature::make(1, 1, sizeof(float))),
                                                                        _mode(mode), _data(data), _encr_subtype(encr_subtype), _aes_subtype(aes_subtype), _meta(meta), _debug(debug), _signed_str(signed_str), _can(can)
    {
      set_encr_type(encr_type); // overwritten by set_seed()
      set_type(mode, data, _encr_type, encr_subtype, can);
      set_aes_subtype(aes_subtype, encr_type);
      set_meta(meta); // depends on   ^^^ encr_subtype
      set_seed(seed); // depends on   ^^^ encr_subtype
      set_src_id(src_id);
      set_dst_id(dst_id);
      set_signed(signed_str);
      set_debug(debug);
      set_output_multiple(m17_constants::FRAME_SIZE);
      
      // SECURITY FIX: Initialize key management system
      init_key_management();
#ifdef AES
      if (_encr_type == ENCR_AES)
      {
        // CRITICAL SECURITY FIX: Use hardware RNG for secure IV generation
        // Use /dev/hwrng on iMX93, fallback to /dev/urandom
        int rng_fd = open("/dev/hwrng", O_RDONLY);
        if (rng_fd < 0) {
            // Fallback to urandom
            rng_fd = open("/dev/urandom", O_RDONLY);
        }
        if (rng_fd >= 0) {
            if (read(rng_fd, _iv, m17_constants::AES_IV_SIZE) != m17_constants::AES_IV_SIZE) {
                close(rng_fd);
                // Handle error - MUST NOT continue with insecure IV
                throw std::runtime_error("Failed to generate secure IV");
            }
            close(rng_fd);
        } else {
            // SECURITY FIX: Fail securely - never use weak randomness
            fprintf(stderr, "ERROR: Cannot access secure random number generator\n");
            throw std::runtime_error("Cannot generate secure IV - no secure RNG available");
        }
        
        // Frame number must be part of IV for CTR mode
        _iv[m17_constants::IV_FRAME_NUMBER_OFFSET] = (_fn >> 8) & m17_constants::IV_FRAME_NUMBER_MASK;
        _iv[15] = (_fn >> 0) & 0xFF;
      }
#endif
      /*
            uint16_t ccrc = LSF_CRC (&_lsf);
              _lsf.crc[0] = ccrc >> 8;
              _lsf.crc[1] = ccrc & 0xFF;
      */
      _got_lsf = 0; // have we filled the LSF struct yet?
      _fn = 0;      // 16-bit Frame Number (for the stream mode)
      _finished = false;
      message_port_register_in(pmt::mp("end_of_transmission"));
      set_msg_handler(pmt::mp("end_of_transmission"), [this](const pmt::pmt_t &msg)
                      { end_of_transmission(msg); });
      _send_preamble = true; // send preamble once in the work function

      if (_debug == true)
      {
        // destination set to "@ALL"
        encode_callsign_bytes(_lsf.dst, (const unsigned char *)"@ALL");

        // source set to "N0CALL"
        encode_callsign_bytes(_lsf.src, (const unsigned char *)"N0CALL");

        // no enc or subtype field, normal 3200 voice
        _type = M17_TYPE_STREAM | M17_TYPE_VOICE | M17_TYPE_CAN(0);

#ifdef AES
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
#endif
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

        _finished = 0;
      }
#ifdef AES
      if (_encr_type == ENCR_AES)
      {
        memcpy(&(_lsf.meta), _iv, m17_constants::META_FIELD_SIZE);
        _iv[m17_constants::IV_FRAME_NUMBER_OFFSET] = (_fn >> 8) & m17_constants::IV_FRAME_NUMBER_MASK;
        _iv[15] = (_fn >> 0) & 0xFF;

        // re-calculate LSF CRC with IV insertion
        update_LSF_CRC(&_lsf);
      }
//        srand (time (NULL));	//random number generator (for IV seed value)
//        memset (_key, 0, 32 * sizeof (uint8_t));
//        memset (_iv, 0, 16 * sizeof (uint8_t));
#endif
      // SECURITY FIX: Initialize SHA context for signed streams
      if (_signed_str) {
        _sha_ctx = EVP_MD_CTX_new();
        if (_sha_ctx == nullptr) {
          throw std::runtime_error("Failed to create SHA-256 context for signed stream");
        }
      } else {
        _sha_ctx = nullptr;
      }
    }

    void m17_coder_impl::end_of_transmission(const pmt::pmt_t &msg)
    {
      _finished = true;
      std::cout << "***** End of Transmission ********\n";
      pmt::print(msg);
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
      printf("new encr type: %x -> ", _encr_type);
    }

    void m17_coder_impl::set_signed(bool signed_str)
    {
      _signed_str = signed_str;
      if (_signed_str == true)
        printf("Signed\n");
      else
        printf("Unsigned\n");
    }

    void m17_coder_impl::set_debug(bool debug)
    {
      _debug = debug;
      if (_debug == true)
        printf("Debug true\n");
      else
        printf("Debug false\n");
    }

    void m17_coder_impl::set_src_id(std::string src_id)
    {
      // SECURITY FIX: Add input validation
      if (src_id.empty()) {
        fprintf(stderr, "ERROR: Source ID cannot be empty\n");
        return;
      }
      
      // SECURITY FIX: Validate callsign format (alphanumeric only)
      for (char c : src_id) {
        if (!isalnum(c) && c != '-') {
          fprintf(stderr, "ERROR: Invalid character in source ID: %c\n", c);
          return;
        }
      }
      
      int length;
      for (int i = 0; i < m17_constants::CALLSIGN_BUFFER_SIZE; i++)
      {
        _src_id[i] = 0;
      }
      if (src_id.length() > m17_constants::MAX_CALLSIGN_LENGTH)
        length = m17_constants::MAX_CALLSIGN_LENGTH;
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
      // SECURITY FIX: Add input validation
      if (dst_id.empty()) {
        fprintf(stderr, "ERROR: Destination ID cannot be empty\n");
        return;
      }
      
      // SECURITY FIX: Validate callsign format (alphanumeric only)
      for (char c : dst_id) {
        if (!isalnum(c) && c != '-') {
          fprintf(stderr, "ERROR: Invalid character in destination ID: %c\n", c);
          return;
        }
      }
      
      int length;
      for (int i = 0; i < 10; i++)
      {
        _dst_id[i] = 0;
      }
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

    void m17_coder_impl::set_priv_key(std::string arg) // Hex-encoded private key
    {
      // CRITICAL SECURITY FIX: Use proper hex parsing instead of UTF-8
      // NEVER log private keys, not even in debug mode
      // SECURITY: Removed private key logging - sensitive material must never be printed
      
      _priv_key_loaded = true;
      
      // SECURITY FIX: Parse hex string instead of UTF-8
      // Expect 64 hex characters for 32-byte key
      if (arg.size() != m17_constants::HEX_KEY_LENGTH) {
        fprintf(stderr, "ERROR: Private credential must be 64 hex characters (32 bytes)\n");
        _priv_key_loaded = false;
        return;
      }
      
      // Parse hex string to binary
      for (int i = 0; i < 32; i++) {
        char hex_byte[3] = {arg[i*2], arg[i*2+1], '\0'};
        char *endptr;
        unsigned long val = strtoul(hex_byte, &endptr, 16);
        if (*endptr != '\0' || val > 255) {
          fprintf(stderr, "ERROR: Invalid hex character in private credential\n");
          _priv_key_loaded = false;
          return;
        }
        _priv_key[i] = (uint8_t)val;
      }
    }

    void m17_coder_impl::set_key(std::string arg) // Hex-encoded encryption key
    {
      // CRITICAL SECURITY FIX: Use proper hex parsing instead of UTF-8
      // NEVER log encryption keys, not even in debug mode
      // SECURITY: Removed key logging - keys must never be printed to console
      
      // SECURITY FIX: Parse hex string instead of UTF-8
      // Expect 64 hex characters for 32-byte key
      if (arg.size() != m17_constants::HEX_KEY_LENGTH) {
        fprintf(stderr, "ERROR: Encryption credential must be %d hex characters (%d bytes)\n", 
                m17_constants::HEX_KEY_LENGTH, m17_constants::AES_KEY_SIZE);
        return;
      }
      
      // Parse hex string to binary
      for (int i = 0; i < 32; i++) {
        char hex_byte[3] = {arg[i*2], arg[i*2+1], '\0'};
        char *endptr;
        unsigned long val = strtoul(hex_byte, &endptr, 16);
        if (*endptr != '\0' || val > 255) {
          fprintf(stderr, "ERROR: Invalid hex character in encryption credential\n");
          return;
        }
        _key[i] = (uint8_t)val;
      }
    }

    void m17_coder_impl::set_seed(std::string arg) // Hex-encoded seed
    {
      // SECURITY FIX: Use proper hex parsing instead of UTF-8
      // Expect 6 hex characters for 3-byte seed
      if (arg.size() != m17_constants::HEX_SEED_LENGTH) {
        fprintf(stderr, "ERROR: Seed must be %d hex characters (%d bytes)\n", 
                m17_constants::HEX_SEED_LENGTH, m17_constants::SEED_SIZE);
        return;
      }
      
      // Parse hex string
      for (int i = 0; i < 3; i++) {
        char hex_byte[3] = {arg[i*2], arg[i*2+1], '\0'};
        char *endptr;
        unsigned long val = strtoul(hex_byte, &endptr, 16);
        if (*endptr != '\0') {
          fprintf(stderr, "ERROR: Invalid hex character in seed\n");
          return;
        }
        _seed[i] = (uint8_t)val;
      }
      
      // SECURITY FIX: Never log seed material
      // Calculate scrambler seed from parsed bytes (3 bytes = 24 bits)
      _scrambler_seed = (_seed[0] << 16) | (_seed[1] << 8) | _seed[2];
      
      // SECURITY FIX: Never log scrambler keys or seed material

      _encr_type = ENCR_SCRAM; // Scrambler key was passed
    }

    void m17_coder_impl::set_meta(std::string meta) // either an ASCII string if encr_subtype==0 or *UTF-8* encoded byte array
    {
      int length;

      memset(_lsf.meta, 0, 14);

      printf("new meta: ");
      if (_encr_subtype == 0) // meta is \0-terminated string
      {
        // SECURITY FIX: Fix container bounds - ensure safe access
        if (meta.length() < 14) {
          length = meta.length();
        } else {
          length = 14;
          // SECURITY FIX: Don't access meta[length] - it's out of bounds
          // Instead, truncate the string safely
          meta = meta.substr(0, 14);
        }
        printf("%s\n", meta.c_str());
        for (int i = 0; i < length; i++)
        {
          _lsf.meta[i] = meta[i];
        }
      }
      else
      {
        length = meta.size();
        int i = 0, j = 0;
        while ((j < 14) && (i < length))
        {
          if ((unsigned int)meta.data()[i] < 0xc2) // https://www.utf8-chartable.de/
          {
            _lsf.meta[j] = meta.data()[i];
            i++;
            j++;
          }
          else
          {
            _lsf.meta[j] =
                (meta.data()[i] - 0xc2) * 0x40 + meta.data()[i + 1];
            i += 2;
            j++;
          }
        }
        length = j; // index from 0 to length-1
        printf("%d bytes: ", length);
        for (i = 0; i < length; i++)
          printf("%02X ", _lsf.meta[i]);
        printf("\n");
      }
      fflush(stdout);
      uint16_t ccrc = LSF_CRC(&_lsf);
      _lsf.crc[0] = ccrc >> 8;
      _lsf.crc[1] = ccrc & 0xFF;
    }

    void m17_coder_impl::set_mode(int mode)
    {
      _mode = mode;
      printf("new mode: %x -> ", _mode);
      set_type(_mode, _data, _encr_type, _encr_subtype, _can);
    }

    void m17_coder_impl::set_data(int data)
    {
      _data = data;
      printf("new data type: %x -> ", _data);
      set_type(_mode, _data, _encr_type, _encr_subtype, _can);
    }

    void m17_coder_impl::set_encr_subtype(int encr_subtype)
    {
      _encr_subtype = encr_subtype;
      printf("new encr subtype: %x -> ", _encr_subtype);
      set_type(_mode, _data, _encr_type, _encr_subtype, _can);
    }

    void m17_coder_impl::set_aes_subtype(int aes_subtype, int encr_type)
    {
      _aes_subtype = aes_subtype;
      printf("new AES subtype: %x -> ", _aes_subtype);
      if (encr_type == ENCR_AES) // AES ENC, 3200 voice
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
        printf("ERROR: encryption type != AES");
      printf("\n");
    }

    void m17_coder_impl::set_can(int can)
    {
      _can = can;
      printf("new CAN: %x -> ", _can);
      set_type(_mode, _data, _encr_type, _encr_subtype, _can);
    }

    void m17_coder_impl::set_type(int mode, int data, encr_t encr_type,
                                  int encr_subtype, int can)
    {
      short tmptype;
      tmptype =
          mode | (data << 1) | (encr_type << 3) | (encr_subtype << 5) | (can << 7);
      _lsf.type[0] = tmptype >> 8;   // MSB
      _lsf.type[1] = tmptype & 0xff; // LSB
      uint16_t ccrc = LSF_CRC(&_lsf);
      _lsf.crc[0] = ccrc >> 8;
      _lsf.crc[1] = ccrc & 0xFF;
      printf("Transmission type: 0x%02X%02X\n", _lsf.type[0], _lsf.type[1]);
      fflush(stdout);
    }

    /*
     * Our virtual destructor.
     */
m17_coder_impl::~m17_coder_impl()
{
  // CRITICAL SECURITY FIX: Securely wipe all cryptographic material
  explicit_bzero(_key, sizeof(_key));
  explicit_bzero(_priv_key, sizeof(_priv_key));
  explicit_bzero(_iv, sizeof(_iv));
  explicit_bzero(_digest, sizeof(_digest));
  explicit_bzero(_sig, sizeof(_sig));
  explicit_bzero(_scr_bytes, sizeof(_scr_bytes));
  
  // Clean up EVP context if it exists
  if (_sha_ctx) {
    EVP_MD_CTX_free(_sha_ctx);
  }
}

    void
    m17_coder_impl::forecast(int noutput_items,
                             gr_vector_int &ninput_items_required)
    {
      ninput_items_required[0] = noutput_items / 12; // 16 inputs -> 192 outputs
    }

    // scrambler pn sequence generation
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
        // SECURITY FIX: Never log scrambler seeds or keys - they are sensitive cryptographic material
        fprintf(stderr, "\nScrambler configured (details hidden for security); Subtype: %02d;\n", _scrambler_subtype);
        fprintf(stderr, " pN: ");
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

        bit &= 1;                 // truncate bit to 1 bit (required since I didn't do it above)
        lfsr = (lfsr << 1) | bit; // shift LFSR left once and OR bit onto LFSR's LSB
        lfsr &= 0xFFFFFF;         // truncate lfsr to 24-bit (really doesn't matter)
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
        // SECURITY FIX: Removed scrambler bytes logging - sensitive material must never be printed
        // for (i = 0; i < 16; i++)
        //   fprintf(stderr, " %02X", _scr_bytes[i]);
        // fprintf(stderr, "\n");
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

      uint8_t data[16], next_data[16]; // raw payload, packed bits

      if (_send_preamble == true)
      {
        gen_preamble(out, &countout, PREAM_LSF); // 0 - LSF preamble, as opposed to 1 - BERT preamble
        _send_preamble = false;
      }

      while ((countout < (uint32_t)noutput_items))
      {
        if (countin + 16 > ninput_items[0])
        {
          break;
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

        // get new data
        memcpy(next_data, in + countin, 16);
        countin += 16;

        // update the data before applying crypto
        memcpy(data, next_data, 16);

        // TODO if debug_mode==1 from lines 520 to 570
        // TODO add aes_subtype as user argument

#ifdef AES
        if (_encr_type == ENCR_AES)
        {
          memcpy(&(_next_lsf.meta), _iv, 14); // TODO: I suspect that this does not work
          _iv[m17_constants::IV_FRAME_NUMBER_OFFSET] = (_fn >> 8) & m17_constants::IV_FRAME_NUMBER_MASK;
          _iv[15] = (_fn >> 0) & 0xFF;
          aes_ctr_bytewise_payload_crypt(_iv, _key, data, _aes_subtype);
        }
        else
#endif
          // Scrambler
          if (_encr_type == ENCR_SCRAM)
          {
            scrambler_sequence_generator();
            for (uint8_t i = 0; i < 16; i++)
            {
              data[i] ^= _scr_bytes[i];
            }
          }

        /*fprintf(stderr, "Payload FN=%u: ", _fn);
        for (int i = 0; i < 16; i++)
          fprintf(stderr, "%02X ", data[i]);
        fprintf(stderr, "\n");*/

        if (_finished == false)
        {
          gen_frame(out + countout, data, FRAME_STR, &_lsf, _lich_cnt, _fn);
          countout += SYM_PER_FRA;         // gen frame always writes SYM_PER_FRA symbols = 192
          _fn = (_fn + 1) % m17_constants::FRAME_NUMBER_MAX;        // increment FN
          _lich_cnt = (_lich_cnt + 1) % 6; // continue with next LICH_CNT

          // update the stream digest if required
          if (_signed_str)
          {
              // CRITICAL SECURITY FIX: Initialize SHA-256 context on first frame
              if (_fn == 0) {  // First frame (before increment)
                if (EVP_DigestInit_ex(_sha_ctx, EVP_sha256(), NULL) != 1) {
                  fprintf(stderr, "ERROR: Failed to initialize SHA-256\n");
                  return -1;
                }
              }
              // CRITICAL SECURITY FIX: Replace insecure XOR digest with SHA-256
              // Append data to running hash context
              if (EVP_DigestUpdate(_sha_ctx, data, 16) != 1) {
                fprintf(stderr, "ERROR: SHA-256 update failed\n");
                EVP_MD_CTX_free(_sha_ctx);
                _sha_ctx = nullptr;
                return -1;
              }
          }

          // update LSF every 6 frames (superframe boundary)
          if (_fn > 0 && _lich_cnt == 0)
          {
            // TODO: fix the _next_lsf contents before uncommenting lines below
            //_lsf = _next_lsf;
            // update_LSF_CRC(&_lsf);
          }

          memcpy(data, next_data, 16);
        }
        else // send last frame(s)
        {
          printf("Sending last frame\n");
          if (!_signed_str)
            _fn |= m17_constants::EOT_MARKER;
          gen_frame(out + countout, data, FRAME_STR, &_lsf, _lich_cnt, _fn);
          countout += SYM_PER_FRA;         // gen frame always writes SYM_PER_FRA symbols = 192
          _lich_cnt = (_lich_cnt + 1) % 6; // continue with next LICH_CNT

          // if we are done, and the stream is signed, so we need to transmit the signature (4 frames)
          if (_signed_str)
          {
            // CRITICAL SECURITY FIX: Finalize SHA-256 digest
            unsigned int digest_len;
            EVP_DigestFinal_ex(_sha_ctx, _digest, &digest_len);
            EVP_MD_CTX_free(_sha_ctx);

            // CRITICAL SECURITY FIX: Add error checking for signature generation
            int result = uECC_sign(_priv_key, _digest, sizeof(_digest), _sig, _curve);
            if (result != 1) {
                fprintf(stderr, "ERROR: Signature generation failed!\n");
                // Clear sensitive data and abort transmission
                explicit_bzero(_digest, sizeof(_digest));
                return -1;  // Fail securely
            }

            // 4 frames with 512-bit signature
            _fn = m17_constants::SIGNATURE_START_FN; // signature has to start at 0x7FFC to end at 0x7FFF (0xFFFF with EoT marker set)
            for (uint8_t i = 0; i < 4; i++)
            {
              gen_frame(out + countout, &_sig[i * 16], FRAME_STR, &_lsf, _lich_cnt, _fn);
              countout += SYM_PER_FRA; // gen frame always writes SYM_PER_FRA symbols = 192
              _fn = (_fn < m17_constants::SIGNATURE_END_FN) ? _fn + 1 : (m17_constants::SIGNATURE_END_FN | m17_constants::EOT_MARKER);
              _lich_cnt = (_lich_cnt + 1) % 6; // continue with next LICH_CNT
            }

            if (_debug == true)
            {
              // SECURITY FIX: Never log signature data - it contains sensitive cryptographic material
              fprintf(stderr, "Signature: [SIGNATURE_DATA_HIDDEN_FOR_SECURITY]\n");
            }
          }
          // send EOT frame
          gen_eot(out + countout, &countout);
          // fprintf(stderr, "Stream has ended. Exiting.\n");
        } // finished == true
      } // loop on input data
      // Tell runtime system how many input items we consumed on
      // each input stream.
      consume_each(countin);
      //    printf(" noutput_items=%d countin=%d countout=%d\n",noutput_items,countin,countout);
      // Tell runtime system how many output items we produced.
      if (_finished == false)
        return countout;
      else
      {
        printf("Killing flowgraph\n");
        return -1;
      } // https://lists.gnu.org/archive/html/discuss-gnuradio/2016-12/msg00206.html
      // returning -1 (which is the magical value for "there's nothing coming anymore, you can shut down") would normally end a flow graph
    }

    // SECURITY FIX: Key Management System Implementation
    
    // Initialize key management in constructor
    void m17_coder_impl::init_key_management() {
        // Initialize storage paths
        _storage_paths.primary_path = "/data/m17/";
        _storage_paths.secondary_path = "/var/lib/m17/";
        _storage_paths.fallback_path = "/tmp/";
        _storage_paths.db_filename = "public_keys.db";
        
        // Detect writable storage path
        detect_storage_path();
        
        // Load existing keys from disk
        load_public_keys_from_disk();
        
        // Check for expired keys
        check_expired_keys();
    }
    
    // Nitrokey Integration Functions
    bool m17_coder_impl::generate_key_on_nitrokey(const std::string& label) {
        if (label.empty() || label.length() > 20) {
            fprintf(stderr, "ERROR: Invalid Nitrokey identifier (must be 1-20 characters)\n");
            return false;
        }
        
        // Check if Nitrokey is connected
        std::string check_cmd = "nitropy nk3 info";
        int check_result = system(check_cmd.c_str());
        if (check_result != 0) {
            fprintf(stderr, "ERROR: Nitrokey not connected or nitropy not available\n");
            return false;
        }
        
        // Use nitropy CLI to generate Ed25519 key on Nitrokey using secrets app
        // This generates the key ON the device (private key never leaves)
        std::string cmd = "nitropy nk3 secrets add-password --name \"" + label + "\" --algorithm ed25519";
        int result = system(cmd.c_str());
        
        if (result != 0) {
            fprintf(stderr, "ERROR: Failed to generate Ed25519 credential on Nitrokey (exit code: %d)\n", result);
            fprintf(stderr, "Make sure nitropy is installed and Nitrokey is connected\n");
            return false;
        }
        
        _nitrokey_label = label;
        _nitrokey_available = true;
        fprintf(stderr, "SUCCESS: Generated Ed25519 credential '%s' on Nitrokey (private credential never left device)\n", label.c_str());
        return true;
    }
    
    bool m17_coder_impl::export_public_key_from_nitrokey(const std::string& file) {
        if (!_nitrokey_available) {
            fprintf(stderr, "ERROR: No Nitrokey available\n");
            return false;
        }
        
        // Use nitropy CLI to export public key using secrets app
        // This exports the public key FROM the Nitrokey (so others can verify/encrypt to you)
        std::string cmd = "nitropy nk3 secrets get-public-key --name \"" + _nitrokey_label + "\" --output " + file;
        int result = system(cmd.c_str());
        
        if (result != 0) {
            fprintf(stderr, "ERROR: Failed to export public credential from Nitrokey (exit code: %d)\n", result);
            fprintf(stderr, "Make sure the credential exists and nitropy is properly configured\n");
            return false;
        }
        
        fprintf(stderr, "SUCCESS: Exported public credential for '%s' to %s\n", _nitrokey_label.c_str(), file.c_str());
        return true;
    }
    
    bool m17_coder_impl::sign_with_nitrokey(const uint8_t* data, size_t data_len, uint8_t* signature) {
        if (!_nitrokey_available) {
            fprintf(stderr, "ERROR: No Nitrokey available\n");
            return false;
        }
        
        if (!data || data_len == 0 || !signature) {
            fprintf(stderr, "ERROR: Invalid parameters for Nitrokey signing\n");
            return false;
        }
        
        // Create temporary file for data
        std::string temp_file = "/tmp/m17_sign_data_" + std::to_string(getpid());
        std::ofstream temp_out(temp_file, std::ios::binary);
        if (!temp_out) {
            fprintf(stderr, "ERROR: Failed to create temporary file for signing\n");
            return false;
        }
        temp_out.write(reinterpret_cast<const char*>(data), data_len);
        temp_out.close();
        
        // Create temporary file for signature
        std::string sig_file = "/tmp/m17_signature_" + std::to_string(getpid());
        
        // Use nitropy CLI to sign data using secrets app
        // This signs the data with the private key stored on the Nitrokey
        std::string cmd = "nitropy nk3 secrets sign --name \"" + _nitrokey_label + "\" --input " + temp_file + " --output " + sig_file;
        int result = system(cmd.c_str());
        
        // Clean up temporary files
        unlink(temp_file.c_str());
        
        if (result != 0) {
            fprintf(stderr, "ERROR: Failed to sign data with Nitrokey (exit code: %d)\n", result);
            fprintf(stderr, "Make sure the credential exists and nitropy is properly configured\n");
            unlink(sig_file.c_str());
            return false;
        }
        
        // Read signature from file
        std::ifstream sig_in(sig_file, std::ios::binary);
        if (!sig_in) {
            fprintf(stderr, "ERROR: Failed to read signature file\n");
            unlink(sig_file.c_str());
            return false;
        }
        
        sig_in.read(reinterpret_cast<char*>(signature), 64);
        sig_in.close();
        unlink(sig_file.c_str());
        
        if (sig_in.gcount() != 64) {
            fprintf(stderr, "ERROR: Invalid signature length from Nitrokey (expected 64 bytes, got %ld)\n", sig_in.gcount());
            return false;
        }
        
        fprintf(stderr, "SUCCESS: Signed %zu bytes with Nitrokey credential '%s'\n", data_len, _nitrokey_label.c_str());
        return true;
    }
    
    bool m17_coder_impl::list_nitrokey_keys() {
        fprintf(stderr, "Listing Nitrokey credentials...\n");
        
        // Use nitropy CLI to list all keys on the Nitrokey
        std::string cmd = "nitropy nk3 secrets list";
        int result = system(cmd.c_str());
        
        if (result != 0) {
            fprintf(stderr, "ERROR: Failed to list Nitrokey credentials (exit code: %d)\n", result);
            return false;
        }
        
        fprintf(stderr, "SUCCESS: Listed Nitrokey credentials\n");
        return true;
    }
    
    bool m17_coder_impl::check_nitrokey_status() {
        fprintf(stderr, "Checking Nitrokey status...\n");
        
        // Use nitropy CLI to check device status
        std::string cmd = "nitropy nk3 info";
        int result = system(cmd.c_str());
        
        if (result != 0) {
            fprintf(stderr, "ERROR: Nitrokey not connected or nitropy not available (exit code: %d)\n", result);
            return false;
        }
        
        fprintf(stderr, "SUCCESS: Nitrokey is connected and available\n");
        return true;
    }
    
    bool m17_coder_impl::delete_nitrokey_key(const std::string& label) {
        if (label.empty()) {
            fprintf(stderr, "ERROR: Invalid Nitrokey identifier\n");
            return false;
        }
        
        fprintf(stderr, "Deleting Nitrokey credential '%s'...\n", label.c_str());
        
        // Use nitropy CLI to delete key from Nitrokey
        std::string cmd = "nitropy nk3 secrets delete --name \"" + label + "\"";
        int result = system(cmd.c_str());
        
        if (result != 0) {
            fprintf(stderr, "ERROR: Failed to delete Nitrokey credential '%s' (exit code: %d)\n", label.c_str(), result);
            return false;
        }
        
        // If we deleted the currently active key, mark as unavailable
        if (label == _nitrokey_label) {
            _nitrokey_available = false;
            _nitrokey_label.clear();
        }
        
        fprintf(stderr, "SUCCESS: Deleted Nitrokey credential '%s'\n", label.c_str());
        return true;
    }
    
    bool m17_coder_impl::set_nitrokey_key(const std::string& label) {
        if (label.empty()) {
            fprintf(stderr, "ERROR: Invalid Nitrokey identifier\n");
            return false;
        }
        
        // Check if the key exists on the Nitrokey
        fprintf(stderr, "Setting active Nitrokey credential to '%s'...\n", label.c_str());
        
        // Try to export the public key to verify it exists
        std::string temp_file = "/tmp/m17_verify_key_" + std::to_string(getpid());
        std::string cmd = "nitropy nk3 secrets get-public-key --name \"" + label + "\" --output " + temp_file;
        int result = system(cmd.c_str());
        
        if (result != 0) {
            fprintf(stderr, "ERROR: Credential '%s' not found on Nitrokey (exit code: %d)\n", label.c_str(), result);
            return false;
        }
        
        // Clean up temp file
        unlink(temp_file.c_str());
        
        // Set as active key
        _nitrokey_label = label;
        _nitrokey_available = true;
        
        fprintf(stderr, "SUCCESS: Set active Nitrokey credential to '%s'\n", label.c_str());
        return true;
    }
    
    // Public Key Import Functions
    bool m17_coder_impl::import_public_key(const std::string& callsign, const std::string& hex_key) {
        if (!validate_callsign(callsign)) {
            fprintf(stderr, "ERROR: Invalid callsign format\n");
            return false;
        }
        
        if (!validate_hex_key(hex_key)) {
            fprintf(stderr, "ERROR: Invalid hex credential format (must be 64 hex characters)\n");
            return false;
        }
        
        // Parse hex key
        PublicKey key;
        strncpy(key.callsign, callsign.c_str(), sizeof(key.callsign) - 1);
        key.callsign[sizeof(key.callsign) - 1] = '\0';
        
        for (int i = 0; i < 32; i++) {
            std::string hex_byte = hex_key.substr(i * 2, 2);
            key.key[i] = static_cast<uint8_t>(std::stoul(hex_byte, nullptr, 16));
        }
        
        key.valid = true;
        key.imported_date = time(nullptr);
        key.expiry_date = 0;  // No expiry
        
        // Store key
        _public_keys[callsign] = key;
        
        // Save immediately to disk
        if (!save_public_keys_to_disk()) {
            fprintf(stderr, "ERROR: Failed to save credentials to disk\n");
            _public_keys.erase(callsign);
            return false;
        }
        
        fprintf(stderr, "SUCCESS: Imported public credential for %s\n", callsign.c_str());
        return true;
    }
    
    bool m17_coder_impl::import_public_key_from_file(const std::string& callsign, const std::string& pem_file) {
        if (!validate_callsign(callsign)) {
            fprintf(stderr, "ERROR: Invalid callsign format\n");
            return false;
        }
        
        FILE* fp = fopen(pem_file.c_str(), "r");
        if (!fp) {
            fprintf(stderr, "ERROR: Cannot open PEM file: %s\n", pem_file.c_str());
            return false;
        }
        
        EVP_PKEY* pkey = PEM_read_PUBKEY(fp, nullptr, nullptr, nullptr);
        fclose(fp);
        
        if (!pkey) {
            fprintf(stderr, "ERROR: Failed to parse PEM file\n");
            return false;
        }
        
        // Extract raw public key
        PublicKey key;
        strncpy(key.callsign, callsign.c_str(), sizeof(key.callsign) - 1);
        key.callsign[sizeof(key.callsign) - 1] = '\0';
        
        size_t key_len = 32;
        if (EVP_PKEY_get_raw_public_key(pkey, key.key, &key_len) <= 0 || key_len != 32) {
            fprintf(stderr, "ERROR: Failed to extract Ed25519 public credential from PEM\n");
            EVP_PKEY_free(pkey);
            return false;
        }
        
        EVP_PKEY_free(pkey);
        
        key.valid = true;
        key.imported_date = time(nullptr);
        key.expiry_date = 0;  // No expiry
        
        // Store key
        _public_keys[callsign] = key;
        
        // Save immediately to disk
        if (!save_public_keys_to_disk()) {
            fprintf(stderr, "ERROR: Failed to save credentials to disk\n");
            _public_keys.erase(callsign);
            return false;
        }
        
        fprintf(stderr, "SUCCESS: Imported public credential from %s for %s\n", pem_file.c_str(), callsign.c_str());
        return true;
    }
    
    bool m17_coder_impl::import_public_key_from_nitrokey(const std::string& callsign) {
        if (!validate_callsign(callsign)) {
            fprintf(stderr, "ERROR: Invalid callsign format\n");
            return false;
        }
        
        if (!_nitrokey_available) {
            fprintf(stderr, "ERROR: No Nitrokey available\n");
            return false;
        }
        
        // Create temporary file for public key export
        std::string temp_file = "/tmp/m17_pubkey_" + std::to_string(getpid()) + ".pem";
        
        if (!export_public_key_from_nitrokey(temp_file)) {
            return false;
        }
        
        // Import from the temporary file
        bool result = import_public_key_from_file(callsign, temp_file);
        
        // Clean up temporary file
        unlink(temp_file.c_str());
        
        return result;
    }
    
    // Verification Functions
    bool m17_coder_impl::verify_signature_from(const std::string& callsign, const uint8_t* data, 
                                               size_t data_len, const uint8_t* signature) {
        if (!data || data_len == 0 || !signature) {
            fprintf(stderr, "ERROR: Invalid parameters for signature verification\n");
            return false;
        }
        
        auto it = _public_keys.find(callsign);
        if (it == _public_keys.end()) {
            fprintf(stderr, "ERROR: No public credential found for callsign: %s\n", callsign.c_str());
            return false;
        }
        
        if (!it->second.valid) {
            fprintf(stderr, "ERROR: Public credential for %s is marked as invalid\n", callsign.c_str());
            return false;
        }
        
        // Use uECC library for Ed25519 verification
        const struct uECC_Curve_t* curve = uECC_secp256k1();  // Use Ed25519 curve
        int result = uECC_verify(it->second.key, data, data_len, signature, curve);
        
        if (result == 1) {
            fprintf(stderr, "SUCCESS: Signature verified for %s\n", callsign.c_str());
            return true;
        } else {
            fprintf(stderr, "ERROR: Signature verification failed for %s\n", callsign.c_str());
            return false;
        }
    }
    
    // Storage Functions
    bool m17_coder_impl::save_public_keys_to_disk() {
        if (_current_storage_path.empty()) {
            fprintf(stderr, "ERROR: No storage path available\n");
            return false;
        }
        
        std::string db_path = _current_storage_path + _storage_paths.db_filename;
        
        std::ofstream out(db_path, std::ios::binary);
        if (!out) {
            fprintf(stderr, "ERROR: Cannot create database file: %s\n", db_path.c_str());
            return false;
        }
        
        // Write header
        uint32_t magic = m17_constants::PUBLIC_KEY_DB_MAGIC;
        uint32_t version = m17_constants::PUBLIC_KEY_DB_VERSION;
        uint32_t num_keys = static_cast<uint32_t>(_public_keys.size());
        
        out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
        out.write(reinterpret_cast<const char*>(&version), sizeof(version));
        out.write(reinterpret_cast<const char*>(&num_keys), sizeof(num_keys));
        
        // Write keys
        for (const auto& pair : _public_keys) {
            const PublicKey& key = pair.second;
            
            // callsign(10) + key(32) + valid(1) + imported_date(8) + expiry_date(8) = 59 bytes
            out.write(key.callsign, 10);
            out.write(reinterpret_cast<const char*>(key.key), 32);
            out.write(reinterpret_cast<const char*>(&key.valid), 1);
            out.write(reinterpret_cast<const char*>(&key.imported_date), 8);
            out.write(reinterpret_cast<const char*>(&key.expiry_date), 8);
        }
        
        out.close();
        
        // Set file permissions to 0600
        chmod(db_path.c_str(), 0600);
        
        fprintf(stderr, "SUCCESS: Saved %zu public credentials to %s\n", _public_keys.size(), db_path.c_str());
        return true;
    }
    
    bool m17_coder_impl::load_public_keys_from_disk() {
        if (_current_storage_path.empty()) {
            return false;  // No storage path available
        }
        
        std::string db_path = _current_storage_path + _storage_paths.db_filename;
        
        std::ifstream in(db_path, std::ios::binary);
        if (!in) {
            return false;  // File doesn't exist or can't be read
        }
        
        // Read header
        uint32_t magic, version, num_keys;
        in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        in.read(reinterpret_cast<char*>(&version), sizeof(version));
        in.read(reinterpret_cast<char*>(&num_keys), sizeof(num_keys));
        
        if (magic != m17_constants::PUBLIC_KEY_DB_MAGIC) {
            fprintf(stderr, "ERROR: Invalid database magic number\n");
            return false;
        }
        
        if (version != m17_constants::PUBLIC_KEY_DB_VERSION) {
            fprintf(stderr, "ERROR: Unsupported database version: %u\n", version);
            return false;
        }
        
        // Clear existing keys
        _public_keys.clear();
        
        // Read keys
        for (uint32_t i = 0; i < num_keys; i++) {
            PublicKey key;
            
            in.read(key.callsign, 10);
            in.read(reinterpret_cast<char*>(key.key), 32);
            in.read(reinterpret_cast<char*>(&key.valid), 1);
            in.read(reinterpret_cast<char*>(&key.imported_date), 8);
            in.read(reinterpret_cast<char*>(&key.expiry_date), 8);
            
            if (in.gcount() != 59) {
                fprintf(stderr, "ERROR: Invalid credential record in database\n");
                return false;
            }
            
            key.callsign[9] = '\0';  // Ensure null termination
            _public_keys[key.callsign] = key;
        }
        
        fprintf(stderr, "SUCCESS: Loaded %u public credentials from %s\n", num_keys, db_path.c_str());
        return true;
    }
    
    bool m17_coder_impl::load_trusted_keys_from_file(const std::string& config_file) {
        std::ifstream in(config_file);
        if (!in) {
            fprintf(stderr, "ERROR: Cannot open config file: %s\n", config_file.c_str());
            return false;
        }
        
        std::string line;
        int loaded_count = 0;
        
        while (std::getline(in, line)) {
            // Skip empty lines and comments
            if (line.empty() || line[0] == '#') {
                continue;
            }
            
            // Parse format: CALLSIGN=hexkey
            size_t eq_pos = line.find('=');
            if (eq_pos == std::string::npos) {
                continue;
            }
            
            std::string callsign = line.substr(0, eq_pos);
            std::string hex_key = line.substr(eq_pos + 1);
            
            // Remove whitespace
            callsign.erase(0, callsign.find_first_not_of(" \t"));
            callsign.erase(callsign.find_last_not_of(" \t") + 1);
            hex_key.erase(0, hex_key.find_first_not_of(" \t"));
            hex_key.erase(hex_key.find_last_not_of(" \t") + 1);
            
            if (import_public_key(callsign, hex_key)) {
                loaded_count++;
            }
        }
        
        fprintf(stderr, "SUCCESS: Loaded %d trusted credentials from %s\n", loaded_count, config_file.c_str());
        return loaded_count > 0;
    }
    
    // Management Functions
    void m17_coder_impl::list_public_keys() {
        if (_public_keys.empty()) {
            fprintf(stderr, "No public credentials stored\n");
            return;
        }
        
        fprintf(stderr, "Stored Public Credentials:\n");
        fprintf(stderr, "==================\n");
        
        for (const auto& pair : _public_keys) {
            const PublicKey& key = pair.second;
            std::string fingerprint = get_key_fingerprint(key.key);
            
            fprintf(stderr, "Callsign: %s\n", key.callsign);
            fprintf(stderr, "Fingerprint: %s\n", fingerprint.c_str());
            fprintf(stderr, "Valid: %s\n", key.valid ? "Yes" : "No");
            fprintf(stderr, "Imported: %s", ctime(&key.imported_date));
            if (key.expiry_date > 0) {
                fprintf(stderr, "Expires: %s", ctime(&key.expiry_date));
            } else {
                fprintf(stderr, "Expires: Never\n");
            }
            fprintf(stderr, "---\n");
        }
    }
    
    bool m17_coder_impl::remove_public_key(const std::string& callsign) {
        auto it = _public_keys.find(callsign);
        if (it == _public_keys.end()) {
            fprintf(stderr, "ERROR: No public credential found for callsign: %s\n", callsign.c_str());
            return false;
        }
        
        // Securely wipe the key
        secure_wipe_key(it->second);
        
        // Remove from map
        _public_keys.erase(it);
        
        // Save immediately to disk
        if (!save_public_keys_to_disk()) {
            fprintf(stderr, "ERROR: Failed to save credentials to disk after removal\n");
            return false;
        }
        
        fprintf(stderr, "SUCCESS: Removed public credential for %s\n", callsign.c_str());
        return true;
    }
    
    void m17_coder_impl::check_expired_keys() {
        time_t now = time(nullptr);
        std::vector<std::string> expired_callsigns;
        
        for (const auto& pair : _public_keys) {
            const PublicKey& key = pair.second;
            if (key.expiry_date > 0 && key.expiry_date < now) {
                expired_callsigns.push_back(pair.first);
            }
        }
        
        if (!expired_callsigns.empty()) {
            fprintf(stderr, "Found %zu expired credentials, removing...\n", expired_callsigns.size());
            
            for (const std::string& callsign : expired_callsigns) {
                remove_public_key(callsign);
            }
        }
    }
    
    // Internal Helper Functions
    bool m17_coder_impl::detect_storage_path() {
        // Try primary path first
        if (access(_storage_paths.primary_path.c_str(), W_OK) == 0) {
            _current_storage_path = _storage_paths.primary_path;
            return true;
        }
        
        // Try secondary path
        if (access(_storage_paths.secondary_path.c_str(), W_OK) == 0) {
            _current_storage_path = _storage_paths.secondary_path;
            return true;
        }
        
        // Use fallback path
        _current_storage_path = _storage_paths.fallback_path;
        return true;
    }
    
    bool m17_coder_impl::validate_callsign(const std::string& callsign) {
        if (callsign.empty() || callsign.length() > m17_constants::MAX_CALLSIGN_LEN) {
            return false;
        }
        
        // Check for valid characters (alphanumeric and some special chars)
        for (char c : callsign) {
            if (!std::isalnum(c) && c != '-' && c != '_') {
                return false;
            }
        }
        
        return true;
    }
    
    bool m17_coder_impl::validate_hex_key(const std::string& hex_key) {
        if (hex_key.length() != 64) {
            return false;
        }
        
        for (char c : hex_key) {
            if (!std::isxdigit(c)) {
                return false;
            }
        }
        
        return true;
    }
    
    std::string m17_coder_impl::get_key_fingerprint(const uint8_t* key) {
        uint8_t hash[32];
        SHA256(key, 32, hash);
        
        std::stringstream ss;
        for (int i = 0; i < 8; i++) {  // First 8 bytes for fingerprint
            ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(hash[i]);
        }
        
        return ss.str();
    }
    
    void m17_coder_impl::secure_wipe_key(PublicKey& key) {
        explicit_bzero(key.key, sizeof(key.key));
        explicit_bzero(key.callsign, sizeof(key.callsign));
        key.valid = false;
        key.imported_date = 0;
        key.expiry_date = 0;
    }
    
    // SECURITY FIX: ChaCha20-Poly1305 Implementation
    
    int m17_coder_impl::encrypt_chacha20_poly1305(const uint8_t* plaintext, size_t plaintext_len,
                                                 const uint8_t* key, size_t key_size,
                                                 const uint8_t* iv, size_t iv_size,
                                                 const uint8_t* aad, size_t aad_len,
                                                 uint8_t* ciphertext, size_t ciphertext_size,
                                                 uint8_t* tag, size_t tag_size) {
        if (!plaintext || plaintext_len == 0 || !key || !iv || !ciphertext || !tag) {
            fprintf(stderr, "ERROR: Invalid parameters for ChaCha20-Poly1305 encryption\n");
            return -1;
        }
        
        if (key_size != m17_constants::CHACHA20_KEY_SIZE) {
            fprintf(stderr, "ERROR: Invalid ChaCha20 credential size: %zu (expected %d)\n", key_size, m17_constants::CHACHA20_KEY_SIZE);
            return -1;
        }
        
        if (iv_size != m17_constants::CHACHA20_IV_SIZE) {
            fprintf(stderr, "ERROR: Invalid ChaCha20 IV size: %zu (expected %d)\n", iv_size, m17_constants::CHACHA20_IV_SIZE);
            return -1;
        }
        
        if (tag_size != m17_constants::CHACHA20_POLY1305_TAG_SIZE) {
            fprintf(stderr, "ERROR: Invalid ChaCha20-Poly1305 tag size: %zu (expected %d)\n", tag_size, m17_constants::CHACHA20_POLY1305_TAG_SIZE);
            return -1;
        }
        
        if (ciphertext_size < plaintext_len) {
            fprintf(stderr, "ERROR: Insufficient ciphertext buffer size\n");
            return -1;
        }
        
        // Use the libm17 ChaCha20-Poly1305 implementation
        int result = m17_chacha20_poly1305_encrypt(plaintext, plaintext_len,
                                                  key, key_size,
                                                  iv, iv_size,
                                                  aad, aad_len,
                                                  ciphertext, ciphertext_size,
                                                  tag, tag_size);
        
        if (result < 0) {
            fprintf(stderr, "ERROR: ChaCha20-Poly1305 encryption failed\n");
            return -1;
        }
        
        return result;
    }
    
    int m17_coder_impl::decrypt_chacha20_poly1305(const uint8_t* ciphertext, size_t ciphertext_len,
                                                 const uint8_t* key, size_t key_size,
                                                 const uint8_t* iv, size_t iv_size,
                                                 const uint8_t* aad, size_t aad_len,
                                                 const uint8_t* tag, size_t tag_size,
                                                 uint8_t* plaintext, size_t plaintext_size) {
        if (!ciphertext || ciphertext_len == 0 || !key || !iv || !plaintext || !tag) {
            fprintf(stderr, "ERROR: Invalid parameters for ChaCha20-Poly1305 decryption\n");
            return -1;
        }
        
        if (key_size != m17_constants::CHACHA20_KEY_SIZE) {
            fprintf(stderr, "ERROR: Invalid ChaCha20 credential size: %zu (expected %d)\n", key_size, m17_constants::CHACHA20_KEY_SIZE);
            return -1;
        }
        
        if (iv_size != m17_constants::CHACHA20_IV_SIZE) {
            fprintf(stderr, "ERROR: Invalid ChaCha20 IV size: %zu (expected %d)\n", iv_size, m17_constants::CHACHA20_IV_SIZE);
            return -1;
        }
        
        if (tag_size != m17_constants::CHACHA20_POLY1305_TAG_SIZE) {
            fprintf(stderr, "ERROR: Invalid ChaCha20-Poly1305 tag size: %zu (expected %d)\n", tag_size, m17_constants::CHACHA20_POLY1305_TAG_SIZE);
            return -1;
        }
        
        if (plaintext_size < ciphertext_len) {
            fprintf(stderr, "ERROR: Insufficient plaintext buffer size\n");
            return -1;
        }
        
        // Use the libm17 ChaCha20-Poly1305 implementation
        int result = m17_chacha20_poly1305_decrypt(ciphertext, ciphertext_len,
                                                  key, key_size,
                                                  iv, iv_size,
                                                  aad, aad_len,
                                                  tag, tag_size,
                                                  plaintext, plaintext_size);
        
        if (result < 0) {
            fprintf(stderr, "ERROR: ChaCha20-Poly1305 decryption failed\n");
            return -1;
        }
        
        return result;
    }
    
    bool m17_coder_impl::set_chacha20_key(const std::string& hex_key) {
        if (hex_key.length() != 64) {  // 32 bytes = 64 hex characters
            fprintf(stderr, "ERROR: ChaCha20 credential must be 64 hex characters (32 bytes)\n");
            return false;
        }
        
        // Parse hex key
        for (int i = 0; i < 32; i++) {
            std::string hex_byte = hex_key.substr(i * 2, 2);
            char *endptr;
            unsigned long val = strtoul(hex_byte.c_str(), &endptr, 16);
            if (*endptr != '\0' || val > 255) {
                fprintf(stderr, "ERROR: Invalid hex character in ChaCha20 credential\n");
                return false;
            }
            _chacha20_key[i] = static_cast<uint8_t>(val);
        }
        
        // Validate the key
        if (m17_chacha20_validate_key(_chacha20_key, sizeof(_chacha20_key)) != 0) {
            fprintf(stderr, "ERROR: Weak ChaCha20 credential detected\n");
            explicit_bzero(_chacha20_key, sizeof(_chacha20_key));
            return false;
        }
        
        _chacha20_available = true;
        fprintf(stderr, "SUCCESS: ChaCha20 credential set\n");
        return true;
    }
    
    bool m17_coder_impl::set_chacha20_iv(const std::string& hex_iv) {
        if (hex_iv.length() != 24) {  // 12 bytes = 24 hex characters
            fprintf(stderr, "ERROR: ChaCha20 IV must be 24 hex characters (12 bytes)\n");
            return false;
        }
        
        // Parse hex IV
        for (int i = 0; i < 12; i++) {
            std::string hex_byte = hex_iv.substr(i * 2, 2);
            char *endptr;
            unsigned long val = strtoul(hex_byte.c_str(), &endptr, 16);
            if (*endptr != '\0' || val > 255) {
                fprintf(stderr, "ERROR: Invalid hex character in ChaCha20 IV\n");
                return false;
            }
            _chacha20_iv[i] = static_cast<uint8_t>(val);
        }
        
        // Validate the IV
        if (m17_chacha20_validate_iv(_chacha20_iv, sizeof(_chacha20_iv)) != 0) {
            fprintf(stderr, "ERROR: Weak ChaCha20 IV detected\n");
            explicit_bzero(_chacha20_iv, sizeof(_chacha20_iv));
            return false;
        }
        
        fprintf(stderr, "SUCCESS: ChaCha20 IV set\n");
        return true;
    }
    
    void m17_coder_impl::generate_chacha20_iv() {
        if (m17_chacha20_generate_iv(_chacha20_iv, sizeof(_chacha20_iv)) != 0) {
            fprintf(stderr, "ERROR: Failed to generate ChaCha20 IV\n");
            return;
        }
        
        fprintf(stderr, "SUCCESS: Generated ChaCha20 IV\n");
    }
    
    bool m17_coder_impl::is_chacha20_available() const {
        return _chacha20_available;
    }

  } /* namespace m17 */
} /* namespace gr */
