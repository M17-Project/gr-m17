//--------------------------------------------------------------------
// FX.25 Forward Error Correction for AX.25
//
// Based on Dire Wolf implementation
// Provides Reed-Solomon FEC for AX.25 frames
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// FX.25 Constants
#define FX25_MAX_FRAME_SIZE     1024
#define FX25_RS_255_239         0x01    // Reed-Solomon (255,239)
#define FX25_RS_255_223         0x02    // Reed-Solomon (255,223)
#define FX25_RS_255_191         0x03    // Reed-Solomon (255,191)
#define FX25_RS_255_159         0x04    // Reed-Solomon (255,159)
#define FX25_RS_255_127         0x05    // Reed-Solomon (255,127)
#define FX25_RS_255_95          0x06    // Reed-Solomon (255,95)
#define FX25_RS_255_63          0x07    // Reed-Solomon (255,63)
#define FX25_RS_255_31          0x08    // Reed-Solomon (255,31)

// FX.25 Frame Structure
#define FX25_PREAMBLE_LEN       8       // Preamble length
#define FX25_SYNC_WORD_LEN      2       // Sync word length
#define FX25_HEADER_LEN         3       // Header length
#define FX25_CRC_LEN            2       // CRC length

// Reed-Solomon codec control block
struct fx25_rs {
    unsigned int mm;              // Bits per symbol
    unsigned int nn;              // Symbols per block (= (1<<mm)-1)
    unsigned char *alpha_to;      // log lookup table
    unsigned char *index_of;      // Antilog lookup table
    unsigned char *genpoly;       // Generator polynomial
    unsigned int nroots;          // Number of generator roots = number of parity symbols
    unsigned char fcr;            // First consecutive root, index form
    unsigned char prim;           // Primitive element, index form
    unsigned char iprim;          // prim-th root of 1, index form
};

// FX.25 Context
typedef struct {
    struct fx25_rs *rs;           // Reed-Solomon codec
    uint8_t rs_type;              // RS code type
    bool enabled;                 // FX.25 enabled
    uint32_t frames_encoded;      // Frames encoded
    uint32_t frames_decoded;      // Frames decoded
    uint32_t errors_corrected;    // Errors corrected
} fx25_context_t;

// FX.25 Frame
typedef struct {
    uint8_t preamble[FX25_PREAMBLE_LEN];
    uint8_t sync_word[FX25_SYNC_WORD_LEN];
    uint8_t header[FX25_HEADER_LEN];
    uint8_t data[FX25_MAX_FRAME_SIZE];
    uint16_t data_length;
    uint8_t parity[32];           // Reed-Solomon parity
    uint8_t parity_length;
    uint8_t crc[FX25_CRC_LEN];
} fx25_frame_t;

// Function Declarations

// Initialization
int fx25_init(fx25_context_t* ctx, uint8_t rs_type);
void fx25_cleanup(fx25_context_t* ctx);

// Reed-Solomon Operations
struct fx25_rs* fx25_rs_init(int symsize, int genpoly, int fcs, int prim, int nroots);
void fx25_rs_free(struct fx25_rs* rs);
int fx25_rs_encode(struct fx25_rs* rs, uint8_t* data, int data_len, uint8_t* parity);
int fx25_rs_decode(struct fx25_rs* rs, uint8_t* data, int data_len, uint8_t* parity, int nroots);

// FX.25 Frame Operations
int fx25_encode_frame(fx25_context_t* ctx, const uint8_t* ax25_data, uint16_t ax25_length, 
                     fx25_frame_t* fx25_frame);
int fx25_decode_frame(fx25_context_t* ctx, const fx25_frame_t* fx25_frame, 
                     uint8_t* ax25_data, uint16_t* ax25_length);
int fx25_detect_frame(const uint8_t* data, uint16_t length);
int fx25_extract_frame(const uint8_t* data, uint16_t length, fx25_frame_t* frame);

// Utility Functions
uint16_t fx25_calculate_crc(const uint8_t* data, uint16_t length);
bool fx25_verify_crc(const uint8_t* data, uint16_t length, uint16_t crc);
void fx25_generate_preamble(uint8_t* preamble);
bool fx25_verify_sync_word(const uint8_t* sync_word);

// Statistics
void fx25_get_stats(const fx25_context_t* ctx, uint32_t* encoded, uint32_t* decoded, uint32_t* errors);

#ifdef __cplusplus
}
#endif
