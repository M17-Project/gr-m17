//--------------------------------------------------------------------
// IL2P Improved Layer 2 Protocol
//
// Based on Dire Wolf implementation
// Modern replacement for AX.25 with better performance
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------

#include "il2p_protocol.h"
#include "fx25_protocol.h"  // For Reed-Solomon operations
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Scrambling polynomial for IL2P
#define IL2P_SCRAMBLE_POLY 0x8E

// Initialize IL2P context
int il2p_init(il2p_context_t* ctx) {
    if (!ctx) return -1;
    
    memset(ctx, 0, sizeof(il2p_context_t));
    ctx->enabled = true;
    ctx->debug_level = 0;
    
    return 0;
}

// Cleanup IL2P context
void il2p_cleanup(il2p_context_t* ctx) {
    if (!ctx) return;
    
    memset(ctx, 0, sizeof(il2p_context_t));
}

// Set debug level
void il2p_set_debug(il2p_context_t* ctx, uint8_t level) {
    if (!ctx) return;
    ctx->debug_level = level;
}

// Get debug level
uint8_t il2p_get_debug(const il2p_context_t* ctx) {
    if (!ctx) return 0;
    return ctx->debug_level;
}

// Scramble data
void il2p_scramble_data(uint8_t* data, uint16_t length) {
    if (!data || length == 0) return;
    
    uint8_t lfsr = 0xFF;
    
    for (uint16_t i = 0; i < length; i++) {
        uint8_t feedback = lfsr;
        lfsr >>= 1;
        if (feedback & 1) {
            lfsr ^= IL2P_SCRAMBLE_POLY;
        }
        data[i] ^= lfsr;
    }
}

// Descramble data
void il2p_descramble_data(uint8_t* data, uint16_t length) {
    if (!data || length == 0) return;
    
    uint8_t lfsr = 0xFF;
    
    for (uint16_t i = 0; i < length; i++) {
        uint8_t feedback = lfsr;
        lfsr >>= 1;
        if (feedback & 1) {
            lfsr ^= IL2P_SCRAMBLE_POLY;
        }
        data[i] ^= lfsr;
    }
}

// Calculate header checksum
uint8_t il2p_calculate_header_checksum(const il2p_header_t* header) {
    if (!header) return 0;
    
    uint8_t checksum = 0;
    checksum ^= header->version;
    checksum ^= header->type;
    checksum ^= header->sequence;
    
    for (int i = 0; i < 6; i++) {
        checksum ^= header->source[i];
    }
    
    for (int i = 0; i < 6; i++) {
        checksum ^= header->destination[i];
    }
    
    checksum ^= (header->payload_length >> 8) & 0xFF;
    checksum ^= header->payload_length & 0xFF;
    
    return checksum;
}

// Encode IL2P header
int il2p_encode_header(il2p_context_t* ctx, const il2p_header_t* header, uint8_t* encoded) {
    if (!ctx || !header || !encoded) return -1;
    
    int offset = 0;
    
    // Version
    encoded[offset++] = header->version;
    
    // Type
    encoded[offset++] = header->type;
    
    // Sequence
    encoded[offset++] = header->sequence;
    
    // Source address
    memcpy(encoded + offset, header->source, 6);
    offset += 6;
    
    // Destination address
    memcpy(encoded + offset, header->destination, 6);
    offset += 6;
    
    // Payload length
    encoded[offset++] = (header->payload_length >> 8) & 0xFF;
    encoded[offset++] = header->payload_length & 0xFF;
    
    // Checksum
    encoded[offset++] = il2p_calculate_header_checksum(header);
    
    return 0;
}

// Decode IL2P header
int il2p_decode_header(il2p_context_t* ctx, const uint8_t* encoded, il2p_header_t* header) {
    if (!ctx || !encoded || !header) return -1;
    
    int offset = 0;
    
    // Version
    header->version = encoded[offset++];
    
    // Type
    header->type = encoded[offset++];
    
    // Sequence
    header->sequence = encoded[offset++];
    
    // Source address
    memcpy(header->source, encoded + offset, 6);
    offset += 6;
    
    // Destination address
    memcpy(header->destination, encoded + offset, 6);
    offset += 6;
    
    // Payload length
    header->payload_length = (encoded[offset] << 8) | encoded[offset + 1];
    offset += 2;
    
    // Checksum
    header->checksum = encoded[offset];
    
    // Verify checksum
    uint8_t calculated_checksum = il2p_calculate_header_checksum(header);
    if (calculated_checksum != header->checksum) {
        return -1;  // Checksum error
    }
    
    return 0;
}

// Encode payload with Reed-Solomon
int il2p_encode_payload(il2p_context_t* ctx, const uint8_t* data, uint16_t length, 
                        uint8_t* encoded, uint16_t* encoded_length) {
    if (!ctx || !data || !encoded || !encoded_length) return -1;
    
    if (length > IL2P_MAX_PAYLOAD_SIZE) return -1;
    
    // Copy data
    memcpy(encoded, data, length);
    
    // Scramble data
    il2p_scramble_data(encoded, length);
    
    // Add Reed-Solomon parity (simplified - in practice would use proper RS encoding)
    // For now, just copy the data
    *encoded_length = length;
    
    return 0;
}

