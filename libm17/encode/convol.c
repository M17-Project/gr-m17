//--------------------------------------------------------------------
// M17 C library - encode/convol.c
//
// This file contains:
// - convolutional encoders
//   for the LSF, stream, packet, and BERT frames
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 12 March 2025
//--------------------------------------------------------------------
#include <string.h>
#include <m17.h>

/**
 * @brief P_1 puncture pattern for Link Setup Frames (LSF).
 */
const uint8_t puncture_pattern_1[61] = {
    1,1,0,1,1,1,0,1,1,1,0,1,1,1,0,1,1,
      1,0,1,1,1,0,1,1,1,0,1,1,1,0,1,1,
      1,0,1,1,1,0,1,1,1,0,1,1,1,0,1,1,
      1,0,1,1,1,0,1,1,1,0,1,1
};

/**
 * @brief P_2 puncture pattern for stream frames (excluding Link Information Channel - LICH).
 */
const uint8_t puncture_pattern_2[12]={1,1,1,1,1,1,1,1,1,1,1,0};

/**
 * @brief P_3 puncture pattern for packet frames.
 */
const uint8_t puncture_pattern_3[8]={1,1,1,1,1,1,1,0};

/**
 * @brief Encode M17 stream frame using convolutional encoder with puncturing.
 *
 * @param out Output - unpacked array of bits, 272 type-3 bits.
 * @param in Input - packed array of uint8_t, 144 type-1 bits.
 * @param fn Input - 16-bit frame number.
 */
void conv_encode_stream_frame(uint8_t* out, const uint8_t* in, const uint16_t fn)
{
	uint8_t pp_len = sizeof(puncture_pattern_2);
	uint8_t p=0;			//puncturing pattern index
	uint16_t pb=0;			//pushed punctured bits
	uint8_t ud[4+144+4];	//unpacked data

	memset(ud, 0, sizeof(ud));

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

		if(puncture_pattern_2[p])
		{
			out[pb]=G1;
			pb++;
		}

		p++;
		p%=pp_len;

		if(puncture_pattern_2[p])
		{
			out[pb]=G2;
			pb++;
		}

		p++;
		p%=pp_len;
	}

	//printf("pb=%d\n", pb);
}

/**
 * @brief Encode M17 packet frame using convolutional encoder with puncturing.
 *
 * @param out Output - unpacked array of bits, 368 type-3 bits.
 * @param in Input - packed array of uint8_t, 206 type-1 bits
 *   (200 bits of data, 1 bit End of Frame indicator, 5 bits frame/byte counter).
 */
void conv_encode_packet_frame(uint8_t out[SYM_PER_PLD*2], const uint8_t in[26])
{
	uint8_t pp_len = sizeof(puncture_pattern_3);
	uint8_t p=0;			//puncturing pattern index
	uint16_t pb=0;			//pushed punctured bits
	uint8_t ud[4+206+4];	//unpacked data

	memset(ud, 0, sizeof(ud));

	//unpack data
	for(uint8_t i=0; i<26; i++)
	{
		for(uint8_t j=0; j<8; j++)
		{
			if(i<=24 || j<=5)
				ud[4+i*8+j]=(in[i]>>(7-j))&1;
		}
	}

	//encode
	for(uint8_t i=0; i<206+4; i++)
	{
		uint8_t G1=(ud[i+4]                +ud[i+1]+ud[i+0])%2;
		uint8_t G2=(ud[i+4]+ud[i+3]+ud[i+2]        +ud[i+0])%2;

		//fprintf(stderr, "%d%d", G1, G2);

		if(puncture_pattern_3[p])
		{
			out[pb]=G1;
			pb++;
		}

		p++;
		p%=pp_len;

		if(puncture_pattern_3[p])
		{
			out[pb]=G2;
			pb++;
		}

		p++;
		p%=pp_len;
	}

	//fprintf(stderr, "pb=%d\n", pb);
}

/**
 * @brief Encode M17 stream frame using convolutional encoder with puncturing.
 *
 * @param out Output - unpacked array of bits, 368 type-3 bits.
 * @param in Input - pointer to a struct holding the Link Setup Data and its 16-bit CRC.
 */
void conv_encode_LSF(uint8_t out[SYM_PER_PLD*2], const lsf_t* in)
{
	uint8_t pp_len = sizeof(puncture_pattern_1);
	uint8_t p=0;			//puncturing pattern index
	uint16_t pb=0;			//pushed punctured bits
	uint8_t ud[4+240+4];	//unpacked data

	memset(ud, 0, sizeof(ud));

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

		if(puncture_pattern_1[p])
		{
			out[pb]=G1;
			pb++;
		}

		p++;
		p%=pp_len;

		if(puncture_pattern_1[p])
		{
			out[pb]=G2;
			pb++;
		}

		p++;
		p%=pp_len;
	}

	//printf("pb=%d\n", pb);
}

/**
 * @brief Encode M17 BERT frame using convolutional encoder with puncturing.
 *
 * @param out Output - unpacked array of bits, 368 type-3 bits.
 * @param in Input - packed array of uint8_t, 197 type-1 bits.
 */
void conv_encode_bert_frame(uint8_t out[SYM_PER_PLD*2], const uint8_t in[25])
{
	uint8_t pp_len = sizeof(puncture_pattern_2);
	uint8_t p=0;			//puncturing pattern index
	uint16_t pb=0;			//pushed punctured bits
	uint8_t ud[4+197+4];	//unpacked data

	memset(ud, 0, sizeof(ud));

	// Un-packing bits
	for(uint8_t i=0; i<197; i++)
	{
		uint8_t byte_idx = i/8;
		uint8_t bit_idx = 7-(i%8);

		ud[4+i] = (in[byte_idx] >> bit_idx) & 1;
	}

	//encode
	for(uint8_t i=0; i<197+4; i++)
	{
		uint8_t G1=(ud[i+4]                +ud[i+1]+ud[i+0])%2;
		uint8_t G2=(ud[i+4]+ud[i+3]+ud[i+2]        +ud[i+0])%2;

		if(puncture_pattern_2[p])
		{
			out[pb]=G1;
			pb++;
		}

		p++;
		p%=pp_len;

		if(puncture_pattern_2[p])
		{
			out[pb]=G2;
			pb++;
		}

		p++;
		p%=pp_len;
	}
}
