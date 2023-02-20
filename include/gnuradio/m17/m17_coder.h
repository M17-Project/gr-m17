/* -*- c++ -*- */
/*
 * Copyright 2023 jmfriedt.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_M17_M17_CODER_H
#define INCLUDED_M17_M17_CODER_H

#include <gnuradio/block.h>
#include <gnuradio/m17/api.h>

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
    typedef std::shared_ptr<m17_coder> sptr;

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
