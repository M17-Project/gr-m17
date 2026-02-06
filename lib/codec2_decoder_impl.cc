/* -*- c++ -*- */
/*
 * Copyright 2026 wkaczmarski.
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
#include "codec2_decoder_impl.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

namespace gr
{
    namespace m17
    {
        codec2_decoder::sptr
        codec2_decoder::make()
        {
            return gnuradio::get_initial_sptr(new codec2_decoder_impl());
        }

        /*
         * The private constructor
         */
        codec2_decoder_impl::codec2_decoder_impl() : gr::block("codec2_decoder", gr::io_signature::make(1, 1, sizeof(uint8_t)),
                                                               gr::io_signature::make(1, 1, sizeof(int16_t)))
        {
            init_state();

        }

        void codec2_decoder_impl::init_state(void)
        {
            codec2_init(&c2);
        }

        /*
         * Our virtual destructor.
         */
        codec2_decoder_impl::~codec2_decoder_impl()
        {
        }

        void
        codec2_decoder_impl::forecast(int noutput_items,
                                      gr_vector_int &ninput_items_required)
        {
            ninput_items_required[0] = CODEC2_BYTES_PER_FRAME;
        }

        int
        codec2_decoder_impl::general_work(int noutput_items,
                                          gr_vector_int &ninput_items,
                                          gr_vector_const_void_star &input_items,
                                          gr_vector_void_star &output_items)
        {
            const uint8_t *bits = static_cast<const uint8_t *>(input_items[0]);

            int16_t *speech = static_cast<int16_t *>(output_items[0]);

            if (ninput_items[0] < CODEC2_BYTES_PER_FRAME)
                return 0;

            if (noutput_items < CODEC2_SAMPLES_PER_FRAME)
                return 0;

            codec2_decode(&c2, speech, bits);

            consume_each(CODEC2_BYTES_PER_FRAME);

            return CODEC2_SAMPLES_PER_FRAME;
        }
    } /* namespace m17 */
} /* namespace gr */
