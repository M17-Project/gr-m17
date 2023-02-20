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

#include "../../inc/m17.h"
#include "../../m17-coder/golay.h"
#include "../../m17-coder/crc.h"

namespace gr {
  namespace m17 {

struct LSF
{
	uint8_t dst[6];
	uint8_t src[6];
	uint8_t type[2];
	uint8_t meta[112/8];
	uint8_t crc[2];
} lsf;

void send_Preamble(const uint8_t type,float *out, int *counterout, float samp_rate)
{
    float symb;

    if(type) //pre-BERT
    {
        // for(uint16_t i=0; i<(int)(40e-3*samp_rate)/2; i++) //40ms * 4800 = 192 TODO JMF
        for(uint16_t i=0; i<(int)(192/2); i++) //40ms * 4800 = 192
        {
            symb=-3.0;
            // write(STDOUT_FILENO, (uint8_t*)&symb,  sizeof(float));
            out[*counterout]=symb;
            *counterout=(*counterout)+1;
            symb=+3.0;
            // write(STDOUT_FILENO, (uint8_t*)&symb,  sizeof(float));
            out[*counterout]=symb;
            *counterout=(*counterout)+1;
        }
    }
    else //pre-LSF
    {
        // for(uint16_t i=0; i<(int)(40e-3*samp_rate)/2; i++) //40ms * 4800 = 192 TODO JMF
        for(uint16_t i=0; i<(int)(192/2); i++) //40ms * 4800 = 192
        {
            symb=+3.0;
            // write(STDOUT_FILENO, (uint8_t*)&symb,  sizeof(float));
            out[*counterout]=symb;
            *counterout=(*counterout)+1;
            symb=-3.0;
            // write(STDOUT_FILENO, (uint8_t*)&symb,  sizeof(float));
            out[*counterout]=symb;
            *counterout=(*counterout)+1;
        }
    }
}

void send_Syncword(const uint16_t sword, float *out, int *counterout)
{
    float symb;

    for(uint8_t i=0; i<16; i+=2)
    {
        symb=symbol_map[(sword>>(14-i))&3];
        // write(STDOUT_FILENO, (uint8_t*)&symb,  sizeof(float));
        out[*counterout]=symb;
        *counterout=(*counterout)+1;
    }
}

//out - unpacked bits
//in - packed raw bits
//fn - frame number
void conv_Encode_Frame(uint8_t* out, uint8_t* in, uint16_t fn)
{
	uint8_t pp_len = sizeof(P_2);
	uint8_t p=0;			//puncturing pattern index
	uint16_t pb=0;			//pushed punctured bits
	uint8_t ud[144+4+4];	//unpacked data

	memset(ud, 0, 144+4+4);

	//unpack frame number
	for(uint8_t i=0; i<16; i++)
	{
		ud[4+i]=(fn>>(15-i))&1;
	}

	//unpack data
	for(uint8_t i=0; i<16; i++)
	{
		for(uint8_t j=0; j<8; j++)
		{
			ud[4+16+i*8+j]=(in[i]>>(7-j))&1;
		}
	}

	//encode
	for(uint8_t i=0; i<144+4; i++)
	{
		uint8_t G1=(ud[i+4]                +ud[i+1]+ud[i+0])%2;
        uint8_t G2=(ud[i+4]+ud[i+3]+ud[i+2]        +ud[i+0])%2;

		//printf("%d%d", G1, G2);

		if(P_2[p])
		{
			out[pb]=G1;
			pb++;
		}

		p++;
		p%=pp_len;

		if(P_2[p])
		{
			out[pb]=G2;
			pb++;
		}

		p++;
		p%=pp_len;
	}

	//printf("pb=%d\n", pb);
}

//out - unpacked bits
//in - packed raw bits (LSF struct)
void conv_Encode_LSF(uint8_t* out, struct LSF *in)
{
	uint8_t pp_len = sizeof(P_1);
	uint8_t p=0;			//puncturing pattern index
	uint16_t pb=0;			//pushed punctured bits
	uint8_t ud[240+4+4];	//unpacked data

	memset(ud, 0, 240+4+4);

	//unpack DST
	for(uint8_t i=0; i<8; i++)
	{
		ud[4+i]   =((in->dst[0])>>(7-i))&1;
		ud[4+i+8] =((in->dst[1])>>(7-i))&1;
		ud[4+i+16]=((in->dst[2])>>(7-i))&1;
		ud[4+i+24]=((in->dst[3])>>(7-i))&1;
		ud[4+i+32]=((in->dst[4])>>(7-i))&1;
		ud[4+i+40]=((in->dst[5])>>(7-i))&1;
	}

	//unpack SRC
	for(uint8_t i=0; i<8; i++)
	{
		ud[4+i+48]=((in->src[0])>>(7-i))&1;
		ud[4+i+56]=((in->src[1])>>(7-i))&1;
		ud[4+i+64]=((in->src[2])>>(7-i))&1;
		ud[4+i+72]=((in->src[3])>>(7-i))&1;
		ud[4+i+80]=((in->src[4])>>(7-i))&1;
		ud[4+i+88]=((in->src[5])>>(7-i))&1;
	}

	//unpack TYPE
	for(uint8_t i=0; i<8; i++)
	{
		ud[4+i+96] =((in->type[0])>>(7-i))&1;
		ud[4+i+104]=((in->type[1])>>(7-i))&1;
	}

	//unpack META
	for(uint8_t i=0; i<8; i++)
	{
		ud[4+i+112]=((in->meta[0])>>(7-i))&1;
		ud[4+i+120]=((in->meta[1])>>(7-i))&1;
		ud[4+i+128]=((in->meta[2])>>(7-i))&1;
		ud[4+i+136]=((in->meta[3])>>(7-i))&1;
		ud[4+i+144]=((in->meta[4])>>(7-i))&1;
		ud[4+i+152]=((in->meta[5])>>(7-i))&1;
		ud[4+i+160]=((in->meta[6])>>(7-i))&1;
		ud[4+i+168]=((in->meta[7])>>(7-i))&1;
		ud[4+i+176]=((in->meta[8])>>(7-i))&1;
		ud[4+i+184]=((in->meta[9])>>(7-i))&1;
		ud[4+i+192]=((in->meta[10])>>(7-i))&1;
		ud[4+i+200]=((in->meta[11])>>(7-i))&1;
		ud[4+i+208]=((in->meta[12])>>(7-i))&1;
		ud[4+i+216]=((in->meta[13])>>(7-i))&1;
	}

	//unpack CRC
	for(uint8_t i=0; i<8; i++)
	{
		ud[4+i+224]=((in->crc[0])>>(7-i))&1;
		ud[4+i+232]=((in->crc[1])>>(7-i))&1;
	}

	//encode
	for(uint8_t i=0; i<240+4; i++)
	{
		uint8_t G1=(ud[i+4]                +ud[i+1]+ud[i+0])%2;
        uint8_t G2=(ud[i+4]+ud[i+3]+ud[i+2]        +ud[i+0])%2;

		//printf("%d%d", G1, G2);

		if(P_1[p])
		{
			out[pb]=G1;
			pb++;
		}

		p++;
		p%=pp_len;

		if(P_1[p])
		{
			out[pb]=G2;
			pb++;
		}

		p++;
		p%=pp_len;
	}

	//printf("pb=%d\n", pb);
}

uint16_t LSF_CRC(struct LSF *in)
{
    uint8_t d[28];

    memcpy(&d[0], in->dst, 6);
    memcpy(&d[6], in->src, 6);
    memcpy(&d[12], in->type, 2);
    memcpy(&d[14], in->meta, 14);

    return CRC_M17(d, 28);
}

