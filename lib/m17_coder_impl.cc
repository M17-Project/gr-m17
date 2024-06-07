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

#include "m17.h"

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
 encode_callsign_bytes(lsf.src, _src_id); // 6 byte ID <- 9 char callsign

 uint16_t ccrc=LSF_CRC(&lsf);
 lsf.crc[0]=ccrc>>8;
 lsf.crc[1]=ccrc&0xFF;
}

void m17_coder_impl::set_dst_id(std::string dst_id)
{int length;
 for (int i=0;i<10;i++) {_dst_id[i]=0;}
 if (dst_id.length()>9) length=9; else length=dst_id.length();
 for (int i=0;i<length;i++) {_dst_id[i]=toupper(dst_id.c_str()[i]);}
 encode_callsign_bytes(lsf.dst, _dst_id); // 6 byte ID <- 9 char callsign
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
      float frame_buff_tmp[192];
      uint32_t frame_buff_count_tmp;
      
      uint8_t enc_bits[SYM_PER_PLD*2];    //type-2 bits, unpacked
      uint8_t rf_bits[SYM_PER_PLD*2];     //type-4 bits, unpacked
      uint8_t lich[6];                    //48 bits packed raw, unencoded LICH
      uint8_t lich_encoded[12];           //96 bits packed, encoded LICH
      
      uint8_t data[16];                   //raw payload, packed bits
      uint8_t lich_cnt=0;                 //0..5 LICH counter, derived from the Frame Number
      
      while (countout<(uint32_t)noutput_items) {
        if (countin+16<=noutput_items)
         {for (int i=0;i<16;i++) {data[i]=in[countin];countin++;}
          if(_got_lsf) //stream frames
           {
            //send stream frame syncword
            send_syncword(out,&countout,SYNC_STR);

            //extract LICH from the whole LSF
            extract_LICH(lich, lich_cnt, &lsf);

            //encode the LICH
            encode_LICH(lich_encoded, lich);

            //unpack LICH (12 bytes)
            unpack_LICH(enc_bits, lich_encoded);

            //encode the rest of the frame (starting at bit 96 - 0..95 are filled with LICH)
            // conv_encode_stream_frame(&enc_bits[96], data, finished ? (_fn | 0x8000) : _fn); JMF review
            conv_encode_stream_frame(&enc_bits[96], data, _fn);

            //reorder bits
            reorder_bits(rf_bits, enc_bits);

            //randomize
            randomize_bits(rf_bits);

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

            _got_lsf=1;
// printf("got_lsf=1\n");

            //send out the preamble and LSF
            send_preamble(frame_buff_tmp, &frame_buff_count_tmp, 0); //0 - LSF preamble, as opposed to 1 - BERT preamble
            for(uint16_t i=0; i<SYM_PER_FRA; i++) //40ms * 4800 - 8 (syncword)
            {
                out[countout]=frame_buff_tmp[i];
                countout++;
            }

            //send LSF syncword
            send_syncword(frame_buff_tmp,&frame_buff_count_tmp,SYNC_LSF);
            for(uint16_t i=0; i<SYM_PER_SWD; i++) //40ms * 4800 - 8 (syncword)
            {
                out[countout]=frame_buff_tmp[i];
                countout++;
            }
            
            //encode LSF data
            conv_encode_LSF(enc_bits, &lsf);

            //reorder bits
            reorder_bits(rf_bits, enc_bits);

            //randomize
            randomize_bits(rf_bits);

			//send LSF data
	    send_data(frame_buff_tmp, &frame_buff_count_tmp, rf_bits);
            
            for(uint16_t i=0; i<SYM_PER_PLD; i++) //40ms * 4800 - 8 (syncword)
            {
                out[countout]=frame_buff_tmp[i];
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
