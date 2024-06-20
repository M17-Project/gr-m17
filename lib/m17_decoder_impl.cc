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
#include "m17_decoder_impl.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#include "m17.h"

#define CODE_MEAN      -0.75        // mean(str_sync_symbols)
#define CODE_STD        8.21583836f //std(str_sync_symbols)*sqrt(length(str_sync_symbols)-1)

namespace gr {
  namespace m17 {

    m17_decoder::sptr
    m17_decoder::make(bool debug_data,bool debug_ctrl,float threshold,bool callsign,bool signed_str)
    {
      return gnuradio::get_initial_sptr
        (new m17_decoder_impl(debug_data,debug_ctrl,threshold,callsign,signed_str));
    }


    /*
     * The private constructor
     */
    m17_decoder_impl::m17_decoder_impl(bool debug_data,bool debug_ctrl,float threshold,bool callsign,bool signed_str)
      : gr::block("m17_decoder",
              gr::io_signature::make(1, 1, sizeof(float)),
              gr::io_signature::make(1, 1, sizeof(char))),
              _debug_data(debug_data), _debug_ctrl(debug_ctrl), _threshold(threshold), _callsign(callsign), _signed_str(signed_str)
    {set_debug_data(debug_data);
     set_debug_ctrl(debug_ctrl);
     set_threshold(threshold);
     set_callsign(callsign);
     set_signed(signed_str);
     _expected_next_fn=0;
    }

    /*
     * Our virtual destructor.
     */
    m17_decoder_impl::~m17_decoder_impl()
    {
    }

    void m17_decoder_impl::set_threshold(float threshold)
    {_threshold=threshold;
     printf("Threshold: %f\n",_threshold);
    }
    
    void m17_decoder_impl::set_debug_data(bool debug)
    {_debug_data=debug;
     if (_debug_data==true) printf("Data debug: true\n"); else printf("Data debug: false\n");
    }
    
    void m17_decoder_impl::set_debug_ctrl(bool debug)
    {_debug_ctrl=debug;
     if (_debug_ctrl==true) printf("Debug control: true\n"); else printf("Debug control: false\n");
    }
    
    void m17_decoder_impl::set_callsign(bool callsign)
    {_callsign=callsign;
     if (_callsign==true) printf("Display callsign\n"); else printf("Do not display callsign\n");
    }
    
    void m17_decoder_impl::set_signed(bool signed_str)
    {_signed_str=signed_str;
     if (_callsign==true) printf("Signed\n"); else printf("Unsigned\n");
    }

    void
    m17_decoder_impl::forecast (int noutput_items, gr_vector_int &ninput_items_required)
    {
      ninput_items_required[0] = 0; //  noutput_items;
    }

