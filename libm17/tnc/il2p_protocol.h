//--------------------------------------------------------------------
// IL2P Improved Layer 2 Protocol
//
// Based on Dire Wolf implementation
// Modern replacement for AX.25 with better performance
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

// IL2P Constants
#define IL2P_PREAMBLE            0x55
#define IL2P_SYNC_WORD           0xF15E48
#define IL2P_SYNC_WORD_SIZE      3
#define IL2P_HEADER_SIZE         13      // Does not include 2 parity
#define IL2P_HEADER_PARITY       2
#define IL2P_MAX_PAYLOAD_SIZE    1023
#define IL2P_MAX_PAYLOAD_BLOCKS  5
#define IL2P_MAX_PARITY_SYMBOLS  16      // For payload only
#define IL2P_MAX_ENCODED_PAYLOAD_SIZE (IL2P_MAX_PAYLOAD_SIZE + IL2P_MAX_PAYLOAD_BLOCKS * IL2P_MAX_PARITY_SYMBOLS)
#define IL2P_MAX_PACKET_SIZE     (IL2P_SYNC_WORD_SIZE + IL2P_HEADER_SIZE + IL2P_HEADER_PARITY + IL2P_MAX_ENCODED_PAYLOAD_SIZE)

// IL2P Header Structure
typedef struct {
    uint8_t preamble;
    uint8_t sync_word[IL2P_SYNC_WORD_SIZE];
    uint8_t header[IL2P_HEADER_SIZE];
    uint8_t header_parity[IL2P_HEADER_PARITY];
    uint8_t payload[IL2P_MAX_ENCODED_PAYLOAD_SIZE];
    uint16_t payload_length;
    uint8_t payload_parity[IL2P_MAX_PARITY_SYMBOLS];
    uint8_t parity_length;
} il2p_frame_t;

// IL2P Context
typedef struct {
    bool enabled;                    // IL2P enabled
    uint32_t frames_encoded;        // Frames encoded
    uint32_t frames_decoded;        // Frames decoded
    uint32_t errors_corrected;      // Errors corrected
    uint8_t debug_level;            // Debug level
} il2p_context_t;

// IL2P Header Fields
typedef struct {
    uint8_t version;                 // Protocol version
    uint8_t type;                   // Frame type
    uint8_t sequence;                // Sequence number
    uint8_t source[6];               // Source address
    uint8_t destination[6];         // Destination address
    uint16_t payload_length;         // Payload length
    uint8_t checksum;                // Header checksum
} il2p_header_t;

// Function Declarations

// Initialization
int il2p_init(il2p_context_t* ctx);
void il2p_cleanup(il2p_context_t* ctx);
void il2p_set_debug(il2p_context_t* ctx, uint8_t level);
uint8_t il2p_get_debug(const il2p_context_t* ctx);

// Frame Operations
int il2p_encode_frame(il2p_context_t* ctx, const uint8_t* data, uint16_t length, 
                     il2p_frame_t* frame);
int il2p_decode_frame(il2p_context_t* ctx, const il2p_frame_t* frame, 
                     uint8_t* data, uint16_t* length);
int il2p_detect_frame(const uint8_t* data, uint16_t length);
int il2p_extract_frame(const uint8_t* data, uint16_t length, il2p_frame_t* frame);

// Header Operations
int il2p_encode_header(il2p_context_t* ctx, const il2p_header_t* header, uint8_t* encoded);
int il2p_decode_header(il2p_context_t* ctx, const uint8_t* encoded, il2p_header_t* header);
uint8_t il2p_calculate_header_checksum(const il2p_header_t* header);

// Payload Operations
int il2p_encode_payload(il2p_context_t* ctx, const uint8_t* data, uint16_t length, 
                        uint8_t* encoded, uint16_t* encoded_length);
int il2p_decode_payload(il2p_context_t* ctx, const uint8_t* encoded, uint16_t encoded_length,
                        uint8_t* data, uint16_t* length);

// Scrambling
void il2p_scramble_data(uint8_t* data, uint16_t length);
void il2p_descramble_data(uint8_t* data, uint16_t length);

// Reed-Solomon Operations (shared with FX.25)
struct il2p_rs* il2p_find_rs(int nparity);
int il2p_encode_rs(uint8_t* tx_data, int data_size, int num_parity, uint8_t* parity_out);
int il2p_decode_rs(uint8_t* rec_block, int data_size, int num_parity, uint8_t* out);

// Statistics
void il2p_get_stats(const il2p_context_t* ctx, uint32_t* encoded, uint32_t* decoded, uint32_t* errors);

#ifdef __cplusplus
}
#endif
