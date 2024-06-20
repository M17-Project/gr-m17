/* -*- c++ -*- */
/*
 * Copyright 2023 jmfriedt.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_M17_M17_CODER_IMPL_H
#define INCLUDED_M17_M17_CODER_IMPL_H

#include <gnuradio/m17/m17_coder.h>

#ifdef AES
#include "aes.hpp"
#endif

namespace gr {
namespace m17 {

class m17_coder_impl : public m17_coder
{
private:
    unsigned char _src_id[10],_dst_id[10]; // 9 character callsign
    int _mode,_data,_encr_type,_encr_subtype,_can;

    std::string _meta;
    int _got_lsf=0;
    uint16_t _fn=0;                      //16-bit Frame Number (for the stream mode)
    bool _debug=0;
    bool _signed_str,_finished;

    //encryption
    uint8_t _encryption=0;
#ifdef AES
    struct AES_ctx _ctx;
#else
    #define AES_KEYLEN 32
    #define AES_BLOCKLEN 32
#endif
    uint8_t _key[AES_KEYLEN]; // ={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32}; //TODO: replace with a `-K` arg key entry
    uint8_t _priv_key[32]; 
    uint8_t _sig[64]; 
    uint8_t iv[AES_BLOCKLEN];
    time_t epoch = 1577836800L;         //Jan 1, 2020, 00:00:00 UTC
#ifdef ECC
    const struct uECC_Curve_t* curve = uECC_secp256r1();
#endif

public:
    void set_src_id(std::string src_id);
    void set_dst_id(std::string dst_id);
    void set_key(std::string key);
    void set_meta(std::string meta);
    void set_type(int mode,int data,int encr_type,int encr_subtype,int can);
    void set_mode(int mode);
    void set_data(int data);
    void set_encr_type(int encr_type);
    void set_encr_subtype(int encr_subtype);
    void set_can(int can);
    void set_debug(bool debug);
    void set_signed(bool signed_str);
    m17_coder_impl(std::string src_id,std::string dst_id,int mode,int data,int encr_type,int encr_subtype,int can,std::string meta, std::string key, bool debug,bool signed_str);
    ~m17_coder_impl();

    // Where all the action really happens
    void forecast(int noutput_items, gr_vector_int& ninput_items_required);

    int general_work(int noutput_items,
                     gr_vector_int& ninput_items,
                     gr_vector_const_void_star& input_items,
                     gr_vector_void_star& output_items);
};

} // namespace m17
} // namespace gr

#endif /* INCLUDED_M17_M17_CODER_IMPL_H */
