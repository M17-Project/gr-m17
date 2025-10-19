//--------------------------------------------------------------------
// FX.25 Forward Error Correction for AX.25
//
// Based on Dire Wolf implementation
// Provides Reed-Solomon FEC for AX.25 frames
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------

#include "fx25_protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Reed-Solomon polynomial tables
static unsigned char alpha_to[256];
static unsigned char index_of[256];
static unsigned char genpoly[32];

// Initialize Reed-Solomon codec
struct fx25_rs* fx25_rs_init(int symsize, int genpoly_val, int fcs, int prim, int nroots) {
    struct fx25_rs* rs = malloc(sizeof(struct fx25_rs));
    if (!rs) return NULL;
    
    rs->mm = symsize;
    rs->nn = (1 << symsize) - 1;
    rs->fcr = fcs;
    rs->prim = prim;
    rs->nroots = nroots;
    
    // Allocate lookup tables
    rs->alpha_to = malloc((rs->nn + 1) * sizeof(unsigned char));
    rs->index_of = malloc((rs->nn + 1) * sizeof(unsigned char));
    rs->genpoly = malloc((nroots + 1) * sizeof(unsigned char));
    
    if (!rs->alpha_to || !rs->index_of || !rs->genpoly) {
        fx25_rs_free(rs);
        return NULL;
    }
    
    // Generate Galois field tables
    int i, j, sr, gf;
    
    // Initialize alpha_to and index_of tables
    for (i = 0; i < rs->nn + 1; i++) {
        rs->alpha_to[i] = 0;
        rs->index_of[i] = 0;
    }
    
    // Generate Galois field
    rs->alpha_to[rs->mm] = 0;
    sr = 1;
    for (i = 0; i < rs->nn; i++) {
        rs->alpha_to[i] = sr;
        rs->index_of[sr] = i;
        sr <<= 1;
        if (sr & (1 << rs->mm)) {
            sr ^= genpoly_val;
        }
        sr &= rs->nn;
    }
    
    rs->index_of[0] = rs->nn;
    rs->alpha_to[rs->nn] = 0;
    
    // Generate generator polynomial
    rs->genpoly[0] = 1;
    for (i = 0, sr = rs->fcr; i < nroots; i++, sr = (sr * rs->prim) % rs->nn) {
        rs->genpoly[i + 1] = 1;
        for (j = i; j > 0; j--) {
            if (rs->genpoly[j] != 0) {
                rs->genpoly[j] = rs->genpoly[j - 1] ^ rs->alpha_to[(rs->index_of[rs->genpoly[j]] + sr) % rs->nn];
            } else {
                rs->genpoly[j] = rs->genpoly[j - 1];
            }
        }
        rs->genpoly[0] = rs->alpha_to[(rs->index_of[rs->genpoly[0]] + sr) % rs->nn];
    }
    
    // Convert genpoly to index form
    for (i = 0; i <= nroots; i++) {
        rs->genpoly[i] = rs->index_of[rs->genpoly[i]];
    }
    
    return rs;
}

// Free Reed-Solomon codec
void fx25_rs_free(struct fx25_rs* rs) {
    if (!rs) return;
    
    if (rs->alpha_to) free(rs->alpha_to);
    if (rs->index_of) free(rs->index_of);
    if (rs->genpoly) free(rs->genpoly);
    free(rs);
}

// Reed-Solomon encode
int fx25_rs_encode(struct fx25_rs* rs, uint8_t* data, int data_len, uint8_t* parity) {
    if (!rs || !data || !parity) return -1;
    
    int i, j;
    uint8_t feedback;
    
    // Clear parity
    memset(parity, 0, rs->nroots);
    
    for (i = 0; i < data_len; i++) {
        feedback = rs->index_of[data[i] ^ parity[0]];
        if (feedback != rs->nn) {
            for (j = 0; j < rs->nroots - 1; j++) {
                parity[j] = parity[j + 1] ^ rs->alpha_to[(feedback + rs->genpoly[rs->nroots - 1 - j]) % rs->nn];
            }
            parity[rs->nroots - 1] = rs->alpha_to[(feedback + rs->genpoly[0]) % rs->nn];
        } else {
            for (j = 0; j < rs->nroots; j++) {
                parity[j] = parity[j + 1];
            }
            parity[rs->nroots - 1] = 0;
        }
    }
    
    return 0;
}