// Decode payload with Reed-Solomon
int il2p_decode_payload(il2p_context_t* ctx, const uint8_t* encoded, uint16_t encoded_length,
                        uint8_t* data, uint16_t* length) {
    if (!ctx || !encoded || !data || !length) return -1;
    
    // Copy data
    memcpy(data, encoded, encoded_length);
    
    // Descramble data
    il2p_descramble_data(data, encoded_length);
    
    *length = encoded_length;
    
    return 0;
}

// Encode IL2P frame
int il2p_encode_frame(il2p_context_t* ctx, const uint8_t* data, uint16_t length, 
                     il2p_frame_t* frame) {
    if (!ctx || !data || !frame || !ctx->enabled) return -1;
    
    if (length > IL2P_MAX_PAYLOAD_SIZE) return -1;
    
    // Set preamble
    frame->preamble = IL2P_PREAMBLE;
    
    // Set sync word
    frame->sync_word[0] = (IL2P_SYNC_WORD >> 16) & 0xFF;
    frame->sync_word[1] = (IL2P_SYNC_WORD >> 8) & 0xFF;
    frame->sync_word[2] = IL2P_SYNC_WORD & 0xFF;
    
    // Create header
    il2p_header_t header;
    header.version = 1;
    header.type = 0;  // Data frame
    header.sequence = 0;  // Would be incremented in practice
    memset(header.source, 0, 6);
    memset(header.destination, 0, 6);
    header.payload_length = length;
    header.checksum = 0;  // Will be calculated
    
    // Encode header
    il2p_encode_header(ctx, &header, frame->header);
    
    // Encode payload
    il2p_encode_payload(ctx, data, length, frame->payload, &frame->payload_length);
    
    // Add Reed-Solomon parity (simplified)
    frame->parity_length = 16;  // Standard parity length
    memset(frame->payload_parity, 0, frame->parity_length);
    
    ctx->frames_encoded++;
    
    return 0;
}

// Decode IL2P frame
int il2p_decode_frame(il2p_context_t* ctx, const il2p_frame_t* frame, 
                     uint8_t* data, uint16_t* length) {
    if (!ctx || !frame || !data || !length || !ctx->enabled) return -1;
    
    // Verify preamble
    if (frame->preamble != IL2P_PREAMBLE) return -1;
    
    // Verify sync word
    uint32_t sync_word = (frame->sync_word[0] << 16) | 
                        (frame->sync_word[1] << 8) | 
                        frame->sync_word[2];
    if (sync_word != IL2P_SYNC_WORD) return -1;
    
    // Decode header
    il2p_header_t header;
    if (il2p_decode_header(ctx, frame->header, &header) != 0) return -1;
    
    // Decode payload
    if (il2p_decode_payload(ctx, frame->payload, frame->payload_length, 
                            data, length) != 0) return -1;
    
    ctx->frames_decoded++;
    
    return 0;
}

// Detect IL2P frame in data stream
int il2p_detect_frame(const uint8_t* data, uint16_t length) {
    if (!data || length < IL2P_SYNC_WORD_SIZE) return -1;
    
    // Look for sync word
    for (uint16_t i = 0; i <= length - IL2P_SYNC_WORD_SIZE; i++) {
        uint32_t sync_word = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
        if (sync_word == IL2P_SYNC_WORD) {
            return i + IL2P_SYNC_WORD_SIZE;
        }
    }
    
    return -1;
}

// Extract IL2P frame from data stream
int il2p_extract_frame(const uint8_t* data, uint16_t length, il2p_frame_t* frame) {
    if (!data || !frame || length < IL2P_SYNC_WORD_SIZE + IL2P_HEADER_SIZE + IL2P_HEADER_PARITY) {
        return -1;
    }
    
    int offset = 0;
    
    // Find sync word
    int sync_pos = il2p_detect_frame(data, length);
    if (sync_pos < 0) return -1;
    
    offset = sync_pos - IL2P_SYNC_WORD_SIZE;
    
    // Copy sync word
    memcpy(frame->sync_word, data + offset, IL2P_SYNC_WORD_SIZE);
    offset += IL2P_SYNC_WORD_SIZE;
    
    // Copy header
    memcpy(frame->header, data + offset, IL2P_HEADER_SIZE);
    offset += IL2P_HEADER_SIZE;
    
    // Copy header parity
    memcpy(frame->header_parity, data + offset, IL2P_HEADER_PARITY);
    offset += IL2P_HEADER_PARITY;
    
    // Extract payload length from header
    uint16_t payload_length = (frame->header[9] << 8) | frame->header[10];
    if (payload_length > IL2P_MAX_PAYLOAD_SIZE) return -1;
    
    // Copy payload
    if (offset + payload_length > length) return -1;
    memcpy(frame->payload, data + offset, payload_length);
    frame->payload_length = payload_length;
    offset += payload_length;
    
    // Copy payload parity
    if (offset + 16 > length) return -1;
    memcpy(frame->payload_parity, data + offset, 16);
    frame->parity_length = 16;
    
    return 0;
}

// Get statistics
void il2p_get_stats(const il2p_context_t* ctx, uint32_t* encoded, uint32_t* decoded, uint32_t* errors) {
    if (!ctx) return;
    
    if (encoded) *encoded = ctx->frames_encoded;
    if (decoded) *decoded = ctx->frames_decoded;
    if (errors) *errors = ctx->errors_corrected;
}
