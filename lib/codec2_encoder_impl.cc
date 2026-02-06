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
#include "codec2_encoder_impl.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

namespace gr
{
    namespace m17
    {
        codec2_encoder::sptr
        codec2_encoder::make()
        {
            return gnuradio::get_initial_sptr(new codec2_encoder_impl());
        }

        /*
         * The private constructor
         */
        codec2_encoder_impl::codec2_encoder_impl() : gr::block("codec2_encoder", gr::io_signature::make(1, 1, sizeof(int16_t)),
                                                               gr::io_signature::make(1, 1, sizeof(uint8_t)))
        {
            set_output_multiple(CODEC2_BYTES_PER_FRAME);

            init_state();

            message_port_register_in(pmt::mp("state_reset"));

            set_msg_handler(
                pmt::mp("state_reset"),
                boost::bind(&codec2_encoder_impl::reset, this,
                            boost::placeholders::_1));
        }

        // TODO: fix this function!
        void codec2_encoder_impl::reset(const pmt::pmt_t &msg)
        {
            std::string cmd = "";

            if (pmt::is_symbol(msg))
            {
                cmd = pmt::symbol_to_string(msg);
            }

            time_t now = time(NULL);
            struct tm t;
            localtime_r(&now, &t);

            if (cmd == "SOT")
            {
                codec2_init(&c2); // i hope this is the right place to put it
                return;
            }

            if (cmd == "EOT")
            {
                ;
                return;
            }

            fprintf(stderr, "[%02d:%02d:%02d] Strange message received\n", t.tm_hour, t.tm_min, t.tm_sec);
        }

        void codec2_encoder_impl::init_state(void)
        {
            codec2_init(&c2);
        }

        /*
         * Our virtual destructor.
         */
        codec2_encoder_impl::~codec2_encoder_impl()
        {
        }

        void
        codec2_encoder_impl::forecast(int noutput_items,
                                      gr_vector_int &ninput_items_required)
        {
            ninput_items_required[0] = (noutput_items / CODEC2_BYTES_PER_FRAME) * CODEC2_SAMPLES_PER_FRAME;
        }

        int
        codec2_encoder_impl::general_work(int noutput_items,
                                          gr_vector_int &ninput_items,
                                          gr_vector_const_void_star &input_items,
                                          gr_vector_void_star &output_items)
        {
            const int16_t *speech = static_cast<const int16_t *>(input_items[0]);
            uint8_t *bits = static_cast<uint8_t *>(output_items[0]);

            // we need one full speech frame
            if (ninput_items[0] < CODEC2_SAMPLES_PER_FRAME)
                return 0;

            // we need space for 8 output bytes
            if (noutput_items < CODEC2_BYTES_PER_FRAME)
                return 0;

            int frames = std::min(
                ninput_items[0] / CODEC2_SAMPLES_PER_FRAME,
                noutput_items / CODEC2_BYTES_PER_FRAME);

            for (int i = 0; i < frames; i++)
            {
                codec2_encode(&c2,
                              bits + i * CODEC2_BYTES_PER_FRAME,
                              speech + i * CODEC2_SAMPLES_PER_FRAME);
            }

            consume_each(frames * CODEC2_SAMPLES_PER_FRAME);
            return frames * CODEC2_BYTES_PER_FRAME;
        }
    } /* namespace m17 */
} /* namespace gr */
