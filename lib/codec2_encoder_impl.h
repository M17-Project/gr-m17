/* -*- c++ -*- */
/*
 * Copyright 2026 wkaczmarski.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_M17_CODEC2_ENCODER_IMPL_H
#define INCLUDED_M17_CODEC2_ENCODER_IMPL_H

#include <atomic>
#include <gnuradio/m17/codec2_encoder.h>

#ifdef __cplusplus
  #define _Static_assert(cond, msg) static_assert(cond, msg)
#endif

#include "../codec2-mod/inc/codec2_mod.h"

#ifdef __cplusplus
  #undef _Static_assert
#endif

namespace gr
{
    namespace m17
    {

        class codec2_encoder_impl : public codec2_encoder
        {
        private:
            codec2_t c2;

            void reset(const pmt::pmt_t &msg);
            void init_state(void);

        public:
            codec2_encoder_impl();
            ~codec2_encoder_impl();

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

#endif /* INCLUDED_M17_CODEC2_ENCODER_IMPL_H */
