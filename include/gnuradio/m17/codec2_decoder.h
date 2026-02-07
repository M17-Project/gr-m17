/* -*- c++ -*- */
/*
 * Copyright 2026 wkaczmarski.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_M17_CODEC2_DECODER_H
#define INCLUDED_M17_CODEC2_DECODER_H

#include <gnuradio/block.h>
#include <gnuradio/m17/api.h>

namespace gr
{
    namespace m17
    {

        /*!
         * \brief <+description of block+>
         * \ingroup m17
         *
         */
        class M17_API codec2_decoder : virtual public gr::block
        {
        public:
            typedef std::shared_ptr<codec2_decoder> sptr;

            /*!
             * \brief Return a shared_ptr to a new instance of m17::m17_coder.
             *
             * To avoid accidental use of raw pointers, m17::m17_coder's
             * constructor is in a private implementation
             * class. m17::m17_coder::make is the public interface for
             * creating new instances.
             */
            static sptr make();
        };

    } // namespace m17
} // namespace gr

#endif /* INCLUDED_M17_CODEC2_DECODER_H */
