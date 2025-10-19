//--------------------------------------------------------------------
// M17 C library - m17.c
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 12 March 2025
//--------------------------------------------------------------------
#include <string.h>
#include <m17.h>
#include <m17_safe.h>

__attribute__((visibility("default")))
const char libm17_version[] = LIBM17_VERSION;

/**
 * @brief Generate symbol stream for a preamble.
 * 
 * @param out Frame buffer (192 floats).
 * @param cnt Pointer to a variable holding the number of written symbols.
 * @param type Preamble type (pre-BERT or pre-LSF).
 */
void gen_preamble(float out[SYM_PER_FRA], uint32_t *cnt, const pream_t type)
{
    // Input validation
    if (out == NULL || cnt == NULL) {
        return;
    }
    
    if (*cnt >= SYM_PER_FRA) {
        return; // Buffer overflow protection
    }
    
    if(type==PREAM_BERT) //pre-BERT
    {
        for(uint16_t i=0; i<SYM_PER_FRA/2 && (*cnt) < SYM_PER_FRA; i++) //40ms * 4800 = 192
        {
            out[(*cnt)++]=-3.0;
            if ((*cnt) < SYM_PER_FRA) {
                out[(*cnt)++]=+3.0;
            }
        }
    }
    else// if(type==PREAM_LSF) //pre-LSF
    {
        for(uint16_t i=0; i<SYM_PER_FRA/2 && (*cnt) < SYM_PER_FRA; i++) //40ms * 4800 = 192
        {
            out[(*cnt)++]=+3.0;
            if ((*cnt) < SYM_PER_FRA) {
                out[(*cnt)++]=-3.0;
            }
        }
    }
}

/**
 * @brief Generate symbol stream for a preamble.
 * 
 * @param out Frame buffer (192 int8_t).
 * @param cnt Pointer to a variable holding the number of written symbols.
 * @param type Preamble type (pre-BERT or pre-LSF).
 */
void gen_preamble_i8(int8_t out[SYM_PER_FRA], uint32_t *cnt, const pream_t type)
{
    if(type==PREAM_BERT) //pre-BERT
    {
        for(uint16_t i=0; i<SYM_PER_FRA/2; i++) //40ms * 4800 = 192
        {
            out[(*cnt)++]=-3;
            out[(*cnt)++]=+3;
        }
    }
    else// if(type==PREAM_LSF) //pre-LSF
    {
        for(uint16_t i=0; i<SYM_PER_FRA/2; i++) //40ms * 4800 = 192
        {
            out[(*cnt)++]=+3;
            out[(*cnt)++]=-3;
        }
    }
}

/**
 * @brief Generate symbol stream for a syncword.
 * 
 * @param out Output buffer (8 floats).
 * @param cnt Pointer to a variable holding the number of written symbols.
 * @param syncword Syncword.
 */
void gen_syncword(float out[SYM_PER_SWD], uint32_t *cnt, const uint16_t syncword)
{
    for(uint8_t i=0; i<SYM_PER_SWD*2; i+=2)
    {
        out[(*cnt)++]=symbol_map[(syncword>>(14-i))&3];
    }
}

/**
 * @brief Generate symbol stream for a syncword.
 * 
 * @param out Output buffer (8 int8_t).
 * @param cnt Pointer to a variable holding the number of written symbols.
 * @param syncword Syncword.
 */
void gen_syncword_i8(int8_t out[SYM_PER_SWD], uint32_t *cnt, const uint16_t syncword)
{
    for(uint8_t i=0; i<SYM_PER_SWD*2; i+=2)
    {
        out[(*cnt)++]=symbol_map[(syncword>>(14-i))&3];
    }
}

/**
 * @brief Generate symbol stream for frame contents (without the syncword).
 * Can be used for both LSF and data frames.
 * 
 * @param out Output buffer (184 floats).
 * @param cnt Pointer to a variable holding the number of written symbols.
 * @param in Data input - unpacked bits (1 bit per byte).
 */
void gen_data(float out[SYM_PER_PLD], uint32_t *cnt, const uint8_t* in)
{
    for(uint16_t i=0; i<SYM_PER_PLD; i++) //40ms * 4800 - 8 (syncword)
    {
        out[(*cnt)++]=symbol_map[in[2*i]*2+in[2*i+1]];
    }
}

/**
 * @brief Generate symbol stream for frame contents (without the syncword).
 * Can be used for both LSF and data frames.
 * 
 * @param out Output buffer (184 int8_t).
 * @param cnt Pointer to a variable holding the number of written symbols.
 * @param in Data input - unpacked bits (1 bit per byte).
 */
void gen_data_i8(int8_t out[SYM_PER_PLD], uint32_t *cnt, const uint8_t* in)
{
    for(uint16_t i=0; i<SYM_PER_PLD; i++) //40ms * 4800 - 8 (syncword)
    {
        out[(*cnt)++]=symbol_map[in[2*i]*2+in[2*i+1]];
    }
}

/**
 * @brief Generate symbol stream for the End of Transmission marker.
 * 
 * @param out Output buffer (192 floats).
 * @param cnt Pointer to a variable holding the number of written symbols.
 */
