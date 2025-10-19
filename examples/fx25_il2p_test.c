//--------------------------------------------------------------------
// FX.25 and IL2P Test Example
//
// Test implementation of FX.25 and IL2P protocols
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// Include our protocol headers
#include "libm17/tnc/fx25_protocol.h"
#include "libm17/tnc/il2p_protocol.h"
#include "libm17/bridge/m17_ax25_bridge.h"

int main() {
    printf("FX.25 and IL2P Protocol Test\n");
    printf("============================\n\n");
    
    // Test FX.25
    printf("Testing FX.25 Protocol:\n");
    printf("----------------------\n");
    
    fx25_context_t fx25_ctx;
    if (fx25_init(&fx25_ctx, FX25_RS_255_239) != 0) {
        printf("ERROR: Failed to initialize FX.25 context\n");
        return -1;
    }
    
    // Test data
    uint8_t test_data[] = "Hello, FX.25!";
    uint16_t test_length = strlen((char*)test_data);
    
    // Encode FX.25 frame
    fx25_frame_t fx25_frame;
    if (fx25_encode_frame(&fx25_ctx, test_data, test_length, &fx25_frame) != 0) {
        printf("ERROR: Failed to encode FX.25 frame\n");
        fx25_cleanup(&fx25_ctx);
        return -1;
    }
    
    printf("✓ FX.25 frame encoded successfully\n");
    printf("  Data length: %d bytes\n", fx25_frame.data_length);
    printf("  Parity length: %d bytes\n", fx25_frame.parity_length);
    
    // Decode FX.25 frame
    uint8_t decoded_data[FX25_MAX_FRAME_SIZE];
    uint16_t decoded_length;
    if (fx25_decode_frame(&fx25_ctx, &fx25_frame, decoded_data, &decoded_length) != 0) {
        printf("ERROR: Failed to decode FX.25 frame\n");
        fx25_cleanup(&fx25_ctx);
        return -1;
    }
    
    printf("✓ FX.25 frame decoded successfully\n");
    printf("  Decoded data: %.*s\n", decoded_length, decoded_data);
    
    // Verify data integrity
    if (decoded_length == test_length && memcmp(decoded_data, test_data, test_length) == 0) {
        printf("✓ Data integrity verified\n");
    } else {
        printf("ERROR: Data integrity check failed\n");
        fx25_cleanup(&fx25_ctx);
        return -1;
    }
    
    fx25_cleanup(&fx25_ctx);
    printf("\n");
    
    // Test IL2P
    printf("Testing IL2P Protocol:\n");
    printf("----------------------\n");
    
    il2p_context_t il2p_ctx;
    if (il2p_init(&il2p_ctx) != 0) {
        printf("ERROR: Failed to initialize IL2P context\n");
        return -1;
    }
    
    // Set debug level
    il2p_set_debug(&il2p_ctx, 1);
    
    // Test data
    uint8_t il2p_test_data[] = "Hello, IL2P!";
    uint16_t il2p_test_length = strlen((char*)il2p_test_data);
    
    // Encode IL2P frame
    il2p_frame_t il2p_frame;
    if (il2p_encode_frame(&il2p_ctx, il2p_test_data, il2p_test_length, &il2p_frame) != 0) {
        printf("ERROR: Failed to encode IL2P frame\n");
        il2p_cleanup(&il2p_ctx);
        return -1;
    }
    
    printf("✓ IL2P frame encoded successfully\n");
    printf("  Payload length: %d bytes\n", il2p_frame.payload_length);
    printf("  Parity length: %d bytes\n", il2p_frame.parity_length);
    
    // Decode IL2P frame
    uint8_t il2p_decoded_data[IL2P_MAX_PAYLOAD_SIZE];
    uint16_t il2p_decoded_length;
    if (il2p_decode_frame(&il2p_ctx, &il2p_frame, il2p_decoded_data, &il2p_decoded_length) != 0) {
        printf("ERROR: Failed to decode IL2P frame\n");
        il2p_cleanup(&il2p_ctx);
        return -1;
    }
    
    printf("✓ IL2P frame decoded successfully\n");
    printf("  Decoded data: %.*s\n", il2p_decoded_length, il2p_decoded_data);
    
    // Verify data integrity
    if (il2p_decoded_length == il2p_test_length && memcmp(il2p_decoded_data, il2p_test_data, il2p_test_length) == 0) {
        printf("✓ Data integrity verified\n");
    } else {
        printf("ERROR: Data integrity check failed\n");
        il2p_cleanup(&il2p_ctx);
        return -1;
    }
    
    il2p_cleanup(&il2p_ctx);
    printf("\n");
    
    // Test Bridge Integration
    printf("Testing Bridge Integration:\n");
    printf("--------------------------\n");
    
    m17_ax25_bridge_t bridge;
    if (m17_ax25_bridge_init(&bridge) != 0) {
        printf("ERROR: Failed to initialize bridge\n");
        return -1;
    }
    
    // Enable FX.25 and IL2P
    bridge.state.config.fx25_enabled = true;
    bridge.state.config.fx25_rs_type = FX25_RS_255_239;
    bridge.state.config.il2p_enabled = true;
    bridge.state.config.il2p_debug = 1;
    
    // Re-initialize with new settings
    if (fx25_init(&bridge.state.fx25_ctx, bridge.state.config.fx25_rs_type) != 0) {
        printf("ERROR: Failed to initialize FX.25 in bridge\n");
        m17_ax25_bridge_cleanup(&bridge);
        return -1;
    }
    
    if (il2p_init(&bridge.state.il2p_ctx) != 0) {
        printf("ERROR: Failed to initialize IL2P in bridge\n");
        m17_ax25_bridge_cleanup(&bridge);
        return -1;
    }
    
    printf("✓ Bridge initialized with FX.25 and IL2P support\n");
    
    // Test protocol detection
    uint8_t test_frame[] = {0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x5D, 0x5F};
    if (m17_ax25_bridge_detect_protocol(&bridge, test_frame, sizeof(test_frame)) == 0) {
        printf("✓ Protocol detection working\n");
        printf("  Detected protocol: %d\n", bridge.state.current_protocol);
    } else {
        printf("ERROR: Protocol detection failed\n");
    }
    
    // Print bridge status
    m17_ax25_bridge_print_status(&bridge);
    
    m17_ax25_bridge_cleanup(&bridge);
    
    printf("\nAll tests completed successfully!\n");
    return 0;
}
