/* -*- c++ -*- */
/*
 * Copyright 2023 jmfriedt.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_M17_M17_DECODER_IMPL_H
#define INCLUDED_M17_M17_DECODER_IMPL_H

#include <gnuradio/m17/m17_decoder.h>
#include "m17.h"

#define AES
#define ECC

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

    class m17_decoder_impl:public m17_decoder
    {
    private:
      bool _debug_data = false;
      bool _debug_ctrl = false;
      float _threshold = 0.9;
      bool _callsign = false;
      bool _signed_str = false;
      // uint8_t _key[32];
      uint8_t _key[64] = { 0 };	//public key
      uint8_t _iv[16];
      encr_t _encr_type = ENCR_NONE;
//used for signatures
      uint8_t _digest[16] = { 0 };	//16-byte field for the stream digest
      uint8_t _sig[64] = { 0 };	//ECDSA signature
      uint32_t _scrambler_key = 0;	//keep set to initial value for seed calculation function

#ifdef AES
      typedef enum
      {
	AES128,
	AES192,
	AES256
      } aes_t;
      int8_t _aes_subtype = -1;
#endif

      float last[8] = { 0 };	//look-back buffer for finding syncwords
      float _pld[SYM_PER_PLD];	//raw frame symbols
      uint16_t soft_bit[2 * SYM_PER_PLD];	//raw frame soft bits
      uint16_t d_soft_bit[2 * SYM_PER_PLD];	//deinterleaved soft bits
      uint16_t _expected_next_fn;
      uint16_t _fn;

      lsf_t _lsf;	//complete LSF (one byte extra needed for the Viterbi decoder)
      uint16_t lich_chunk[96];	//raw, soft LSF chunk extracted from the LICH
      uint8_t _lich_b[6];	//48-bit decoded LICH
      uint8_t _lich_cnt;		//LICH_CNT
      uint8_t lich_chunks_rcvd = 0;	//flags set for each LSF chunk received

      uint16_t enc_data[272];	//raw frame data soft bits
      uint8_t _frame_data[19];	//decoded frame data, 144 bits (16+128), plus 4 flushing bits
      uint8_t digest[16] = { 0 };

      uint8_t syncd = 0;	//syncword found?
      uint8_t fl = 0;		//Frame=0 of LSF=1
      uint8_t pushed;		//counter for pushed symbols

      uint8_t d_dst[12], d_src[12];	//decoded strings
#ifdef ECC
//Scrambler
      uint8_t _seed[3]; //24-bit is the largest seed value
      uint8_t _scr_bytes[16];
      uint8_t _scrambler_pn[128];
      uint32_t _scrambler_seed = 0;
      int8_t _scrambler_subtype = -1;
#endif
      const struct uECC_Curve_t *_curve = uECC_secp256r1 ();

    public:
      m17_decoder_impl (bool debug_data, bool debug_ctrl, float threshold,
			bool callsign, bool signed_str, int encr_type,
			std::string key, std::string seed);
      ~m17_decoder_impl ();
      void set_debug_data (bool debug);
      void set_key (std::string arg);
      void set_seed (std::string seed);
      void set_debug_ctrl (bool debug);
      void set_callsign (bool callsign);
      void set_threshold (float threshold);
      void set_signed (bool signed_str);
      void set_encr_type (int encr_type);
      void parse_raw_key_string (uint8_t * dest, const char *inp);
      void scrambler_sequence_generator ();
      uint32_t scrambler_seed_calculation (int8_t subtype, uint32_t key,
					   int fn);

      // Where all the action really happens
      void forecast (int noutput_items,
		     gr_vector_int & ninput_items_required);

      int general_work (int noutput_items,
			gr_vector_int & ninput_items,
			gr_vector_const_void_star & input_items,
			gr_vector_void_star & output_items);
    };

  }				// namespace m17
}				// namespace gr

#endif /* INCLUDED_M17_M17_DECODER_IMPL_H */
