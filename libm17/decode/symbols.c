//--------------------------------------------------------------------
// M17 C library - decode/symbols.c
//
// Wojciech Kaczmarski, SP5WWP
// M17 Project, 29 December 2023
//--------------------------------------------------------------------
#include <m17.h>

// syncword patterns (RX) - computed at runtime
int8_t lsf_sync_symbols[8];
int8_t str_sync_symbols[8];
int8_t pkt_sync_symbols[8];

// Initialize sync symbols at runtime
void init_sync_symbols(void) {
    // LSF sync symbols
    lsf_sync_symbols[0] = +3; lsf_sync_symbols[1] = +3; lsf_sync_symbols[2] = +3; lsf_sync_symbols[3] = +3;
    lsf_sync_symbols[4] = -3; lsf_sync_symbols[5] = -3; lsf_sync_symbols[6] = +3; lsf_sync_symbols[7] = -3;
    
    // STR sync symbols
    str_sync_symbols[0] = -3; str_sync_symbols[1] = -3; str_sync_symbols[2] = -3; str_sync_symbols[3] = -3;
    str_sync_symbols[4] = +3; str_sync_symbols[5] = +3; str_sync_symbols[6] = -3; str_sync_symbols[7] = +3;
    
    // PKT sync symbols
    pkt_sync_symbols[0] = +3; pkt_sync_symbols[1] = -3; pkt_sync_symbols[2] = +3; pkt_sync_symbols[3] = +3;
    pkt_sync_symbols[4] = -3; pkt_sync_symbols[5] = -3; pkt_sync_symbols[6] = -3; pkt_sync_symbols[7] = -3;
}
