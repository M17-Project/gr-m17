//--------------------------------------------------------------------
// M17 C library - decode/symbols.c
//
// Wojciech Kaczmarski, SP5WWP
// M17 Project, 29 December 2023
//--------------------------------------------------------------------
#include <m17.h>

// syncword patterns (RX)
// TODO: Compute those at runtime from the consts below
const int8_t lsf_sync_symbols[8]={+3, +3, +3, +3, -3, -3, +3, -3};
const int8_t str_sync_symbols[8]={-3, -3, -3, -3, +3, +3, -3, +3};
const int8_t pkt_sync_symbols[8]={+3, -3, +3, +3, -3, -3, -3, -3};
