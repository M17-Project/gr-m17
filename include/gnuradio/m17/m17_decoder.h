/* -*- c++ -*- */
/*
 * Copyright 2023 jmfriedt.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_M17_M17_DECODER_H
#define INCLUDED_M17_M17_DECODER_H

#include <gnuradio/block.h>
#include <gnuradio/m17/api.h>

namespace gr
{
  namespace m17
  {

/*!
 * \brief <+description of block+>
 * \ingroup m17
 *
 */
    class M17_API m17_decoder:virtual public gr::block
    {
    public:
      typedef std::shared_ptr < m17_decoder > sptr;
      typedef enum
      {
	ENCR_NONE,
	ENCR_SCRAM,
	ENCR_AES,
	ENCR_RES		//reserved
      } encr_t;

      /*!
       * \brief Return a shared_ptr to a new instance of m17::m17_decoder.
       *
       * To avoid accidental use of raw pointers, m17::m17_decoder's
       * constructor is in a private implementation
       * class. m17::m17_decoder::make is the public interface for
       * creating new instances.
       */
      static sptr make (bool debug_data, bool debug_ctrl, float threshold,
			bool callsign, bool signed_str, int encr_type,
			std::string key, std::string seed);
      virtual void set_debug_data (bool debug) = 0;
      virtual void set_debug_ctrl (bool debug) = 0;
      virtual void set_callsign (bool callsign) = 0;
      virtual void set_threshold (float threshold) = 0;
      virtual void set_signed (bool signed_str) = 0;
      virtual void set_key (std::string key) = 0;
      virtual void set_seed (std::string seed) = 0;
      virtual void parse_raw_key_string (uint8_t * dest, const char *inp) = 0;
      virtual void scrambler_sequence_generator () = 0;
      virtual uint32_t scrambler_seed_calculation (int8_t subtype,
						   uint32_t key, int fn) = 0;

    };

  }				// namespace m17
}				// namespace gr

#endif				/* INCLUDED_M17_M17_DECODER_H */
