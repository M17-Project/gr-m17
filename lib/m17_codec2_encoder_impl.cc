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
        codec2_encoder_impl::codec2_encoder_impl() : gr::block("m17_coder", gr::io_signature::make(1, 1, sizeof(char)),
                                                               gr::io_signature::make(1, 1, sizeof(float)))
        {
            message_port_register_in(pmt::mp("transmission_control"));

            set_msg_handler(
                pmt::mp("transmission_control"),
                boost::bind(&codec2_encoder_impl::reset, this,
                            boost::placeholders::_1));

            ;
        }

        // TODO: fix this function!
        void codec2_encoder_impl::reset(const pmt::pmt_t &msg)
        {
            std::string cmd = "", val = "";

            if (pmt::is_symbol(msg))
            {
                cmd = pmt::symbol_to_string(msg);
            }
            /*else if (pmt::is_pair(msg))
            {
                const pmt::pmt_t &car = pmt::car(msg);
                if (pmt::is_symbol(car))
                    cmd = pmt::symbol_to_string(car);
                const pmt::pmt_t &cdr = pmt::cdr(msg);
                if (pmt::is_symbol(cdr))
                    val = pmt::symbol_to_string(cdr);
            }*/

            time_t now = time(NULL);
            struct tm t;
            localtime_r(&now, &t);

            if (cmd == "SOT")
            {
                ;
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
            ninput_items_required[0] = CODEC2_SAMPLES_PER_FRAME; // full Codec2 3200 frame: 8 byes (64 bits)
        }

        int
        codec2_encoder_impl::general_work(int noutput_items,
                                     gr_vector_int &ninput_items,
                                     gr_vector_const_void_star &input_items,
                                     gr_vector_void_star &output_items)
        {
            //TODO: fix this
            //uint8_t out[CODEC2_BYTES_PER_FRAME] = {0};
            //int16_t speech[CODEC2_SAMPLES_PER_FRAME] = {0};

            //codec2_encode(&c2, out, speech);
            //memcpy(..., out, sizeof(out));

            return CODEC2_BYTES_PER_FRAME;
        }
    } /* namespace m17 */
} /* namespace gr */
