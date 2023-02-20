/* -*- c++ -*- */
/*
 * Copyright 2023 jmfriedt.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifndef INCLUDED_M17_M17_CODER_IMPL_H
#define INCLUDED_M17_M17_CODER_IMPL_H

#include <m17/m17_coder.h>

namespace gr {
  namespace m17 {

class m17_coder_impl : public m17_coder
{
private:
    std::string _meta;
    unsigned char _src_id[6],_dst_id[6];
    float _samp_rate=0.;
    short _type;
    int _got_lsf=0;
    uint16_t _fn=0;                      //16-bit Frame Number (for the stream mode)


public:
    void set_src_id(std::string meta);
    void set_dst_id(std::string meta);
    void set_samp_rate(float samp_rate);
    void set_meta(std::string meta);
    void set_type(short type);
    m17_coder_impl(std::string src_id,std::string dst_id,short type,std::string meta,float samp_rate);
    ~m17_coder_impl();

      // Where all the action really happens
      void forecast (int noutput_items, gr_vector_int &ninput_items_required);

      int general_work(int noutput_items,
           gr_vector_int &ninput_items,
           gr_vector_const_void_star &input_items,
           gr_vector_void_star &output_items);

    };

  } // namespace m17
} // namespace gr

#endif /* INCLUDED_M17_M17_CODER_IMPL_H */

