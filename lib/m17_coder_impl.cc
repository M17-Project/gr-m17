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

// 240620: todo uncomment #idef AES for cryptography and #ifdef ECC for signature

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

#ifdef AES
#include "aes.hpp"
#endif

#ifdef ECC
#include "../../micro-ecc/uECC.h"
#endif

namespace gr {
  namespace m17 {

struct LSF lsf;

    m17_coder::sptr
    m17_coder::make(std::string src_id,std::string dst_id,int mode,int data,int encr_type,int encr_subtype,int can,std::string meta, std::string key, bool debug, bool signed_str)
    {
      return gnuradio::get_initial_sptr
        (new m17_coder_impl(src_id,dst_id,mode,data,encr_type,encr_subtype,can,meta,key,debug,signed_str));
    }

    /*
     * The private constructor
     */
    m17_coder_impl::m17_coder_impl(std::string src_id,std::string dst_id,int mode,int data,int encr_type,int encr_subtype,int can,std::string meta, std::string key, bool debug,bool signed_str)
      : gr::block("m17_coder",
              gr::io_signature::make(1, 1, sizeof(char)),
              gr::io_signature::make(1, 1, sizeof(float)))
              , _mode(mode),_data(data),_encr_type(encr_type),_encr_subtype(encr_subtype), _can(can), _meta(meta), _debug(debug), _signed_str(signed_str)
{    set_type(mode, data, encr_type, encr_subtype, can);
     set_meta(meta); // depends on   ^^^ encr_subtype
     set_src_id(src_id);
     set_dst_id(dst_id);
     set_signed(signed_str);
     set_debug(debug);
     set_output_multiple(192);
     uint16_t ccrc=LSF_CRC(&lsf);
     lsf.crc[0]=ccrc>>8;
     lsf.crc[1]=ccrc&0xFF;
     _got_lsf=0;                  //have we filled the LSF struct yet?
     _fn=0;                      //16-bit Frame Number (for the stream mode)
     _finished=false;
     if(_encr_type==2)
      {
        set_key(key); // read key
#ifdef AES
        AES_init_ctx(&_ctx, _key);
#endif
        *((int32_t*)&iv[0])=(uint32_t)time(NULL)-(uint32_t)epoch; //timestamp
        for(uint8_t i=4; i<4+10; i++) iv[i]=0; //10 random bytes TODO: replace with a rand() or pass through an additional arg
      }

}

void m17_coder_impl::set_signed(bool signed_str)
{_signed_str=signed_str;
 if (_signed_str==true) printf("Signed\n"); else printf("Unsigned\n");
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

void m17_coder_impl::set_key(std::string arg) // *UTF-8* encoded byte array
{int length;
 printf("new key: ");
 length=arg.size();
 int i=0,j=0;
 while ((j<32) && (i<length))
    {if ((unsigned int)arg.data()[i]<0xc2) // https://www.utf8-chartable.de/
         {_key[j]=arg.data()[i];i++;j++;}
     else
         {_key[j]=(arg.data()[i]-0xc2)*0x40+arg.data()[i+1];i+=2;j++;}
    }
 length=j; // index from 0 to length-1
 printf("%d bytes: ",length);
 for (i=0;i<length;i++) printf("%02X ",_key[i]);
 printf("\n");
 fflush(stdout);
}

void m17_coder_impl::set_meta(std::string meta) // either an ASCII string if encr_subtype==0 or *UTF-8* encoded byte array
{int length;
 printf("new meta: ");
 if (_encr_subtype==0) // meta is \0-terminated string
    {if (meta.length()<14) length=meta.length(); else {length=14;meta[length]=0;}
     printf("%s\n",meta.c_str());
     for (int i=0;i<length;i++) {lsf.meta[i]=meta[i];}
    }
 else 
    {length=meta.size();
     int i=0,j=0;
     while ((j<14) && (i<length))
       {if ((unsigned int)meta.data()[i]<0xc2) // https://www.utf8-chartable.de/
           {lsf.meta[j]=meta.data()[i];i++;j++;}
        else
           {lsf.meta[j]=(meta.data()[i]-0xc2)*0x40+meta.data()[i+1];i+=2;j++;}
       }
       length=j; // index from 0 to length-1
       printf("%d bytes: ",length);
       for (i=0;i<length;i++) printf("%02X ",lsf.meta[i]);
     printf("\n");
    }
 fflush(stdout);
 uint16_t ccrc=LSF_CRC(&lsf);
 lsf.crc[0]=ccrc>>8;
 lsf.crc[1]=ccrc&0xFF;
}

void m17_coder_impl::set_mode(int mode)
{_mode=mode;
 printf("new mode: %x -> ",_mode);
 set_type(_mode,_data,_encr_type,_encr_subtype,_can);
}

void m17_coder_impl::set_data(int data)
{_data=data;
 printf("new data type: %x -> ",_data);
 set_type(_mode,_data,_encr_type,_encr_subtype,_can);
}

void m17_coder_impl::set_encr_type(int encr_type)
{_encr_type=encr_type;
 printf("new encr type: %x -> ",_encr_type);
 set_type(_mode,_data,_encr_type,_encr_subtype,_can);
}

void m17_coder_impl::set_encr_subtype(int encr_subtype)
{_encr_subtype=encr_subtype;
 printf("new encr subtype: %x -> ",_encr_subtype);
 set_type(_mode,_data,_encr_type,_encr_subtype,_can);
}

void m17_coder_impl::set_can(int can)
{_can=can;
 printf("new CAN: %x -> ",_can);
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
 printf("Transmission type: 0x%02X%02X\n",lsf.type[0],lsf.type[1]);fflush(stdout);
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

            if(!_signed_str)
                conv_encode_stream_frame(&enc_bits[96], data, _finished ? (_fn|0x8000) : _fn);
            else //dont set the MSB is the stream is signed
            {
                conv_encode_stream_frame(&enc_bits[96], data, _fn);
            }

            //reorder bits
            reorder_bits(rf_bits, enc_bits);

            //randomize
            randomize_bits(rf_bits);

            //send frame data
	    send_data(out, &countout, rf_bits);

            //increment the Frame Number
            _fn = (_fn + 1) % 0x8000;

            //increment the LICH counter
            lich_cnt = (lich_cnt + 1) % 6;

            if(_finished && _signed_str) //if we are done, and the stream is signed, so we need to transmit the signature (4 frames)
            {
                for(uint8_t i=0; i<sizeof(_priv_key); i++) //test fill
                    _priv_key[i]=i;
#ifdef ECC
                uECC_sign(priv_key, digest, sizeof(digest), _sig, curve);
#endif

                //4 frames with 512-bit signature
                _fn = 0x7FFC; //signature has to start at 0x7FFC to end at 0x7FFF (0xFFFF with EoT marker set)
                for(uint8_t i=0; i<4; i++)
                {
                    send_syncword(out, &countout, SYNC_STR);
                    extract_LICH(lich, lich_cnt, &lsf); //continue with next LICH_CNT
                    encode_LICH(lich_encoded, lich);
                    unpack_LICH(enc_bits, lich_encoded);
                    conv_encode_stream_frame(&enc_bits[96], &_sig[i*16], _fn);
                    reorder_bits(rf_bits, enc_bits);
                    randomize_bits(rf_bits);
	            send_data(out, &countout, rf_bits);
                    _fn = (_fn<0x7FFE) ? _fn+1 : (0x7FFF|0x8000);
                    lich_cnt = (lich_cnt + 1) % 6;
                }
            }

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
                   printf("%hhX", lsf.dst[i]);
               printf(" SRC: ");
               for(uint8_t i=0; i<6; i++)
                   printf("%hhX", lsf.src[i]);
               printf(" TYPE: ");
               for(uint8_t i=0; i<2; i++)
                   printf("%hhX", lsf.type[i]);
               printf(" META: ");
               for(uint8_t i=0; i<14; i++)
                   printf("%hhX", lsf.meta[i]);
               printf(" CRC: ");
               for(uint8_t i=0; i<2; i++)
                   printf("%hhX", lsf.crc[i]);
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
