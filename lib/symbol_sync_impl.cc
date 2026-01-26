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

        static constexpr int INTERPS_PER_SYMBOL = 16;

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

        void symbol_sync_impl::init_state(void)
        {
            d_omega_mid = (float)_in_sps;
            d_omega = d_omega_mid;
            d_omega_lim = _max_dev;

            // ---- PLL coefficients (zeta = 1.0, TED gain = 1.0) ----
            const float zeta = 1.0f;
            // const float Kp = 1.0f;     // Gardner TED gain (normalized)
            const float Bn = _loop_bw; // normalized loop bandwidth

            const float denom = 1.0f + 2.0f * zeta * Bn + Bn * Bn;
            d_gain_mu = (4.0f * zeta * Bn) / denom;
            d_gain_omega = (4.0f * Bn * Bn) / denom;

            d_mu = 0.0f;
            d_tau = 0.0f;

            d_prev_mid = 0.0f;
            d_prev_symbol = 0.0f;

            d_interp_tick = 0;
            d_phase = 0.0f;
        }

        inline float clampf(float x, float lo, float hi)
        {
            if (x < lo)
                return lo;
            if (x > hi)
                return hi;
            return x;
        }

        static inline float interp_cubic(const float *x, float mu)
        {
            // x[-1], x[0], x[1], x[2] must be valid
            const float xm1 = x[-1];
            const float x0 = x[0];
            const float x1 = x[1];
            const float x2 = x[2];

            // const float a0 = x1 - x0;
            const float a1 = 0.5f * (x1 - xm1);
            const float a2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
            const float a3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);

            return ((a3 * mu + a2) * mu + a1) * mu + x0;
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
            ninput_items_required[0] = (int)ceilf((d_omega + d_omega_lim) * noutput_items) + 2;
        }

        int
        symbol_sync_impl::general_work(int noutput_items,
                                       gr_vector_int &ninput_items,
                                       gr_vector_const_void_star &input_items,
                                       gr_vector_void_star &output_items)
        {
            const float *in = (const float *)input_items[0];
            float *out = (float *)output_items[0];

            int ni = ninput_items[0];
            int ii = 1; // input index
            int oo = 0;

            while (ii + 2 < ni && oo < noutput_items)
            {
                // --------------------------------------------------
                // Uniform interpolation clock (fixed rate)
                // --------------------------------------------------
                float interp_mu = d_phase;
                float interp_out = interp_cubic(&in[ii], interp_mu);

                // --------------------------------------------------
                // Gardner TED at half-symbol spacing
                // --------------------------------------------------
                if (d_interp_tick == INTERPS_PER_SYMBOL / 2)
                {
                    d_prev_mid = interp_out;
                }

                // --------------------------------------------------
                // Symbol clock
                // --------------------------------------------------
                if (d_interp_tick == 0)
                {
                    float curr = interp_out;

                    float error = (d_prev_mid - interp_out) * curr;

                    d_omega += d_gain_omega * error;
                    d_omega = clampf(d_omega,
                                     d_omega_mid - d_omega_lim,
                                     d_omega_mid + d_omega_lim);

                    d_phase += d_gain_mu * error;

                    out[oo++] = curr;
                }

                // --------------------------------------------------
                // Advance interpolator phase
                // --------------------------------------------------
                d_phase += d_omega / INTERPS_PER_SYMBOL;

                while (d_phase >= 1.0f)
                {
                    d_phase -= 1.0f;
                    ii++;
                }

                d_interp_tick++;
                if (d_interp_tick == INTERPS_PER_SYMBOL)
                    d_interp_tick = 0;
            }

            consume_each(ii);
            return oo;
        }
    } /* namespace m17 */
} /* namespace gr */
