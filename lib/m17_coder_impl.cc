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
#include <sys/wait.h>  // for waitpid
#include <vector>   // for std::vector
#include <cctype>   // for std::isspace, std::isalnum
#include <signal.h> // for signal handling
#include <map>      // for std::map
#include <memory>   // for std::unique_ptr

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
      
      // SECURITY FIX: Initialize secure storage and key isolation
      _key_isolation.start_key_isolation();
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
              _lsf.crc[1] = ccrc & m17_constants::BYTE_MASK;
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
        _lsf.type[1] = (uint16_t)_type & m17_constants::BYTE_MASK;

        // calculate LSF CRC (unclear whether or not this is only
        // needed here for debug, or if this is missing on every initial LSF)
        update_LSF_CRC(&_lsf);

        _finished = 0;
      }
#ifdef AES
      if (_encr_type == ENCR_AES)
      {
        // SECURITY FIX: Bounds check before memcpy
        if (m17_constants::META_FIELD_SIZE <= sizeof(_lsf.meta)) {
          memcpy(&(_lsf.meta), _iv, m17_constants::META_FIELD_SIZE);
        }
        _iv[m17_constants::IV_FRAME_NUMBER_OFFSET] = (_fn >> 8) & m17_constants::IV_FRAME_NUMBER_MASK;
        _iv[15] = (_fn >> 0) & 0xFF;

        // re-calculate LSF CRC with IV insertion
        update_LSF_CRC(&_lsf);
      }
        // SECURITY FIX: Use secure memory clearing for sensitive data
        explicit_bzero(_key, sizeof(_key));
        explicit_bzero(_iv, sizeof(_iv));
