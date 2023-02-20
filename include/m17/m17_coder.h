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

#ifndef INCLUDED_M17_M17_CODER_H
#define INCLUDED_M17_M17_CODER_H

#include <m17/api.h>
#include <gnuradio/block.h>

namespace gr {
  namespace m17 {

    /*!
     * \brief <+description of block+>
     * \ingroup m17
     *
     */
    class M17_API m17_coder : virtual public gr::block
    {
     public:
      typedef boost::shared_ptr<m17_coder> sptr;

      /*!
       * \brief Return a shared_ptr to a new instance of m17::m17_coder.
       *
       * To avoid accidental use of raw pointers, m17::m17_coder's
       * constructor is in a private implementation
       * class. m17::m17_coder::make is the public interface for
       * creating new instances.
       */
       static sptr make(std::string src_id,std::string dst_id,short type,std::string meta,float samp_rate);
       virtual void set_meta(std::string meta)=0;
       virtual void set_src_id(std::string src_id)=0;
       virtual void set_dst_id(std::string dst_id)=0;
       virtual void set_samp_rate(float samp_rate)=0;

    };

  } // namespace m17
} // namespace gr

#endif /* INCLUDED_M17_M17_CODER_H */

