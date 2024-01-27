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

namespace gr {
namespace m17 {

class m17_decoder_impl : public m17_decoder
{
private:
    bool _debug_data=false;
    bool _debug_ctrl=false;
    float _threshold=0.9;

    float last[8] = {0};                //look-back buffer for finding syncwords
    float pld[SYM_PER_PLD];             //raw frame symbols
    uint16_t soft_bit[2*SYM_PER_PLD];   //raw frame soft bits
    uint16_t d_soft_bit[2*SYM_PER_PLD]; //deinterleaved soft bits
    uint16_t _expected_next_fn;
    
    uint8_t lsf[30+1];                  //complete LSF (one byte extra needed for the Viterbi decoder)
    uint16_t lich_chunk[96];            //raw, soft LSF chunk extracted from the LICH
    uint8_t lich_b[6];                  //48-bit decoded LICH
    uint8_t lich_cnt;                   //LICH_CNT
    uint8_t lich_chunks_rcvd=0;         //flags set for each LSF chunk received
    
    uint16_t enc_data[272];             //raw frame data soft bits
    uint8_t frame_data[19];             //decoded frame data, 144 bits (16+128), plus 4 flushing bits
    
    uint8_t syncd=0;                    //syncword found?
    uint8_t fl=0;                       //Frame=0 of LSF=1
    uint8_t pushed;                     //counter for pushed symbols

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
