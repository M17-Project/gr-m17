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

#ifndef INCLUDED_M17_M17_DECODER_H
#define INCLUDED_M17_M17_DECODER_H

#include <m17/api.h>
#include <gnuradio/block.h>

namespace gr {
  namespace m17 {

    /*!
     * \brief <+description of block+>
     * \ingroup m17
     *
     */
    class M17_API m17_decoder : virtual public gr::block
    {
     public:
      typedef boost::shared_ptr<m17_decoder> sptr;

      /*!
       * \brief Return a shared_ptr to a new instance of m17::m17_decoder.
       *
       * To avoid accidental use of raw pointers, m17::m17_decoder's
       * constructor is in a private implementation
       * class. m17::m17_decoder::make is the public interface for
       * creating new instances.
       */
      static sptr make();
      virtual void set_debug(bool debug)=0;
    };

  } // namespace m17
} // namespace gr

#endif /* INCLUDED_M17_M17_DECODER_H */

