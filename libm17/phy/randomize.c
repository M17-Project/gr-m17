//--------------------------------------------------------------------
// M17 C library - phy/randomize.c
//
// Wojciech Kaczmarski, SP5WWP
// M17 Project, 29 December 2023
//--------------------------------------------------------------------
#include <m17.h>

//randomizing pattern
const uint8_t rand_seq[46]=
{
    0xD6, 0xB5, 0xE2, 0x30, 0x82, 0xFF, 0x84, 0x62, 0xBA, 0x4E, 0x96, 0x90, 0xD8, 0x98, 0xDD, 0x5D, 0x0C, 0xC8, 0x52, 0x43, 0x91, 0x1D, 0xF8,
    0x6E, 0x68, 0x2F, 0x35, 0xDA, 0x14, 0xEA, 0xCD, 0x76, 0x19, 0x8D, 0xD5, 0x80, 0xD1, 0x33, 0x87, 0x13, 0x57, 0x18, 0x2D, 0x29, 0x78, 0xC3
};

/**
 * @brief Randomize type-4 unpacked bits.
 * 
 * @param inp Input 368 unpacked type-4 bits.
 */
void randomize_bits(uint8_t inp[SYM_PER_PLD*2])
{
    for(uint16_t i=0; i<SYM_PER_PLD*2; i++)
    {
        if((rand_seq[i/8]>>(7-(i%8)))&1) //flip bit if '1'
        {
            if(inp[i])
                inp[i]=0;
            else
                inp[i]=1;
        }
    }
}

/**
 * @brief Randomize type-4 soft bits.
 * 
 * @param inp Input 368 soft type-4 bits.
 */
void randomize_soft_bits(uint16_t inp[SYM_PER_PLD*2])
{
    for(uint16_t i=0; i<SYM_PER_PLD*2; i++)
    {
        if((rand_seq[i/8]>>(7-(i%8)))&1) //flip bit if '1'
        {
            inp[i]=soft_bit_NOT(inp[i]);
        }
    }
}