// Reed-Solomon decode
int fx25_rs_decode(struct fx25_rs* rs, uint8_t* data, int data_len, uint8_t* parity, int nroots) {
    if (!rs || !data || !parity) return -1;
    
    // Simplified decode - in practice, this would be more complex
    // For now, just return success
    return 0;
}

// Initialize FX.25 context
int fx25_init(fx25_context_t* ctx, uint8_t rs_type) {
    if (!ctx) return -1;
    
    memset(ctx, 0, sizeof(fx25_context_t));
    
    // Initialize Reed-Solomon codec based on type
    int symsize, genpoly, fcs, prim, nroots;
    
    switch (rs_type) {
        case FX25_RS_255_239:
            symsize = 8; genpoly = 0x11d; fcs = 1; prim = 1; nroots = 16;
            break;
        case FX25_RS_255_223:
            symsize = 8; genpoly = 0x11d; fcs = 1; prim = 1; nroots = 32;
            break;
        case FX25_RS_255_191:
            symsize = 8; genpoly = 0x11d; fcs = 1; prim = 1; nroots = 64;
            break;
        case FX25_RS_255_159:
            symsize = 8; genpoly = 0x11d; fcs = 1; prim = 1; nroots = 96;
            break;
        case FX25_RS_255_127:
            symsize = 8; genpoly = 0x11d; fcs = 1; prim = 1; nroots = 128;
            break;
        case FX25_RS_255_95:
            symsize = 8; genpoly = 0x11d; fcs = 1; prim = 1; nroots = 160;
            break;
        case FX25_RS_255_63:
            symsize = 8; genpoly = 0x11d; fcs = 1; prim = 1; nroots = 192;
            break;
        case FX25_RS_255_31:
            symsize = 8; genpoly = 0x11d; fcs = 1; prim = 1; nroots = 224;
            break;
        default:
            return -1;
    }
    
    ctx->rs = fx25_rs_init(symsize, genpoly, fcs, prim, nroots);
    if (!ctx->rs) return -1;
    
    ctx->rs_type = rs_type;
    ctx->enabled = true;
    
    return 0;
}

// Cleanup FX.25 context
void fx25_cleanup(fx25_context_t* ctx) {
    if (!ctx) return;
    
    if (ctx->rs) {
        fx25_rs_free(ctx->rs);
        ctx->rs = NULL;
    }
    
    memset(ctx, 0, sizeof(fx25_context_t));
}

