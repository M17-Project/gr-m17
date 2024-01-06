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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "m17_coder_impl.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include "../M17_Implementations/SP5WWP/lib/lib.h"
#include "../M17_Implementations/SP5WWP/lib/math/golay.h"
#include "../M17_Implementations/SP5WWP/lib/payload/crc.h"
#include "../M17_Implementations/SP5WWP/lib/encode/symbols.h"
#include "../M17_Implementations/SP5WWP/lib/phy/sync.h"
#include "../M17_Implementations/SP5WWP/lib/encode/convol.h"
#include "../M17_Implementations/SP5WWP/lib/payload/call.h"
#include "../M17_Implementations/SP5WWP/lib/payload/lsf.h"
#include "../M17_Implementations/SP5WWP/lib/phy/interleave.h"
#include "../M17_Implementations/SP5WWP/lib/phy/randomize.h"

namespace gr {
  namespace m17 {

struct LSF lsf;

    m17_coder::sptr
    m17_coder::make(std::string src_id,std::string dst_id,short type,std::string meta, bool debug)
    {
      return gnuradio::get_initial_sptr
        (new m17_coder_impl(src_id,dst_id,type,meta,debug));
    }

    /*
     * The private constructor
     */
    m17_coder_impl::m17_coder_impl(std::string src_id,std::string dst_id,short type,std::string meta, bool debug)
      : gr::block("m17_coder",
              gr::io_signature::make(1, 1, sizeof(char)),
              gr::io_signature::make(1, 1, sizeof(float)))
              , _meta(meta),_type(type), _debug(debug)
{    set_meta(meta);
     set_src_id(src_id);
     set_dst_id(dst_id);
     set_type(type);
     set_debug(debug);
     set_output_multiple(192);
     uint16_t ccrc=LSF_CRC(&lsf);
     lsf.crc[0]=ccrc>>8;
     lsf.crc[1]=ccrc&0xFF;
     _got_lsf=0;                  //have we filled the LSF struct yet?
     _fn=0;                      //16-bit Frame Number (for the stream mode)
}

void m17_coder_impl::set_debug(bool debug)
{_debug=debug;
 if (_debug==true) printf("Debug true\n"); else printf("Debug false\n");
}

void m17_coder_impl::set_src_id(std::string src_id)
{int length;
 for (int i=0;i<10;i++) {_src_id[i]=0;}
 if (src_id.length()>9) length=9; else length=src_id.length();
 for (int i=0;i<length;i++) {_src_id[i]=toupper(src_id.c_str()[i]);}
 encode_callsign((uint64_t*)lsf.src,_src_id); // 6 byte ID <- 9 char callsign
 uint16_t ccrc=LSF_CRC(&lsf);
 lsf.crc[0]=ccrc>>8;
 lsf.crc[1]=ccrc&0xFF;
}

void m17_coder_impl::set_dst_id(std::string dst_id)
{int length;
 for (int i=0;i<10;i++) {_dst_id[i]=0;}
 if (dst_id.length()>9) length=9; else length=dst_id.length();
 for (int i=0;i<length;i++) {_dst_id[i]=toupper(dst_id.c_str()[i]);}
 encode_callsign((uint64_t*)lsf.dst,_dst_id); // 6 byte ID <- 9 char callsign
 uint16_t ccrc=LSF_CRC(&lsf);
 lsf.crc[0]=ccrc>>8;
 lsf.crc[1]=ccrc&0xFF;
}

void m17_coder_impl::set_meta(std::string meta)
{int length;
 printf("new meta: %s\n",meta.c_str());fflush(stdout);
 _meta.assign(meta);
 if (meta.length()<14) length=meta.length(); else length=14;
 for (int i=0;i<length;i++) {lsf.meta[i]=_meta[i];}
 uint16_t ccrc=LSF_CRC(&lsf);
 lsf.crc[0]=ccrc>>8;
 lsf.crc[1]=ccrc&0xFF;
}

void m17_coder_impl::set_type(short type)
{_type=type;
 lsf.type[0]=_type>>8;   // MSB
 lsf.type[1]=_type&0xff; // LSB
 uint16_t ccrc=LSF_CRC(&lsf);
 lsf.crc[0]=ccrc>>8;
 lsf.crc[1]=ccrc&0xFF;
 printf("new type: %hhd %hhd\n",lsf.type[1],lsf.type[0]);fflush(stdout);
}

    /*
     * Our virtual destructor.
     */
    m17_coder_impl::~m17_coder_impl()
    {
    }

