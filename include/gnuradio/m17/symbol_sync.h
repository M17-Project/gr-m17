/* -*- c++ -*- */
/*
 * Copyright 2026 wkaczmarski.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_M17_SYMBOL_SYNC_H
#define INCLUDED_M17_SYMBOL_SYNC_H

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
        class M17_API symbol_sync : virtual public gr::block
        {
        public:
            typedef std::shared_ptr<symbol_sync> sptr;

            /*!
             * \brief Return a shared_ptr to a new instance of m17::m17_coder.
             *
             * To avoid accidental use of raw pointers, m17::m17_coder's
             * constructor is in a private implementation
             * class. m17::m17_coder::make is the public interface for
             * creating new instances.
             */
            static sptr make(int in_sps, float loop_bw, float max_dev);
            virtual void set_in_sps(int in_sps) = 0;
            virtual void set_loop_bw(float loop_bw) = 0;
            virtual void set_max_dev(float max_dev) = 0;
        };

    } // namespace m17
} // namespace gr

#endif /* INCLUDED_M17_SYMBOL_SYNC_H */
