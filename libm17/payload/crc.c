//--------------------------------------------------------------------
// M17 C library - payload/crc.c
//
// This file contains:
// - CRC calculating functions (arbitrary length)
//
// Wojciech Kaczmarski, SP5WWP
// M17 Project, 29 December 2023
//--------------------------------------------------------------------
#include <string.h>
#include <m17.h>

//M17 CRC polynomial
const uint16_t M17_CRC_POLY = 0x5935;

/**
 * @brief Calculate CRC value.
 *
 * @param in Pointer to the input byte array.
 * @param len Input's length.
 * @return uint16_t CRC value.
 */
uint16_t CRC_M17(const uint8_t* in, const uint16_t len)
{
	uint32_t crc=0xFFFF; //init val

	for(uint16_t i=0; i<len; i++)
	{
		crc^=in[i]<<8;
		for(uint8_t j=0; j<8; j++)
		{
			crc<<=1;
			if(crc&0x10000)
				crc=(crc^M17_CRC_POLY)&0xFFFF;
		}
	}

	return crc&(0xFFFF);
}

/**
 * @brief Calculate CRC value for the Link Setup Frame.
 *
 * @param in Pointer to an LSF struct.
 * @return uint16_t CRC value.
 */
uint16_t LSF_CRC(const lsf_t* in)
{
    uint8_t d[28];

    memcpy(&d[0], in->dst, 6);
    memcpy(&d[6], in->src, 6);
    memcpy(&d[12], in->type, 2);
    memcpy(&d[14], in->meta, 14);

    return CRC_M17(d, 28);
}
