//--------------------------------------------------------------------
// M17 C library - encode/symbols.c
//
// Wojciech Kaczmarski, SP5WWP
// M17 Project, 8 January 2024
//--------------------------------------------------------------------
#include <m17.h>

//dibits-symbols map (TX)
const int8_t symbol_map[4]={+1, +3, -1, -3};

//symbol list (RX)
const int8_t symbol_list[4]={-3, -1, +1, +3};

//End of Transmission symbol pattern
const int8_t eot_symbols[8]={+3, +3, +3, +3, +3, +3, -3, +3};
