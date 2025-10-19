//--------------------------------------------------------------------
// M17 C library - payload/call.c
//
// This file contains:
// - callsign encoders and decoders
//
// Wojciech Kaczmarski, SP5WWP
// M17 Project, 24 September 2024
//--------------------------------------------------------------------
#include <stdio.h>
#include <string.h>
#include <m17.h>

/**
 * @brief Decode a 48-bit value (stored as uint64_t) into callsign string.
 * 
 * @param outp Decoded callsign string (null-terminated). Must be at least 10 bytes long and pre-allocated by the caller.
 * @param inp Encoded value.
 */
void decode_callsign_value(uint8_t *outp, const uint64_t inp)
{
    uint64_t encoded=inp;
    uint8_t start=0; //where to put the first decoded, non-# character

	//address range check
	if(encoded>=U40_9)
	{
        if(encoded==0xFFFFFFFFFFFF) //broadcast special address
        {
            sprintf((char*)outp, "@ALL");
            return;
        }
        else if(encoded<=U40_9_8) //#-address range
        {
            start=1;
            encoded-=U40_9;
            outp[0]='#';
        }
        else //reserved address range
        {
            return;
        }
	}

	//decode the callsign
	uint8_t i=start;
	while(encoded>0)
	{
		outp[i]=CHAR_MAP[encoded%40];
		encoded/=40;
		i++;
	}
	outp[i]=0;
}

/**
 * @brief Decode a 6-byte long array (big-endian) into callsign string.
 * 
 * @param outp Decoded callsign string (null-terminated). Must be at least 10 bytes long and pre-allocated by the caller.
 * @param inp Pointer to a byte array holding the encoded value (big-endian).
 */
void decode_callsign_bytes(uint8_t *outp, const uint8_t inp[6])
{
	uint64_t encoded=0;

	//repack the data to a uint64_t
	for(uint8_t i=0; i<6; i++)
		encoded|=(uint64_t)inp[5-i]<<(8*i);

	decode_callsign_value(outp, encoded);
}

/**
 * @brief Encode callsign string into a 48-bit value, stored as uint64_t.
 * 
 * @param out Pointer to a uint64_t variable for the encoded value.
 * @param inp Callsign string (null-terminated). Maximum 9 characters long (excluding null terminator).
 * @return int8_t Return value, 0 -> OK.
 */
int8_t encode_callsign_value(uint64_t *out, const uint8_t *inp)
{
    //assert inp length
    if(strlen((const char*)inp)>9)
        return -1;

    const uint8_t charMap[40]=CHAR_MAP;

    uint64_t tmp=0;
    uint8_t start=0; //where's the first char of the address? this excludes the leading #, if present

    //a special address that's encoded differently
    if(strcmp((const char*)inp, "@ALL")==0)
    {
        *out=0xFFFFFFFFFFFF;
        return 0;
    }

    //check if the address is in the hash-space
    if(inp[0]=='#')
        start=1;

    for(int8_t i=strlen((const char*)inp)-1; i>=start; i--)
    {
        for(uint8_t j=0; j<40; j++)
        {
            if(inp[i]==charMap[j])
            {
                tmp=tmp*40+j;
                break;
            }
        }
    }

    if(start) //starts with a hash?
        tmp+=U40_9; //40^9

    *out=tmp;
    return 0;
}

/**
 * @brief Encode callsign string and store in a 6-byte array (big-endian)
 * 
 * @param out Pointer to a byte array for the encoded value (big-endian).
 * @param inp Callsign string (null-terminated). Maximum 9 characters long (excluding null terminator).
 * @return int8_t Return value, 0 -> OK.
 */
int8_t encode_callsign_bytes(uint8_t out[6], const uint8_t *inp)
{
    uint64_t tmp=0;

    if(encode_callsign_value(&tmp, inp))
    {
        return -1;
    }

	for(uint8_t i=0; i<6; i++)
    	out[5-i]=(tmp>>(8*i))&0xFF;
    	
    return 0;
}