void gen_eot(float out[SYM_PER_FRA], uint32_t *cnt)
{
    for(uint16_t i=0; i<SYM_PER_FRA; i++) //40ms * 4800 = 192
    {
        out[(*cnt)++]=eot_symbols[i%8];
    }
}

/**
 * @brief Generate symbol stream for the End of Transmission marker.
 * 
 * @param out Output buffer (192 int8_t).
 * @param cnt Pointer to a variable holding the number of written symbols.
 */
void gen_eot_i8(int8_t out[SYM_PER_FRA], uint32_t *cnt)
{
    for(uint16_t i=0; i<SYM_PER_FRA; i++) //40ms * 4800 = 192
    {
        out[(*cnt)++]=eot_symbols[i%8];
    }
}

/**
 * @brief Generate frame symbols.
 * 
 * @param out Output buffer for symbols (192 floats).
 * @param data Payload (16 or 25 bytes).
 * @param type Frame type (LSF, Stream, Packet).
 * @param lsf Pointer to a structure holding Link Setup Frame data.
 * @param lich_cnt LICH counter (0..5).
 * @param fn Frame number.
 */
void gen_frame(float out[SYM_PER_FRA], const uint8_t* data, const frame_t type, const lsf_t* lsf, const uint8_t lich_cnt, const uint16_t fn)
{
    uint8_t lich[6];                    //48 bits packed raw, unencoded LICH
    uint8_t lich_encoded[12];           //96 bits packed, encoded LICH
    uint8_t enc_bits[SYM_PER_PLD*2];    //type-2 bits, unpacked
    uint8_t rf_bits[SYM_PER_PLD*2];     //type-4 bits, unpacked
    uint32_t sym_cnt=0;                 //symbols written counter

    if(type==FRAME_LSF)
    {
        gen_syncword(out, &sym_cnt, SYNC_LSF);
        conv_encode_LSF(enc_bits, lsf);
    }
    else if(type==FRAME_STR)
    {
        gen_syncword(out, &sym_cnt, SYNC_STR);
        extract_LICH(lich, lich_cnt, lsf);
        encode_LICH(lich_encoded, lich);
        unpack_LICH(enc_bits, lich_encoded);
        conv_encode_stream_frame(&enc_bits[96], data, fn); //stream frames require 16-byte payloads
    }
    else if(type==FRAME_PKT)
    {
        gen_syncword(out, &sym_cnt, SYNC_PKT);
        conv_encode_packet_frame(enc_bits, data); //packet frames require 200-bit payload chunks plus a 6-bit counter
    }
	else if(type==FRAME_BERT)
    {
        gen_syncword(out, &sym_cnt, SYNC_BER);
        conv_encode_bert_frame(enc_bits, data); //BERT frames require 197 BERT bits packed as 25 bytes
    }

    //common stuff
    reorder_bits(rf_bits, enc_bits);
    randomize_bits(rf_bits);
    gen_data(out, &sym_cnt, rf_bits);
}

/**
 * @brief Generate frame symbols.
 * 
 * @param out Output buffer for symbols (192 int8_t).
 * @param data Payload (16 or 25 bytes).
 * @param type Frame type (LSF, Stream, Packet).
 * @param lsf Pointer to a structure holding Link Setup Frame data.
 * @param lich_cnt LICH counter (0..5).
 * @param fn Frame number.
 */
void gen_frame_i8(int8_t out[SYM_PER_FRA], const uint8_t* data, const frame_t type, const lsf_t* lsf, const uint8_t lich_cnt, const uint16_t fn)
{
    uint8_t lich[6];                    //48 bits packed raw, unencoded LICH
    uint8_t lich_encoded[12];           //96 bits packed, encoded LICH
    uint8_t enc_bits[SYM_PER_PLD*2];    //type-2 bits, unpacked
    uint8_t rf_bits[SYM_PER_PLD*2];     //type-4 bits, unpacked
    uint32_t sym_cnt=0;                 //symbols written counter

    if(type==FRAME_LSF)
    {
        gen_syncword_i8(out, &sym_cnt, SYNC_LSF);
        conv_encode_LSF(enc_bits, lsf);
    }
    else if(type==FRAME_STR)
    {
        gen_syncword_i8(out, &sym_cnt, SYNC_STR);
        extract_LICH(lich, lich_cnt, lsf);
        encode_LICH(lich_encoded, lich);
        unpack_LICH(enc_bits, lich_encoded);
        conv_encode_stream_frame(&enc_bits[96], data, fn); //stream frames require 16-byte payloads
    }
    else if(type==FRAME_PKT)
    {
        gen_syncword_i8(out, &sym_cnt, SYNC_PKT);
        conv_encode_packet_frame(enc_bits, data); //packet frames require 200-bit payload chunks plus a 6-bit counter
    }
    else if(type==FRAME_BERT)
    {
        gen_syncword_i8(out, &sym_cnt, SYNC_BER);
        conv_encode_bert_frame(enc_bits, data); //BERT frames require 197 BERT bits packed as 25 bytes
    }

    //common stuff
    reorder_bits(rf_bits, enc_bits);
    randomize_bits(rf_bits);
    gen_data_i8(out, &sym_cnt, rf_bits);
}

