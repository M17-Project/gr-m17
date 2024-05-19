/* -*- c++ -*- */
/*
 * Copyright 2023 jmfriedt.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_M17_M17_CODER_IMPL_H
#define INCLUDED_M17_M17_CODER_IMPL_H

#include <gnuradio/m17/m17_coder.h>
#include "m17.h" // struct lsf declaration

namespace gr {
namespace m17 {

class m17_coder_impl : public m17_coder
{
private:
    std::string _meta;
    unsigned char _src_id[10],_dst_id[10]; // 9 character callsign
    short _type;
    int _got_lsf=0;
    uint16_t _fn=0;                      //16-bit Frame Number (for the stream mode)
    bool _debug=0;
    struct LSF _lsf, _next_lsf;
    int _countin=0;
    uint32_t _countout=0;
    uint8_t _lich_cnt=0;                 //0..5 LICH counter
    uint32_t _frame_buff_cnt;

public:
    void set_src_id(std::string src_id);
    void set_dst_id(std::string dst_id);
    void set_meta(std::string meta);
    void set_type(short type);
    void set_debug(bool debug);
    m17_coder_impl(std::string src_id,std::string dst_id,short type,std::string meta,bool debug);
    ~m17_coder_impl();

    // Where all the action really happens
    void forecast(int noutput_items, gr_vector_int& ninput_items_required);

    int work(int noutput_items,
             gr_vector_const_void_star& input_items,
             gr_vector_void_star& output_items);
};

} // namespace m17
} // namespace gr

#endif /* INCLUDED_M17_M17_CODER_IMPL_H */
