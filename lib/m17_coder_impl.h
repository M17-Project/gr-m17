/* -*- c++ -*- */
/*
 * Copyright 2023 jmfriedt.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_M17_M17_CODER_IMPL_H
#define INCLUDED_M17_M17_CODER_IMPL_H

#include <atomic>
#include <gnuradio/m17/m17_coder.h>

#include "m17.h" // lsf_t declaration
#include "aes.h"
#include "uECC.h"

namespace gr
{
  namespace m17
  {

    class m17_coder_impl : public m17_coder
    {
    private:
      // M17
      static constexpr int PAYLOAD_BYTES = 16;

      unsigned char _src_id[10], _dst_id[10]; // 9 character callsign
      int _mode, _data;
      uint16_t _type;
      uint16_t _send_preamble;
      int _encr_subtype;

      // encryption
      int8_t _aes_subtype = -1;
      const struct uECC_Curve_t *_curve = uECC_secp256r1();
      encr_t _encr_type = ENCR_NONE;

      // AES
      typedef enum
      {
        AES128,
        AES192,
        AES256
      } aes_t;
      uint8_t _key[32];
      uint8_t _iv[PAYLOAD_BYTES];
      const time_t epoch = 1577836800L; // Jan 1, 2020, 00:00:00 UTC

      int _can;
      lsf_t _lsf, _next_lsf;
      std::string _meta;
      int _got_lsf = 0;
      uint16_t _fn = 0;      // 16-bit Frame Number (for the stream mode)
      uint8_t _lich_cnt = 0; // 0..5 LICH counter, derived from the Frame Number
      bool _debug = 0;
      bool _signed_str = false;
      bool _finalizing = false;
      std::atomic<bool> _finished = false, _active = false;

      uint8_t _digest[PAYLOAD_BYTES] = {0};     // 16-byte field for the stream digest
      bool _priv_key_loaded = false;            // do we have a sig key loaded?
      uint8_t _priv_key[32] = {0};              // private key
      uint8_t _sig[64] = {0};                   // ECDSA signature
      int _eot_cnt = 1;
      bool _init_frame;

      // Scrambler
      uint8_t _seed[3]; // 24-bit is the largest seed value
      uint8_t _scr_bytes[PAYLOAD_BYTES];
      uint8_t _scrambler_pn[128];
      uint32_t _scrambler_seed = 0;
      int8_t _scrambler_subtype = -1;

    public:
      void parse_raw_key_string(uint8_t *, const char *);
      void scrambler_sequence_generator();
      void set_src_id(std::string src_id);
      void set_dst_id(std::string dst_id);
      void set_key(std::string key);
      void set_priv_key(std::string key);
      void set_meta(std::string meta);
      void set_seed(std::string seed);
      void set_eot_cnt(int eot_cnt);
      void set_type(int mode, int data, encr_t encr_type, int encr_subtype,
                    int can);
      void set_mode(int mode);
      void set_data(int data);
      void set_encr_type(int encr_type);
      void set_encr_subtype(int encr_subtype);
      void set_aes_subtype(int aes_subtype, int encr_type);
      void set_can(int can);
      void set_debug(bool debug);
      void set_signed(bool signed_str);
      void switch_state(const pmt::pmt_t &msg);
      void init_state(void);

      m17_coder_impl(std::string src_id, std::string dst_id, int mode,
                     int data, int encr_type, int encr_subtype, int aes_subtype, int can,
                     std::string meta, std::string key, std::string priv_key,
                     bool debug, bool signed_str, std::string seed, int eot_cnt);
      ~m17_coder_impl();

      // Where all the action really happens
      void forecast(int noutput_items,
                    gr_vector_int &ninput_items_required);

      int general_work(int noutput_items,
                       gr_vector_int &ninput_items,
                       gr_vector_const_void_star &input_items,
                       gr_vector_void_star &output_items);
    };

  } // namespace m17
} // namespace gr

#endif /* INCLUDED_M17_M17_CODER_IMPL_H */