/**
 * @brief Decode the Link Setup Frame from a symbol stream.
 *
 * @param lsf Pointer to an LSF struct.
 * @param pld_symbs Input 184 symbols represented as floats: {-3, -1, +1, +3}.
 * @return uint32_t Viterbi metric for the payload.
 */
uint32_t decode_LSF(lsf_t* lsf, const float pld_symbs[SYM_PER_PLD])
{
	uint8_t lsf_b[30+1];
	uint16_t soft_bit[2*SYM_PER_PLD];
	uint16_t d_soft_bit[2*SYM_PER_PLD];
	uint32_t e;

	slice_symbols(soft_bit, pld_symbs);
	randomize_soft_bits(soft_bit);
	reorder_soft_bits(d_soft_bit, soft_bit);

	e = viterbi_decode_punctured(lsf_b, d_soft_bit, puncture_pattern_1, 2*SYM_PER_PLD, sizeof(puncture_pattern_1));

	//copy over the data starting at byte 1 (byte 0 needs to be omitted)
	memcpy(lsf->dst, &lsf_b[1+0], 6);		//DST field
	memcpy(lsf->src, &lsf_b[1+6], 6);		//SRC field
	lsf->type[0]=lsf_b[1+12];				//TYPE field
	lsf->type[1]=lsf_b[1+13];
	memcpy(lsf->meta, &lsf_b[1+14], 14);	//META field
	lsf->crc[0]=lsf_b[1+28];				//CRC field
	lsf->crc[1]=lsf_b[1+29];

	return e; //return Viterbi error metric
}

/**
 * @brief Decode a single Stream Frame from a symbol stream.
 *
 * @param frame_data Pointer to a 16-byte array for the decoded payload.
 * @param lich Pointer to a 5-byte array for the decoded LICH data chunk.
 * @param fn Pointer to a uint16_t variable for the Frame Number.
 * @param lich_cnt Pointer to a uint8_t variable for the LICH Counter.
 * @param pld_symbs Input 184 symbols represented as floats: {-3, -1, +1, +3}.
 * @return uint32_t Viterbi metric for the payload.
 */
uint32_t decode_str_frame(uint8_t frame_data[16], uint8_t lich[5], uint16_t* fn, uint8_t* lich_cnt, const float pld_symbs[SYM_PER_PLD])
{
	uint16_t soft_bit[2*SYM_PER_PLD];
	uint16_t d_soft_bit[2*SYM_PER_PLD];
	uint8_t tmp_frame_data[(16+128)/8+1]; //1 byte extra for flushing
	uint32_t e;

	slice_symbols(soft_bit, pld_symbs);
	randomize_soft_bits(soft_bit);
	reorder_soft_bits(d_soft_bit, soft_bit);

	//decode LICH
    uint8_t tmp[6];
	decode_LICH(tmp, d_soft_bit);
    memcpy(lich, tmp, 5);

	if(lich_cnt!=NULL) *lich_cnt = tmp[5]>>5;

	e = viterbi_decode_punctured(tmp_frame_data, &d_soft_bit[96], puncture_pattern_2, 2*SYM_PER_PLD-96, sizeof(puncture_pattern_2));
	
	//shift 1+2 positions left - get rid of the encoded flushing bits and FN
    memcpy(frame_data, &tmp_frame_data[1+2], 16);

	if(fn!=NULL) *fn = (tmp_frame_data[1]<<8)|tmp_frame_data[2];

	return e;
}

/**
 * @brief Decode a single Packet Frame from a symbol stream.
 *
 * @param frame_data Pointer to a 25-byte array for the decoded payload.
 * @param eof Pointer to a uint8_t variable for the End of Frame marker.
 * @param fn Pointer to a uint8_t variable for the Frame Number.
 * @param pld_symbs Input 184 symbols represented as floats: {-3, -1, +1, +3}.
 * @return uint32_t Viterbi metric for the payload.
 */
uint32_t decode_pkt_frame(uint8_t frame_data[25], uint8_t* eof, uint8_t* fn, const float pld_symbs[SYM_PER_PLD])
{
	uint16_t soft_bit[2*SYM_PER_PLD];
	uint16_t d_soft_bit[2*SYM_PER_PLD];
	uint8_t tmp_frame_data[26+1]; //1 byte extra for flushing
	uint32_t e;

	slice_symbols(soft_bit, pld_symbs);
	randomize_soft_bits(soft_bit);
	reorder_soft_bits(d_soft_bit, soft_bit);

	e = viterbi_decode_punctured(tmp_frame_data, d_soft_bit, puncture_pattern_3, 2*SYM_PER_PLD, sizeof(puncture_pattern_3));
	
	//shift 1 position left - get rid of the encoded flushing bits
    memcpy(frame_data, &tmp_frame_data[1], 25);

	if(fn!=NULL) *fn = (tmp_frame_data[26]>>2)&0x1F;
    if(eof!=NULL) *eof = tmp_frame_data[26]>>7;

	return e;
}
