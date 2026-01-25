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
#include "symbol_sync_impl.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

namespace gr
{
    namespace m17
    {
        symbol_sync::sptr
        symbol_sync::make(int in_sps, float loop_bw, float max_dev)
        {
            return gnuradio::get_initial_sptr(new symbol_sync_impl(in_sps, loop_bw, max_dev));
        }

        /*
         * The private constructor
         */
        symbol_sync_impl::symbol_sync_impl(int in_sps, float loop_bw, float max_dev) : gr::block("symbol_sync", gr::io_signature::make(1, 1, sizeof(float)),
                                                         gr::io_signature::make(1, 1, sizeof(float)))
        {
			set_in_sps(in_sps);
			set_loop_bw(loop_bw);
			set_max_dev(max_dev);

            init_state();

            ;
        }

        void symbol_sync_impl::set_in_sps(int in_sps)
        {
            _in_sps = in_sps;
        }

        void symbol_sync_impl::set_loop_bw(float loop_bw)
        {
            _loop_bw = loop_bw;
        }

        void symbol_sync_impl::set_max_dev(float max_dev)
        {
            _max_dev = max_dev;
        }

        // TODO: fix this function
        void symbol_sync_impl::init_state(void)
        {
            ;
        }

        /*
         * Our virtual destructor.
         */
        symbol_sync_impl::~symbol_sync_impl()
        {
        }

        void
        symbol_sync_impl::forecast(int noutput_items,
                                   gr_vector_int &ninput_items_required)
        {
            ninput_items_required[0] = 1;
        }

        int
        symbol_sync_impl::general_work(int noutput_items,
                                       gr_vector_int &ninput_items,
                                       gr_vector_const_void_star &input_items,
                                       gr_vector_void_star &output_items)
        {
            // TODO: fill with actual work
            ;

            // emit 1 symbol
            return 1;
        }
    } /* namespace m17 */
} /* namespace gr */