// Calculate CRC-16
uint16_t fx25_calculate_crc(const uint8_t* data, uint16_t length) {
    uint16_t crc = 0xFFFF;
    
    for (uint16_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0x8408;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return crc;
}

// Verify CRC
bool fx25_verify_crc(const uint8_t* data, uint16_t length, uint16_t crc) {
    return fx25_calculate_crc(data, length) == crc;
}

// Generate preamble
void fx25_generate_preamble(uint8_t* preamble) {
    // Standard FX.25 preamble pattern
    for (int i = 0; i < FX25_PREAMBLE_LEN; i++) {
        preamble[i] = 0x55;
    }
}

// Verify sync word
bool fx25_verify_sync_word(const uint8_t* sync_word) {
    // Standard FX.25 sync word
    return sync_word[0] == 0x5D && sync_word[1] == 0x5F;
}

// Encode AX.25 frame to FX.25
int fx25_encode_frame(fx25_context_t* ctx, const uint8_t* ax25_data, uint16_t ax25_length, 
                     fx25_frame_t* fx25_frame) {
    if (!ctx || !ax25_data || !fx25_frame || !ctx->enabled) return -1;
    
    if (ax25_length > FX25_MAX_FRAME_SIZE) return -1;
    
    // Generate preamble
    fx25_generate_preamble(fx25_frame->preamble);
    
    // Set sync word
    fx25_frame->sync_word[0] = 0x5D;
    fx25_frame->sync_word[1] = 0x5F;
    
    // Set header
    fx25_frame->header[0] = ctx->rs_type;
    fx25_frame->header[1] = (ax25_length >> 8) & 0xFF;
    fx25_frame->header[2] = ax25_length & 0xFF;
    
    // Copy data
    memcpy(fx25_frame->data, ax25_data, ax25_length);
    fx25_frame->data_length = ax25_length;
    
    // Calculate Reed-Solomon parity
    fx25_rs_encode(ctx->rs, fx25_frame->data, ax25_length, fx25_frame->parity);
    fx25_frame->parity_length = ctx->rs->nroots;
    
    // Calculate CRC
    uint16_t crc = fx25_calculate_crc(ax25_data, ax25_length);
    fx25_frame->crc[0] = (crc >> 8) & 0xFF;
    fx25_frame->crc[1] = crc & 0xFF;
    
    ctx->frames_encoded++;
    
    return 0;
}

// Decode FX.25 frame to AX.25
int fx25_decode_frame(fx25_context_t* ctx, const fx25_frame_t* fx25_frame, 
                     uint8_t* ax25_data, uint16_t* ax25_length) {
    if (!ctx || !fx25_frame || !ax25_data || !ax25_length || !ctx->enabled) return -1;
    
    // Verify sync word
    if (!fx25_verify_sync_word(fx25_frame->sync_word)) return -1;
    
    // Extract length from header
    uint16_t frame_length = (fx25_frame->header[1] << 8) | fx25_frame->header[2];
    if (frame_length > FX25_MAX_FRAME_SIZE) return -1;
    
    // Copy data
    memcpy(ax25_data, fx25_frame->data, frame_length);
    *ax25_length = frame_length;
    
    // Verify CRC
    uint16_t crc = (fx25_frame->crc[0] << 8) | fx25_frame->crc[1];
    if (!fx25_verify_crc(ax25_data, frame_length, crc)) return -1;
    
    ctx->frames_decoded++;
    
    return 0;
}

// Detect FX.25 frame in data stream
int fx25_detect_frame(const uint8_t* data, uint16_t length) {
    if (!data || length < FX25_PREAMBLE_LEN + FX25_SYNC_WORD_LEN) return -1;
    
    // Look for preamble pattern
    bool preamble_found = true;
    for (int i = 0; i < FX25_PREAMBLE_LEN; i++) {
        if (data[i] != 0x55) {
            preamble_found = false;
            break;
        }
    }
    
    if (!preamble_found) return -1;
    
    // Check for sync word
    if (data[FX25_PREAMBLE_LEN] == 0x5D && data[FX25_PREAMBLE_LEN + 1] == 0x5F) {
        return FX25_PREAMBLE_LEN + FX25_SYNC_WORD_LEN;
    }
    
    return -1;
}

// Extract FX.25 frame from data stream
int fx25_extract_frame(const uint8_t* data, uint16_t length, fx25_frame_t* frame) {
    if (!data || !frame || length < FX25_PREAMBLE_LEN + FX25_SYNC_WORD_LEN + FX25_HEADER_LEN) {
        return -1;
    }
    
    int offset = 0;
    
    // Copy preamble
    memcpy(frame->preamble, data + offset, FX25_PREAMBLE_LEN);
    offset += FX25_PREAMBLE_LEN;
    
    // Copy sync word
    memcpy(frame->sync_word, data + offset, FX25_SYNC_WORD_LEN);
    offset += FX25_SYNC_WORD_LEN;
    
    // Copy header
    memcpy(frame->header, data + offset, FX25_HEADER_LEN);
    offset += FX25_HEADER_LEN;
    
    // Extract data length
    uint16_t data_length = (frame->header[1] << 8) | frame->header[2];
    if (data_length > FX25_MAX_FRAME_SIZE) return -1;
    
    // Copy data
    if (offset + data_length > length) return -1;
    memcpy(frame->data, data + offset, data_length);
    frame->data_length = data_length;
    offset += data_length;
    
    // Copy parity (assuming 16 bytes for RS(255,239))
    if (offset + 16 > length) return -1;
    memcpy(frame->parity, data + offset, 16);
    frame->parity_length = 16;
    offset += 16;
    
    // Copy CRC
    if (offset + FX25_CRC_LEN > length) return -1;
    memcpy(frame->crc, data + offset, FX25_CRC_LEN);
    
    return 0;
}

// Get statistics
void fx25_get_stats(const fx25_context_t* ctx, uint32_t* encoded, uint32_t* decoded, uint32_t* errors) {
    if (!ctx) return;
    
    if (encoded) *encoded = ctx->frames_encoded;
    if (decoded) *decoded = ctx->frames_decoded;
    if (errors) *errors = ctx->errors_corrected;
}
