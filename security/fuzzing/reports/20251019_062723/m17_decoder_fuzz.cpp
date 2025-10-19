#include <iostream>
#include <cstring>
#include <cstdint>
#include <cstdlib>

// CRITICAL: Include actual M17 decoder headers when integrating
// #include "m17_decoder_impl.h"

// Mock M17 frame structure for testing
struct M17Frame {
    uint8_t syncword[8];
    uint8_t lsf[240];      // Link Setup Frame
    uint8_t payload[128];
    uint8_t crc[16];
};

// Simulate M17 decoder functionality
int decode_m17_frame(const uint8_t* data, size_t size) {
    if (size < sizeof(M17Frame)) {
        return -1;  // Invalid size
    }
    
    M17Frame frame;
    memcpy(&frame, data, sizeof(M17Frame));
    
    // Test syncword detection
    uint8_t expected_sync[8] = {0x55, 0xF7, 0x7F, 0xD7, 0x7F, 0x55, 0xF7, 0x7F};
    bool sync_match = true;
    for (int i = 0; i < 8; i++) {
        if (frame.syncword[i] != expected_sync[i]) {
            sync_match = false;
            break;
        }
    }
    
    // Test LSF decoding (simulate)
    uint16_t type = (frame.lsf[0] << 8) | frame.lsf[1];
    uint8_t encr_type = (type >> 9) & 0x3;
    
    // Test payload decoding
    uint8_t decoded_payload[128];
    memcpy(decoded_payload, frame.payload, 128);
    
    // Test CRC validation (simple XOR for testing)
    uint16_t crc = 0;
    for (int i = 0; i < 128; i++) {
        crc ^= frame.payload[i];
    }
    
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    // Input validation
    if (size < 1 || size > 10000) {
        return 0;
    }
    
    // Test M17 frame decoding
    decode_m17_frame(data, size);
    
    // Test buffer operations
    if (size >= 16) {
        uint8_t buffer[1024];
        size_t copy_size = (size < sizeof(buffer)) ? size : sizeof(buffer);
        memcpy(buffer, data, copy_size);
    }
    
    // Test string operations
    if (size > 0 && size < 256) {
        char callsign[10];
        size_t copy_len = (size < 9) ? size : 9;
        memcpy(callsign, data, copy_len);
        callsign[copy_len] = '\0';
        
        // Validate callsign characters
        for (size_t i = 0; i < copy_len; i++) {
            if (!isalnum(callsign[i]) && callsign[i] != '-') {
                // Invalid character
                break;
            }
        }
    }
    
    return 0;
}

// Add main function for standalone testing
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <input_file>" << std::endl;
        return 1;
    }
    
    FILE* f = fopen(argv[1], "rb");
    if (!f) {
        std::cout << "Error opening file: " << argv[1] << std::endl;
        return 1;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size > 10000) {
        std::cout << "File too large: " << size << " bytes" << std::endl;
        fclose(f);
        return 1;
    }
    
    uint8_t* data = new uint8_t[size];
    fread(data, 1, size, f);
    fclose(f);
    
    int result = LLVMFuzzerTestOneInput(data, size);
    delete[] data;
    
    return result;
}
