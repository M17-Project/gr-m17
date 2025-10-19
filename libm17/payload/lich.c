//--------------------------------------------------------------------
// M17 C library - payload/lich.c
//
// This file contains:
// - Link Information CHannel (LICH) repacking functions
//
// Wojciech Kaczmarski, SP5WWP
// M17 Project, 8 January 2024
//--------------------------------------------------------------------
#include <string.h>
#include <m17.h>

/**
 * @brief Extract LICH from the whole LSF.
 * 
 * @param outp 6-byte array for the LICH.
 * @param cnt LICH counter (0 to 5)
 * @param inp Pointer to an LSF struct.
 */
void extract_LICH(uint8_t outp[6], const uint8_t cnt, const lsf_t* inp)
{
    switch(cnt)
    {
        case 0:
            outp[0]=inp->dst[0];
            outp[1]=inp->dst[1];
            outp[2]=inp->dst[2];
            outp[3]=inp->dst[3];
            outp[4]=inp->dst[4];
        break;

        case 1:
            outp[0]=inp->dst[5];
            outp[1]=inp->src[0];
            outp[2]=inp->src[1];
            outp[3]=inp->src[2];
            outp[4]=inp->src[3];
        break;

        case 2:
            outp[0]=inp->src[4];
            outp[1]=inp->src[5];
            outp[2]=inp->type[0];
            outp[3]=inp->type[1];
            outp[4]=inp->meta[0];
        break;

        case 3:
            outp[0]=inp->meta[1];
            outp[1]=inp->meta[2];
            outp[2]=inp->meta[3];
            outp[3]=inp->meta[4];
            outp[4]=inp->meta[5];
        break;

        case 4:
            outp[0]=inp->meta[6];
            outp[1]=inp->meta[7];
            outp[2]=inp->meta[8];
            outp[3]=inp->meta[9];
            outp[4]=inp->meta[10];
        break;

        case 5:
            outp[0]=inp->meta[11];
            outp[1]=inp->meta[12];
            outp[2]=inp->meta[13];
            outp[3]=inp->crc[0];
            outp[4]=inp->crc[1];
        break;

        default:
            ;
        break;
    }

    outp[5]=cnt<<5;
}

/**
 * @brief Unpack LICH bytes.
 * 
 * @param out Unpacked, encoded LICH bits (array of at least 96 bytes).
 * @param in 12-byte (96 bits) encoded LICH, packed.
 */
void unpack_LICH(uint8_t *out, const uint8_t in[12])
{
    for(uint8_t i=0; i<12; i++)
    {
        for(uint8_t j=0; j<8; j++)
            out[i*8+j]=(in[i]>>(7-j))&1;
    }
}
