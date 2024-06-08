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
    m17_coder::make(std::string src_id,std::string dst_id,int mode,int data,int encr_type,int encr_subtype,int can,std::string meta, bool debug)
    {
      return gnuradio::get_initial_sptr
        (new m17_coder_impl(src_id,dst_id,mode,data,encr_type,encr_subtype,can,meta,debug));
    }

    /*
     * The private constructor
     */
    m17_coder_impl::m17_coder_impl(std::string src_id,std::string dst_id,int mode,int data,int encr_type,int encr_subtype,int can,std::string meta, bool debug)
      : gr::block("m17_coder",
              gr::io_signature::make(1, 1, sizeof(char)),
              gr::io_signature::make(1, 1, sizeof(float)))
              , _mode(mode),_data(data),_encr_type(encr_type),_encr_subtype(encr_subtype), _can(can),_meta(meta), _debug(debug)
{    set_type(mode, data, encr_type, encr_subtype, can);
     set_meta(meta); // depends on   ^^^ encr_subtype
     set_src_id(src_id);
     set_dst_id(dst_id);
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
 const char *c;
 printf("new meta: ");
 if (_encr_subtype==1) // meta is \0-terminated string
    {if (meta.length()<14) length=meta.length(); else {length=14;meta[length]=0;}
     printf("%s\n",meta.c_str());
     for (int i=0;i<length;i++) {lsf.meta[i]=meta[i];}
    }
 else 
    {c=meta.data();
printf("%d %d --- ",meta.size(),meta.length());
     if (meta.size()<14) length=meta.size(); else length=14;
     printf("%d bytes: ",length);
     for (int i=0;i<length;i++) 
        {printf("%02X ",c[i]);
         lsf.meta[i]=c[i];
        }
     printf("\n");
    }
 fflush(stdout);
 uint16_t ccrc=LSF_CRC(&lsf);
 lsf.crc[0]=ccrc>>8;
 lsf.crc[1]=ccrc&0xFF;
}

void m17_coder_impl::set_mode(int mode)
{_mode=mode;
 printf("new mode: %02x -> ",_mode);
 set_type(_mode,_data,_encr_type,_encr_subtype,_can);
}

void m17_coder_impl::set_data(int data)
{_data=data;
 printf("new data type: %02x -> ",_data);
 set_type(_mode,_data,_encr_type,_encr_subtype,_can);
}

void m17_coder_impl::set_encr_type(int encr_type)
{_encr_type=encr_type;
 printf("new encr type: %02x -> ",_encr_type);
 set_type(_mode,_data,_encr_type,_encr_subtype,_can);
}

void m17_coder_impl::set_encr_subtype(int encr_subtype)
{_encr_subtype=encr_subtype;
 printf("new encr subtype: %02x -> ",_encr_subtype);
 set_type(_mode,_data,_encr_type,_encr_subtype,_can);
}

void m17_coder_impl::set_can(int can)
{_can=can;
 printf("new CAN: %02x -> ",_can);
 set_type(_mode,_data,_encr_type,_encr_subtype,_can);
}

void m17_coder_impl::set_type(int mode,int data,int encr_type,int encr_subtype,int can)
{short tmptype;
 tmptype = mode | (data<<1) | (encr_type<<3) | (encr_subtype<<5) | (can<<7);
 lsf.type[0]=tmptype>>8;   // MSB
 lsf.type[1]=tmptype&0xff; // LSB
 uint16_t ccrc=LSF_CRC(&lsf);
 lsf.crc[0]=ccrc>>8;
 lsf.crc[1]=ccrc&0xFF;
 printf("type: %02X%02X\n",lsf.type[0],lsf.type[1]);fflush(stdout);
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
/*            
            if (_debug==true)
               {printf("\tTX DATA: ");
                for(uint8_t i=0; i<16; i++)
                   printf("%02X", data[i]);
                printf("\n");
               }
*/
            //increment the Frame Number
            _fn = (_fn + 1) % 0x8000;

            //increment the LICH counter
            lich_cnt = (lich_cnt + 1) % 6;
           }
           else //LSF
           {
            _got_lsf=1;

            //send out the preamble and LSF
            send_preamble(out, &countout, 0); //0 - LSF preamble, as opposed to 1 - BERT preamble

            //send LSF syncword
            send_syncword(out, &countout,SYNC_LSF);
            
            //encode LSF data
            conv_encode_LSF(enc_bits, &lsf);

            //reorder bits
            reorder_bits(rf_bits, enc_bits);

            //randomize
            randomize_bits(rf_bits);

	    //send LSF data
	    send_data(out, &countout, rf_bits);

            if (_debug==true)
            {printf("TX DST: ");
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
             printf("\n");
            }
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