    int
    m17_decoder_impl::general_work (int noutput_items,
                       gr_vector_int &ninput_items,
                       gr_vector_const_void_star &input_items,
                       gr_vector_void_star &output_items)
    {
     const float *in = (const float *) input_items[0];
     char *out = (char *) output_items[0];
     int countout=0;

     float sample;                       //last raw sample from the stdin
     float dist;                         //Euclidean distance for finding syncwords in the symbol stream

     for (int counterin=0;counterin<ninput_items[0];counterin++)
     {
        //wait for another symbol
        sample=in[counterin];

        if(!syncd)
        {
            //push new symbol
            for(uint8_t i=0; i<7; i++)
            {
                last[i]=last[i+1];
            }

            last[7]=sample;

            //calculate euclidean norm
            dist = eucl_norm(last, str_sync_symbols, 8);

            if(dist<_threshold)     //frame syncword detected
            {
                //fprintf(stderr, "str_sync_symbols dist: %3.5f\n", dist);
                syncd=1;
                pushed=0;
                fl=0;
            }
            else
            {
                //calculate euclidean norm again, this time against LSF syncword
                dist = eucl_norm(last, lsf_sync_symbols, 8);

                if(dist<_threshold) //LSF syncword
                {
                    //fprintf(stderr, "lsf_sync dist: %3.5f\n", dist);
                    syncd=1;
                    pushed=0;
                    fl=1;
                }
            }
        }
        else
        {
            pld[pushed++]=sample;

            if(pushed==SYM_PER_PLD)
            {
                //common operations for all frame types
                //slice symbols to soft dibits
                slice_symbols(soft_bit, pld);

                //derandomize
                randomize_soft_bits(soft_bit);

                //deinterleave
                reorder_soft_bits(d_soft_bit, soft_bit);

                //if it is a frame
                if(!fl)
                {
                    //extract data
                    for(uint16_t i=0; i<272; i++)
                    {
                        enc_data[i]=d_soft_bit[96+i];
                    }

                    //decode
                    uint32_t e=viterbi_decode_punctured(frame_data, enc_data, puncture_pattern_2, 272, 12);

                    uint16_t fn = (frame_data[1] << 8) | frame_data[2];

                    if (_debug_data==true) {   //dump data - first byte is empty
                       printf("RX FN: %04X PLD: ", fn);
                    }

                    for(uint8_t i=3; i<19; i++)
                    {
                     if (_debug_data==true) {
                         printf("%02X", frame_data[i]);
                        }
                     out[countout]=frame_data[i];countout++;
                    }
                    if (_debug_data==true) {
                      printf(" e=%1.1f\n", (float)e/0xFFFF);
                    }
                    //send codec2 stream to stdout
                    //fwrite(&frame_data[3], 16, 1, stdout);

                    //if the stream is signed
                    if(_signed_str && fn<0x7FFC)
                    {
                        //if thats the first frame (fn=0)
                        if(fn==0)
                            memcpy(digest, &frame_data[3], sizeof(digest));

                        for(uint8_t i=0; i<sizeof(digest); i++)
                            digest[i]^=frame_data[3+i];
                        uint8_t tmp=digest[0];
                        for(uint8_t i=0; i<sizeof(digest)-1; i++)
                            digest[i]=digest[i+1];
                        digest[sizeof(digest)-1]=tmp;
                    }

                    //extract LICH
                    for(uint16_t i=0; i<96; i++)
                    {
                        lich_chunk[i]=d_soft_bit[i];
                    }

                    //Golay decoder
                    decode_LICH(lich_b, lich_chunk);
                    lich_cnt=lich_b[5]>>5;

                    //If we're at the start of a superframe, or we missed a frame, reset the LICH state
                    if((lich_cnt==0) || ((fn % 0x8000)!=_expected_next_fn))
                        lich_chunks_rcvd=0;

                    lich_chunks_rcvd|=(1<<lich_cnt);
                    memcpy(&lsf[lich_cnt*5], lich_b, 5);

                    //debug - dump LICH
                    if(lich_chunks_rcvd==0x3F) //all 6 chunks received?
                    {
                      if (_debug_ctrl==true)
                        {if (_callsign==true)
                           {decode_callsign_bytes(d_dst, &lsf[0]);
                            decode_callsign_bytes(d_src, &lsf[6]);
                            printf("DST: %-9s ", d_dst); //DST
                            printf("SRC: %-9s ", d_src); //SRC
                           } 
                        else
                           {printf("DST: ");   //DST
                            for(uint8_t i=0; i<6; i++)
                              printf("%02X", lsf[i]);
                            printf(" ");
                            printf("SRC: ");   //SRC
                            for(uint8_t i=0; i<6; i++)
                              printf("%02X", lsf[6+i]);
                            printf(" ");
                           }
                        }

                      //TYPE
                      uint16_t type=(uint16_t)lsf[12]*0x100+lsf[13]; //big-endian
                      if (_debug_ctrl==true)
                        {printf("TYPE: %04X (", type);
                         if(type&&1)
                            printf("STREAM: ");
                         else
                            printf("PACKET: "); //shouldn't happen
                         if(((type>>1)&3)==1)
                            printf("DATA, ");
                         else if(((type>>1)&3)==2)
                            printf("VOICE, ");
                         else if(((type>>1)&3)==3)
                            printf("VOICE+DATA, "); 
                         printf("ENCR: ");
                         if(((type>>3)&3)==0)
                            printf("PLAIN, ");
                         else if(((type>>3)&3)==1)
                           {
                            printf("SCRAM ");
                            if(((type>>5)&3)==1)
                               printf("8-bit, ");
                            else if(((type>>5)&3)==2)
                               printf("16-bit, ");
                            else if(((type>>5)&3)==3)
                               printf("24-bit, ");
                           }
                           else if(((type>>3)&3)==2)
                               printf("AES, ");
                           else
                               printf("UNK, ");
                           printf("CAN: %d", (type>>7)&0xF);
                           if((type>>11)&1)
                               printf(", SIGNED");
                           printf(") ");
                        }

                      //META
                      if (_debug_ctrl==true)
                        {printf("META: ");
                         for(uint8_t i=0; i<14; i++)
                            printf("%02X", lsf[14+i]);

                          if(CRC_M17(lsf, 30)) //CRC
                              printf(" LSF_CRC_ERR");
                          else
                              printf(" LSF_CRC_OK ");
                          printf("\n");
                        }
                    }
                    _expected_next_fn = (fn + 1) % 0x8000;
                }
                else //lsf
                {
                    if (_debug_ctrl==true) {
                        printf("{LSF}\n");
                    }
                    //decode
                    uint32_t e=viterbi_decode_punctured(lsf, d_soft_bit, puncture_pattern_1, 2*SYM_PER_PLD, 61);

                    //shift the buffer 1 position left - get rid of the encoded flushing bits
                    for(uint8_t i=0; i<30; i++)
                        lsf[i]=lsf[i+1];

                    //dump data
                    if (_debug_ctrl==true) 
                      {if (_callsign==true) 
                         {decode_callsign_bytes(d_dst, &lsf[0]);
                          decode_callsign_bytes(d_src, &lsf[6]);
                          printf("DST: %-9s ", d_dst); //DST
                          printf("SRC: %-9s ", d_src); //SRC
                         }
                         else
                         {printf("DST: "); //DST
                          for(uint8_t i=0; i<6; i++)
                            printf("%02X", lsf[i]);
                          printf(" ");
  
                          //SRC
                          printf("SRC: ");
                          for(uint8_t i=0; i<6; i++)
                            printf("%02X", lsf[6+i]);
                          printf(" ");
                         }
  
                       //TYPE
                       printf("TYPE: ");
                       for(uint8_t i=0; i<2; i++)
                          printf("%02X", lsf[12+i]);
                       printf(" ");
  
                       //META
                       printf("META: ");
                       for(uint8_t i=0; i<14; i++)
                          printf("%02X", lsf[14+i]);
                       printf(" ");
  
                       //CRC
                       //printf("CRC: ");
                       //for(uint8_t i=0; i<2; i++)
                          //printf("%02X", lsf[28+i]);
                       if(CRC_M17(lsf, 30))
                          printf("LSF_CRC_ERR");
                       else
                          printf("LSF_CRC_OK ");
                       //Viterbi decoder errors
                       printf(" e=%1.1f\n", (float)e/0xFFFF);
                      }
                }

                //job done
                syncd=0;
                pushed=0;

                for(uint8_t i=0; i<8; i++)
                    last[i]=0.0;
            }
        }
     }
     // Tell runtime system how many input items we consumed on
     // each input stream.
     consume_each (ninput_items[0]);

     // Tell runtime system how many output items we produced.
     return countout;
    }

  } /* namespace m17 */
} /* namespace gr */