    void
    m17_coder_impl::forecast (int noutput_items, gr_vector_int &ninput_items_required)
    {
      ninput_items_required[0] = noutput_items/12; // 16 inputs -> 192 outputs
    }

    int
    m17_coder_impl::general_work (int noutput_items,
                       gr_vector_int &ninput_items,
                       gr_vector_const_void_star &input_items,
                       gr_vector_void_star &output_items)
    {
      const char *in = (const char *) input_items[0];
      float *out = (float *) output_items[0];

      int countin=0;
      uint32_t countout=0;
      
      uint8_t enc_bits[SYM_PER_PLD*2];    //type-2 bits, unpacked
      uint8_t rf_bits[SYM_PER_PLD*2];     //type-4 bits, unpacked
      uint8_t lich[6];                    //48 bits packed raw, unencoded LICH
      uint8_t lich_encoded[12];           //96 bits packed, encoded LICH
      
      uint8_t data[16];                   //raw payload, packed bits
      uint8_t lich_cnt=0;                 //0..5 LICH counter, derived from the Frame Number
      
      while (countout<(uint32_t)noutput_items) {
        if (countin+16<=noutput_items)
         {if(_got_lsf) //stream frames
           {
            //we could discard the data we already have
	    for (int i=0;i<16;i++) {data[i]=in[countin];countin++;}

            //send stream frame syncword
            send_syncword(out,&countout,SYNC_STR);

            //derive the LICH_CNT from the Frame Number
            lich_cnt=_fn%6;

            //extract LICH from the whole LSF
            switch(lich_cnt)
            {
                case 0:
                    lich[0]=lsf.dst[0];
                    lich[1]=lsf.dst[1];
                    lich[2]=lsf.dst[2];
                    lich[3]=lsf.dst[3];
                    lich[4]=lsf.dst[4];
                break;

                case 1:
                    lich[0]=lsf.dst[5];
                    lich[1]=lsf.src[0];
                    lich[2]=lsf.src[1];
                    lich[3]=lsf.src[2];
                    lich[4]=lsf.src[3];
                break;

                case 2:
                    lich[0]=lsf.src[4];
                    lich[1]=lsf.src[5];
                    lich[2]=lsf.type[0];
                    lich[3]=lsf.type[1];
                    lich[4]=lsf.meta[0];
                break;

                case 3:
                    lich[0]=lsf.meta[1];
                    lich[1]=lsf.meta[2];
                    lich[2]=lsf.meta[3];
                    lich[3]=lsf.meta[4];
                    lich[4]=lsf.meta[5];
                break;

                case 4:
                    lich[0]=lsf.meta[6];
                    lich[1]=lsf.meta[7];
                    lich[2]=lsf.meta[8];
                    lich[3]=lsf.meta[9];
                    lich[4]=lsf.meta[10];
                break;

                case 5:
                    lich[0]=lsf.meta[11];
                    lich[1]=lsf.meta[12];
                    lich[2]=lsf.meta[13];
                    lich[3]=lsf.crc[0];
                    lich[4]=lsf.crc[1];
                break;

                default:
                    ;
                break;
            }
            lich[5]=lich_cnt<<5;

            //encode the LICH
            uint32_t val;

            val=golay24_encode((lich[0]<<4)|(lich[1]>>4));
            lich_encoded[0]=(val>>16)&0xFF;
            lich_encoded[1]=(val>>8)&0xFF;
            lich_encoded[2]=(val>>0)&0xFF;
            val=golay24_encode(((lich[1]&0x0F)<<8)|lich[2]);
            lich_encoded[3]=(val>>16)&0xFF;
            lich_encoded[4]=(val>>8)&0xFF;
            lich_encoded[5]=(val>>0)&0xFF;
            val=golay24_encode((lich[3]<<4)|(lich[4]>>4));
            lich_encoded[6]=(val>>16)&0xFF;
            lich_encoded[7]=(val>>8)&0xFF;
            lich_encoded[8]=(val>>0)&0xFF;
            val=golay24_encode(((lich[4]&0x0F)<<8)|lich[5]);
            lich_encoded[9]=(val>>16)&0xFF;
            lich_encoded[10]=(val>>8)&0xFF;
            lich_encoded[11]=(val>>0)&0xFF;

            //unpack LICH (12 bytes)
            memset(enc_bits, 0, SYM_PER_PLD*2);
            for(uint8_t i=0; i<12; i++)
            {
                for(uint8_t j=0; j<8; j++)
                    enc_bits[i*8+j]=(lich_encoded[i]>>(7-j))&1;
            }

            //encode the rest of the frame
            conv_encode_stream_frame(&enc_bits[96], data, _fn);

            //reorder bits
            for(uint16_t i=0; i<SYM_PER_PLD*2; i++)
                rf_bits[i]=enc_bits[intrl_seq[i]];

            //randomize
            for(uint16_t i=0; i<SYM_PER_PLD*2; i++)
            {
                if((rand_seq[i/8]>>(7-(i%8)))&1) //flip bit if '1'
                {
                    if(rf_bits[i])
                        rf_bits[i]=0;
                    else
                        rf_bits[i]=1;
                }
            }

            //send dummy symbols (debug)
            /*float s=0.0;
            for(uint8_t i=0; i<SYM_PER_PLD; i++) //40ms * 4800 - 8 (syncword)
                write(STDOUT_FILENO, (uint8_t*)&s, sizeof(float));*/

            float s;
            for(uint16_t i=0; i<SYM_PER_PLD; i++) //40ms * 4800 - 8 (syncword)
            {
                s=symbol_map[rf_bits[2*i]*2+rf_bits[2*i+1]];
                // write(STDOUT_FILENO, (uint8_t*)&s, sizeof(float));
                out[countout]=s;
                countout++;
            }

            /*printf("\tDATA: ");
            for(uint8_t i=0; i<16; i++)
                printf("%02X", data[i]);
            printf("\n");*/

            //increment the Frame Number
            _fn = (_fn + 1) % 0x8000;
           }
           else //LSF
           {
//	    for (int i=0;i<6;i++) {lsf.dst[i]=in[countin];countin++;}
//	    for (int i=0;i<6;i++) {lsf.src[i]=in[countin];countin++;}
//	    for (int i=0;i<2;i++) {lsf.type[i]=in[countin];countin++;}
//	    for (int i=0;i<14;i++) {lsf.meta[i]=in[countin];countin++;}
	    for (int i=0;i<16;i++) {data[i]=in[countin];countin++;}

            //calculate LSF CRC
//            uint16_t ccrc=LSF_CRC(&lsf);
//            lsf.crc[0]=ccrc>>8;
//            lsf.crc[1]=ccrc&0xFF;

            _got_lsf=1;
// printf("got_lsf=1\n");

            //encode LSF data
            conv_encode_LSF(enc_bits, &lsf);

            //send out the preamble and LSF
            //send_Preamble(0,out,&countout); //0 - LSF preamble, as opposed to 1 - BERT preamble
            send_preamble(out, &countout, 0);

            //send LSF syncword
            send_syncword(out,&countout,SYNC_LSF);

            //reorder bits
            for(uint16_t i=0; i<SYM_PER_PLD*2; i++)
                rf_bits[i]=enc_bits[intrl_seq[i]];

            //randomize
            for(uint16_t i=0; i<SYM_PER_PLD*2; i++)
            {
                if((rand_seq[i/8]>>(7-(i%8)))&1) //flip bit if '1'
                {
                    if(rf_bits[i])
                        rf_bits[i]=0;
                    else
                        rf_bits[i]=1;
                }
            }

            float s;
            for(uint16_t i=0; i<SYM_PER_PLD; i++) //40ms * 4800 - 8 (syncword)
            {
                s=symbol_map[rf_bits[2*i]*2+rf_bits[2*i+1]];
                out[countout]=s;
                countout++;
            }

            /*printf("DST: ");
            for(uint8_t i=0; i<6; i++)
                printf("%02X", lsf.dst[i]);
            printf(" SRC: ");
            for(uint8_t i=0; i<6; i++)
                printf("%02X", lsf.src[i]);
            printf(" TYPE: ");
            for(uint8_t i=0; i<2; i++)
                printf("%02X", lsf.type[i]);
            printf(" META: ");
            for(uint8_t i=0; i<14; i++)
                printf("%02X", lsf.meta[i]);
            printf(" CRC: ");
            for(uint8_t i=0; i<2; i++)
                printf("%02X", lsf.crc[i]);
            printf("\n");*/
           }
         }
     }
      // Tell runtime system how many input items we consumed on
      // each input stream.
    consume_each (countin);
//    printf(" noutput_items=%d countin=%d countout=%d\n",noutput_items,countin,countout);
      // Tell runtime system how many output items we produced.
      return countout;
    }

  } /* namespace m17 */
} /* namespace gr */
