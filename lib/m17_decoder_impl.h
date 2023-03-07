/* -*- c++ -*- */
/*
 * Copyright 2023 jmfriedt.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_M17_M17_DECODER_IMPL_H
#define INCLUDED_M17_M17_DECODER_IMPL_H

#include <gnuradio/m17/m17_decoder.h>

namespace gr {
namespace m17 {

class m17_decoder_impl : public m17_decoder
{
private:
    bool _debug_data=false;
    bool _debug_ctrl=false;
    float _threshold=0.9;

public:
    m17_decoder_impl(bool debug_data,bool debug_ctrl,float threshold);
    ~m17_decoder_impl();
    void set_debug_data(bool debug);
    void set_debug_ctrl(bool debug);
    void set_threshold(float threshold);

    // Where all the action really happens
    void forecast(int noutput_items, gr_vector_int& ninput_items_required);

    int general_work(int noutput_items,
                     gr_vector_int& ninput_items,
                     gr_vector_const_void_star& input_items,
                     gr_vector_void_star& output_items);
};

} // namespace m17
} // namespace gr

#endif /* INCLUDED_M17_M17_DECODER_IMPL_H */
