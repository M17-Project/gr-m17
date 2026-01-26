/* -*- c++ -*- */
/*
 * Copyright 2026 wkaczmarski.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_M17_SYMBOL_SYNC_IMPL_H
#define INCLUDED_M17_SYMBOL_SYNC_IMPL_H

#include <atomic>
#include <gnuradio/m17/symbol_sync.h>

namespace gr
{
    namespace m17
    {

        class symbol_sync_impl : public symbol_sync
        {
        private:
            int _in_sps = 10;
            float _loop_bw = 0.001f;
            float _max_dev = 0.005f;

            // timing
            float d_mu;          // fractional timing [0, 1)
            float d_omega;       // samples per symbol (≈ in_sps)
            float d_omega_mid;   // nominal in_sps
            float d_omega_lim;   // max deviation

            // loop filter
            float d_gain_omega;  // proportional
            float d_gain_mu;     // integral

            // TED history
            float d_prev_sample;
            float d_mid_sample;
            bool  d_have_mid;

        public:
            symbol_sync_impl(int in_sps, float loop_bw, float max_dev);
            ~symbol_sync_impl();

            void set_in_sps(int in_sps);
            void set_loop_bw(float loop_bw);
            void set_max_dev(float max_dev);

            void init_state(void);

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

#endif /* INCLUDED_M17_SYMBOL_SYNC_IMPL_H */
