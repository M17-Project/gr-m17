//--------------------------------------------------------------------
// M17 C library - phy/sync.c
//
// Wojciech Kaczmarski, SP5WWP
// M17 Project, 29 December 2023
//--------------------------------------------------------------------
#include <m17.h>

//syncwords
const uint16_t SYNC_LSF = 0x55F7;
const uint16_t SYNC_STR = 0xFF5D;
const uint16_t SYNC_PKT = 0x75FF;
const uint16_t SYNC_BER = 0xDF55;
const uint16_t EOT_MRKR = 0x555D;