    m17_coder::sptr
    m17_coder::make(std::string src_id,std::string dst_id,short type,std::string meta, float samp_rate)
    {
      return gnuradio::get_initial_sptr
        (new m17_coder_impl(src_id,dst_id,type,meta,samp_rate));
    }

    /*
     * The private constructor
     */
    m17_coder_impl::m17_coder_impl(std::string src_id,std::string dst_id,short type,std::string meta, float samp_rate)
      : gr::block("m17_coder",
              gr::io_signature::make(1, 1, sizeof(char)),
              gr::io_signature::make(1, 1, sizeof(float)))
              ,_meta(meta), _samp_rate(samp_rate)
{    set_meta(meta);
     set_src_id(src_id);
     set_dst_id(dst_id);
     set_samp_rate(samp_rate);
     set_type(type);
     uint16_t ccrc=LSF_CRC(&lsf);
     lsf.crc[0]=ccrc>>8;
     lsf.crc[1]=ccrc&0xFF;
     _got_lsf=0;                  //have we filled the LSF struct yet?
     _fn=0;                      //16-bit Frame Number (for the stream mode)

}

void m17_coder_impl::set_samp_rate(float samp_rate)
{_samp_rate=samp_rate;
 printf("New sampling rate: %f\n",_samp_rate); 
}

void m17_coder_impl::set_src_id(std::string src_id)
{for (int i=0;i<6;i++) {_src_id[i]=0;}
 sscanf(src_id.c_str(), "%hhu.%hhu.%hhu.%hhu.%hhu.%hhu", &_src_id[0], &_src_id[1], &_src_id[2], &_src_id[3],&_src_id[4],&_src_id[5]);
 for (int i=0;i<6;i++) {lsf.src[i]=_src_id[i];}
 printf("new SRC ID: %hhu %hhu %hhu %hhu %hhu %hhu\n",_src_id[0],_src_id[1],_src_id[2],_src_id[3],_src_id[4],_src_id[5]);fflush(stdout);
}

void m17_coder_impl::set_dst_id(std::string dst_id)
{for (int i=0;i<6;i++) {_dst_id[i]=0;}
 sscanf(dst_id.c_str(), "%hhu.%hhu.%hhu.%hhu.%hhu.%hhu", &_dst_id[0], &_dst_id[1], &_dst_id[2], &_dst_id[3],&_dst_id[4],&_dst_id[5]);
 for (int i=0;i<6;i++) {lsf.dst[i]=_dst_id[i];}
 printf("new DST ID: %hhu %hhu %hhu %hhu %hhu %hhu\n",_dst_id[0],_dst_id[1],_dst_id[2],_dst_id[3],_dst_id[4],_dst_id[5]);fflush(stdout);
}

void m17_coder_impl::set_meta(std::string meta)
{int length;
 printf("new meta: %s\n",meta.c_str());fflush(stdout);
 _meta=meta;
 if (meta.length()<14) length=meta.length(); else length=14;
 for (int i=0;i<length;i++) {lsf.meta[i]=_meta[i];}
}

void m17_coder_impl::set_type(short type)
{_type=type;
 lsf.type[0]=_type&0xff;
 lsf.type[1]=_type>>8;
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
      ninput_items_required[0] = noutput_items/24; // TODO JMF (if 16 inputs -> 384 outputs)
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
int countout=0;

uint8_t enc_bits[SYM_PER_PLD*2];    //type-2 bits, unpacked
uint8_t rf_bits[SYM_PER_PLD*2];     //type-4 bits, unpacked
uint8_t lich[6];                    //48 bits packed raw, unencoded LICH
uint8_t lich_encoded[12];           //96 bits packed, encoded LICH

uint8_t data[16];                   //raw payload, packed bits
uint8_t lich_cnt=0;                 //0..5 LICH counter, derived from the Frame Number

do {
    if (countin+16<=noutput_items) 
       {if(_got_lsf) //stream frames
          {
            //we could discard the data we already have
//	    for (int i=0;i<6;i++) {lsf.dst[i]=in[countin];countin++;}
//	    for (int i=0;i<6;i++) {lsf.src[i]=in[countin];countin++;}
//	    for (int i=0;i<2;i++) {lsf.type[i]=in[countin];countin++;}
//	    for (int i=0;i<14;i++) {lsf.meta[i]=in[countin];countin++;}
	    for (int i=0;i<16;i++) {data[i]=in[countin];countin++;}

            //send stream frame syncword
            send_Syncword(SYNC_STR,out,&countout);

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
            conv_Encode_Frame(&enc_bits[96], data, _fn);

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
            _fn++;

            //debug-only
            if(_fn==6*10)
                return 0;
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
printf("got_lsf=1\n");

            //encode LSF data
            conv_Encode_LSF(enc_bits, &lsf);

            //send out the preamble and LSF
            send_Preamble(0,out,&countout,_samp_rate); //0 - LSF preamble, as opposed to 1 - BERT preamble

            //send LSF syncword
            send_Syncword(SYNC_LSF,out,&countout);

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

            //send dummy symbols (debug)
            /*float s=0.0;
            for(uint8_t i=0; i<184; i++) //40ms * 4800 - 8 (syncword)
                write(STDOUT_FILENO, (uint8_t*)&s, sizeof(float));*/

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
    } while (countin+16<=noutput_items);
      // Tell runtime system how many input items we consumed on
      // each input stream.
    consume_each (countin);
//    printf("\nnoutput_items=%d countin=%d countout=%d\n",noutput_items,countin,countout);
      // Tell runtime system how many output items we produced.
      return countout;
    }

  } /* namespace m17 */
} /* namespace gr */