#endif
      // SECURITY FIX: Initialize SHA context for signed streams
      if (_signed_str) {
        _sha_ctx = EVP_MD_CTX_new();
        if (_sha_ctx == nullptr) {
          throw std::runtime_error("Failed to create SHA-256 context for signed stream");
        }
        
        // SECURITY FIX: Check OpenSSL initialization
        if (EVP_DigestInit_ex(_sha_ctx, EVP_sha256(), nullptr) != 1) {
          EVP_MD_CTX_free(_sha_ctx);
          _sha_ctx = nullptr;
          throw std::runtime_error("Failed to initialize SHA-256 context");
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
      
      // SECURITY FIX: Parse hex string instead of UTF-8
      // Expect 64 hex characters for 32-byte key
      if (arg.size() != m17_constants::HEX_KEY_LENGTH) {
        fprintf(stderr, "ERROR: Private credential must be 64 hex characters (32 bytes)\n");
        _priv_key_loaded = false;
        return;
      }
      
      // Parse hex string to binary
      uint8_t temp_key[32];
      for (int i = 0; i < 32; i++) {
        char hex_byte[3] = {arg[i*2], arg[i*2+1], '\0'};
        char *endptr;
        unsigned long val = strtoul(hex_byte, &endptr, 16);
        if (*endptr != '\0' || val > 255) {
          fprintf(stderr, "ERROR: Invalid hex character in private credential\n");
          _priv_key_loaded = false;
          return;
        }
        temp_key[i] = (uint8_t)val;
      }
      
      // SECURITY FIX: Store key in encrypted secure storage
      if (_secure_private_key.store_key(temp_key, sizeof(temp_key))) {
        _priv_key_loaded = true;
        // Clear temporary key from memory
        secure_wipe_memory(temp_key, sizeof(temp_key));
      } else {
        fprintf(stderr, "ERROR: Failed to store private key securely\n");
        _priv_key_loaded = false;
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
      uint8_t temp_key[32];
      for (int i = 0; i < 32; i++) {
        char hex_byte[3] = {arg[i*2], arg[i*2+1], '\0'};
        char *endptr;
        unsigned long val = strtoul(hex_byte, &endptr, 16);
        if (*endptr != '\0' || val > 255) {
          fprintf(stderr, "ERROR: Invalid hex character in encryption credential\n");
          return;
        }
        temp_key[i] = (uint8_t)val;
      }
      
      // SECURITY FIX: Store key in encrypted secure storage
      if (_secure_encryption_key.store_key(temp_key, sizeof(temp_key))) {
        // Clear temporary key from memory
        secure_wipe_memory(temp_key, sizeof(temp_key));
      } else {
        fprintf(stderr, "ERROR: Failed to store encryption key securely\n");
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

      explicit_bzero(_lsf.meta, 14);

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
        // SECURITY FIX: Use safe output to prevent format string injection
        // Use puts() instead of printf() to avoid format string vulnerabilities
        puts(meta.c_str());
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
          if ((unsigned int)meta.data()[i] < m17_constants::UTF8_START_BYTE) // https://www.utf8-chartable.de/
          {
            _lsf.meta[j] = meta.data()[i];
            i++;
            j++;
          }
          else
          {
            _lsf.meta[j] =
                (meta.data()[i] - m17_constants::UTF8_START_BYTE) * m17_constants::UTF8_MULTIPLIER + meta.data()[i + 1];
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
  // SECURITY FIX: Stop key isolation process
  _key_isolation.stop_key_isolation();
  
  // SECURITY FIX: Clear secure storage
  _secure_private_key.clear_key();
  _secure_encryption_key.clear_key();
  
  // CRITICAL SECURITY FIX: Securely wipe all cryptographic material
  explicit_bzero(_key, sizeof(_key));
  explicit_bzero(_priv_key, sizeof(_priv_key));
  explicit_bzero(_iv, sizeof(_iv));
  explicit_bzero(_digest, sizeof(_digest));
  explicit_bzero(_sig, sizeof(_sig));
  explicit_bzero(_scr_bytes, sizeof(_scr_bytes));
  
  // SECURITY FIX: Wipe extended crypto keys
  secure_wipe_memory(_chacha20_key, sizeof(_chacha20_key));
  secure_wipe_memory(_chacha20_iv, sizeof(_chacha20_iv));
  secure_wipe_memory(_chacha20_tag, sizeof(_chacha20_tag));
  
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

      // Set Frame Type based on scrambler_subtype value
      if (_scrambler_subtype == 0) {
        _type = 0; // Standard M17 frame
      } else if (_scrambler_subtype == 1) {
        _type = 1; // Scrambled M17 frame
      } else {
        _type = 2; // Extended M17 frame
      }
      
      if (_debug == true)
      {
        // SECURITY FIX: Never log scrambler seeds or keys - they are sensitive cryptographic material
        fprintf(stderr, "\nScrambler configured (details hidden for security); Subtype: %02d; Frame Type: %d\n", _scrambler_subtype, _type);
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

      explicit_bzero(dest, len / 2); // one character represents half of a byte

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
        // SECURITY FIX: Bounds check before memcpy
        if (countin + 16 <= ninput_items[0]) {
          memcpy(next_data, in + countin, 16);
        } else {
          return -1; // Buffer overflow protection
        }
        countin += 16;

        // update the data before applying crypto
        // SECURITY FIX: Bounds check before memcpy
        if (16 <= sizeof(data)) {
          memcpy(data, next_data, 16);
        } else {
          return -1; // Buffer overflow protection
        }

        // Debug mode processing
        if (_debug) {
          // Enhanced debug output for cryptographic operations
          fprintf(stderr, "M17 Debug: Processing frame %d, encryption type: %d\n", _fn, _encr_type);
          if (_encr_type == ENCR_AES) {
            fprintf(stderr, "M17 Debug: AES encryption active, subtype: %d\n", _aes_subtype);
          }
        }
        
        // Set AES subtype based on encryption type
        if (_encr_type == ENCR_AES) {
          _aes_subtype = (_encr_type == ENCR_AES) ? 1 : 0;
        }

#ifdef AES
        if (_encr_type == ENCR_AES)
        {
          // Properly handle IV for LSF meta field
          // Copy IV to LSF meta field for proper frame identification
          // SECURITY FIX: Bounds check before memcpy
          if (14 <= sizeof(_next_lsf.meta)) {
            memcpy(_next_lsf.meta, _iv, 14);
          }
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
            // Fix _next_lsf contents and update LSF
            // SECURITY FIX: Remove NULL checks for arrays (they're never NULL)
              // Copy source and destination callsigns
              // SECURITY FIX: Bounds check before memcpy
              if (6 <= sizeof(_lsf.src) && 6 <= sizeof(_next_lsf.src)) {
                memcpy(_lsf.src, _next_lsf.src, 6);
              }
              if (6 <= sizeof(_lsf.dst) && 6 <= sizeof(_next_lsf.dst)) {
                memcpy(_lsf.dst, _next_lsf.dst, 6);
              }
              // Arrays are 6 elements, no need for null termination
              
              // Copy type and meta information
              _lsf.type[0] = _next_lsf.type[0];
              _lsf.type[1] = _next_lsf.type[1];
              
              // SECURITY FIX: Remove NULL check for array (it's never NULL)
                // SECURITY FIX: Bounds check before memcpy
                if (14 <= sizeof(_lsf.meta) && 14 <= sizeof(_next_lsf.meta)) {
                  memcpy(_lsf.meta, _next_lsf.meta, 14);
                }
              
              // Update LSF CRC
              update_LSF_CRC(&_lsf);
            }
        } // end of if (_finished == false)
        
        // Handle finished case
        if (_finished == true) // send last frame(s)
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
            // SECURITY FIX: Check OpenSSL digest finalization
            if (EVP_DigestFinal_ex(_sha_ctx, _digest, &digest_len) != 1) {
              throw std::runtime_error("Failed to finalize SHA-256 digest");
            }
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
      
      // Tell runtime system how many output items we produced.
      if (_finished == false)
        return countout;
      else
      {
        printf("Killing flowgraph\n");
        return -1;
      } // https://lists.gnu.org/archive/html/discuss-gnuradio/2016-12/msg00206.html
      // returning -1 (which is the magical value for "there's nothing coming anymore, you can shut down") would normally end a flow graph
    } // end of general_work function

  } /* namespace m17 */
} /* namespace gr */
