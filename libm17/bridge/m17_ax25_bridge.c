//--------------------------------------------------------------------
// M17 ↔ AX.25 Protocol Bridge
//
// Protocol bridge for dual-mode operation
// Supports both M17 and AX.25 packet radio
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------
#include "m17_ax25_bridge.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

// Initialize M17-AX.25 bridge
int m17_ax25_bridge_init(m17_ax25_bridge_t* bridge) {
    if (!bridge) {
        return -1;
    }
    
    // Initialize KISS TNC
    if (kiss_init(&bridge->kiss_tnc) != 0) {
        return -1;
    }
    
    // Initialize AX.25 TNC
    if (ax25_init(&bridge->ax25_tnc) != 0) {
        kiss_cleanup(&bridge->kiss_tnc);
        return -1;
    }
    
    // Initialize bridge state
    bridge->state.config.m17_enabled = true;
    bridge->state.config.ax25_enabled = true;
    bridge->state.config.auto_detect = true;
    bridge->state.config.m17_frequency = 144800000;  // 144.8 MHz
    bridge->state.config.ax25_frequency = 144800000; // Same frequency
    bridge->state.config.m17_can = 0;
    strncpy(bridge->state.config.ax25_callsign, "N0CALL", sizeof(bridge->state.config.ax25_callsign) - 1);
    bridge->state.config.ax25_callsign[sizeof(bridge->state.config.ax25_callsign) - 1] = '\0';
    bridge->state.config.ax25_ssid = 0;
    
    bridge->state.current_protocol = PROTOCOL_UNKNOWN;
    bridge->state.m17_active = false;
    bridge->state.ax25_active = false;
    bridge->state.fx25_active = false;
    bridge->state.il2p_active = false;
    bridge->state.last_activity = 0;
    bridge->state.protocol_timeout = 5000; // 5 seconds
    
    // Initialize FX.25 context
    if (bridge->state.config.fx25_enabled) {
        if (fx25_init(&bridge->state.fx25_ctx, bridge->state.config.fx25_rs_type) != 0) {
            return -1;
        }
    }
    
    // Initialize IL2P context
    if (bridge->state.config.il2p_enabled) {
        if (il2p_init(&bridge->state.il2p_ctx) != 0) {
            return -1;
        }
        il2p_set_debug(&bridge->state.il2p_ctx, bridge->state.config.il2p_debug);
    }
    
    // Initialize mappings
    bridge->num_mappings = 0;
    memset(bridge->mappings, 0, sizeof(bridge->mappings));
    
    return 0;
}

// Cleanup M17-AX.25 bridge
int m17_ax25_bridge_cleanup(m17_ax25_bridge_t* bridge) {
    if (!bridge) {
        return -1;
    }
    
    // Cleanup TNCs
    kiss_cleanup(&bridge->kiss_tnc);
    ax25_cleanup(&bridge->ax25_tnc);
    
    // Reset state
    bridge->state.current_protocol = PROTOCOL_UNKNOWN;
    bridge->state.m17_active = false;
    bridge->state.ax25_active = false;
    bridge->num_mappings = 0;
    
    return 0;
}

// Set bridge configuration
int m17_ax25_bridge_set_config(m17_ax25_bridge_t* bridge, const bridge_config_t* config) {
    if (!bridge || !config) {
        return -1;
    }
    
    bridge->state.config = *config;
    return 0;
}

// Get bridge configuration
int m17_ax25_bridge_get_config(const m17_ax25_bridge_t* bridge, bridge_config_t* config) {
    if (!bridge || !config) {
        return -1;
    }
    
    *config = bridge->state.config;
    return 0;
}

// Detect protocol from data
int m17_ax25_bridge_detect_protocol(m17_ax25_bridge_t* bridge, const uint8_t* data, uint16_t length) {
    if (!bridge || !data || length == 0) {
        return -1;
    }
    
    // Check for FX.25 frame
    if (bridge->state.config.fx25_enabled) {
        int fx25_pos = fx25_detect_frame(data, length);
        if (fx25_pos >= 0) {
            bridge->state.current_protocol = PROTOCOL_FX25;
            bridge->state.fx25_active = true;
            bridge->state.m17_active = false;
            bridge->state.ax25_active = false;
            bridge->state.il2p_active = false;
            return 0;
        }
    }
    
    // Check for IL2P frame
    if (bridge->state.config.il2p_enabled) {
        int il2p_pos = il2p_detect_frame(data, length);
        if (il2p_pos >= 0) {
            bridge->state.current_protocol = PROTOCOL_IL2P;
            bridge->state.il2p_active = true;
            bridge->state.m17_active = false;
            bridge->state.ax25_active = false;
            bridge->state.fx25_active = false;
            return 0;
        }
    }
    
    // Check for M17 frame markers
    if (length >= 2 && data[0] == 0x5D && data[1] == 0x5F) {
        bridge->state.current_protocol = PROTOCOL_M17;
        bridge->state.m17_active = true;
        bridge->state.ax25_active = false;
        bridge->state.fx25_active = false;
        bridge->state.il2p_active = false;
        return 0;
    }
    
    // Check for AX.25 frame markers (flag 0x7E)
    if (data[0] == 0x7E) {
        bridge->state.current_protocol = PROTOCOL_AX25;
        bridge->state.ax25_active = true;
        bridge->state.m17_active = false;
        bridge->state.fx25_active = false;
        bridge->state.il2p_active = false;
        return 0;
    }
    
    // Check for APRS data (UI frame with specific PID)
    if (length >= 3 && data[0] == 0x7E && data[1] == 0x7E) {
        // Look for APRS PID (0xF0) in the frame
        for (uint16_t i = 2; i < length - 2; i++) {
            if (data[i] == 0xF0) {
                bridge->state.current_protocol = PROTOCOL_APRS;
                bridge->state.ax25_active = true;
                bridge->state.m17_active = false;
                bridge->state.fx25_active = false;
                bridge->state.il2p_active = false;
                return 0;
            }
        }
    }
    
    // Unknown protocol
    bridge->state.current_protocol = PROTOCOL_UNKNOWN;
    return -1;
}

// Get current protocol
protocol_type_t m17_ax25_bridge_get_current_protocol(const m17_ax25_bridge_t* bridge) {
    if (!bridge) {
        return PROTOCOL_UNKNOWN;
    }
    
    return bridge->state.current_protocol;
}

// Set protocol
int m17_ax25_bridge_set_protocol(m17_ax25_bridge_t* bridge, protocol_type_t protocol) {
    if (!bridge) {
        return -1;
    }
    
    bridge->state.current_protocol = protocol;
    
    switch (protocol) {
        case PROTOCOL_M17:
            bridge->state.m17_active = true;
            bridge->state.ax25_active = false;
            break;
        case PROTOCOL_AX25:
        case PROTOCOL_APRS:
            bridge->state.ax25_active = true;
            bridge->state.m17_active = false;
            break;
        default:
            bridge->state.m17_active = false;
            bridge->state.ax25_active = false;
            break;
    }
    
    return 0;
}

// Convert M17 to AX.25
int m17_ax25_bridge_convert_m17_to_ax25(m17_ax25_bridge_t* bridge, const uint8_t* m17_data, uint16_t m17_length,
                                       uint8_t* ax25_data, uint16_t* ax25_length) {
    if (!bridge || !m17_data || m17_length == 0 || !ax25_data || !ax25_length) {
        return -1;
    }
    
    // Implement M17 to AX.25 conversion
    // This involves:
    // 1. Parse M17 frame
    // 2. Extract callsigns and data
    // 3. Create AX.25 frame
    // 4. Encode AX.25 frame
    
    // For now, implement basic conversion
    // In a real implementation, you would:
    // 1. Parse M17 frame structure
    // 2. Extract source and destination callsigns
    // 3. Map M17 callsigns to AX.25 callsigns
    // 4. Create AX.25 UI frame
    // 5. Encode the frame
    
    // Basic validation
    if (m17_length < 2) {
        return -1; // Invalid M17 frame
    }
    
    // Check for M17 frame markers
    if (m17_data[0] != 0x5D || m17_data[1] != 0x5F) {
        return -1; // Not a valid M17 frame
    }
    
    // Extract M17 frame information
    if (m17_length < 4) {
        return -1; // Invalid M17 frame
    }
    
    uint8_t frame_type = m17_data[2];
    
    // Create AX.25 frame based on M17 frame type
    if (frame_type == 0x00) { // LSF -> APRS beacon
        return m17_ax25_bridge_convert_m17_lsf_to_aprs(bridge, m17_data, m17_length, ax25_data, ax25_length);
    } else if (frame_type == 0x02) { // Packet -> AX.25 UI frame
        return m17_ax25_bridge_convert_m17_packet_to_ax25(bridge, m17_data, m17_length, ax25_data, ax25_length);
    } else {
        return -1; // Unsupported M17 frame type for conversion
    }
}

// Convert M17 LSF to APRS beacon
int m17_ax25_bridge_convert_m17_lsf_to_aprs(m17_ax25_bridge_t* bridge, const uint8_t* m17_data, uint16_t m17_length,
                                           uint8_t* ax25_data, uint16_t* ax25_length) {
    if (!bridge || !m17_data || m17_length < 30 || !ax25_data || !ax25_length) {
        return -1;
    }
    
    if (*ax25_length < 50) {
        return -1; // Buffer too small
    }
    
    // Extract M17 callsigns
    char src_callsign[10] = {0};
    char dst_callsign[10] = {0};
    
    for (int i = 0; i < 9; i++) {
        src_callsign[i] = m17_data[3 + i];
        dst_callsign[i] = m17_data[13 + i];
    }
    
    // Find AX.25 callsign mapping
    char ax25_src[7] = {0};
    char ax25_dst[7] = {0};
    uint8_t src_ssid = 0;
    uint8_t dst_ssid = 0;
    
    // Use mapping if available, otherwise use truncated callsigns
    if (m17_ax25_bridge_find_mapping(bridge, src_callsign, ax25_src, &src_ssid) != 0) {
        // Truncate M17 callsign to AX.25 format
        strncpy(ax25_src, src_callsign, 6);
    }
    
    if (m17_ax25_bridge_find_mapping(bridge, dst_callsign, ax25_dst, &dst_ssid) != 0) {
        // Truncate M17 callsign to AX.25 format
        strncpy(ax25_dst, dst_callsign, 6);
    }
    
    // Create AX.25 APRS frame
    int pos = 0;
    
    // Opening flag
    ax25_data[pos++] = 0x7E;
    
    // Destination address
    for (int i = 0; i < 6; i++) {
        ax25_data[pos++] = (ax25_dst[i] << 1);
    }
    ax25_data[pos++] = (dst_ssid << 1);
    
    // Source address
    for (int i = 0; i < 6; i++) {
        ax25_data[pos++] = (ax25_src[i] << 1);
    }
    ax25_data[pos++] = (src_ssid << 1) | 0x60; // Command bit
    
    // Control field (UI frame)
    ax25_data[pos++] = 0x03;
    
    // PID (no layer 3)
    ax25_data[pos++] = 0xF0;
    
    // APRS data (position beacon)
    const char* aprs_data = "!0000.00N/00000.00W-M17 Bridge";
    for (int i = 0; aprs_data[i] && pos < *ax25_length - 3; i++) {
        ax25_data[pos++] = aprs_data[i];
    }
    
    // Calculate proper FCS (Frame Check Sequence)
    uint16_t fcs = 0xFFFF;
    for (int i = 0; i < pos; i++) {
        fcs ^= ax25_data[i];
        for (int j = 0; j < 8; j++) {
            if (fcs & 0x0001) {
                fcs = (fcs >> 1) ^ 0x8408;
            } else {
                fcs = fcs >> 1;
            }
        }
    }
    fcs = ~fcs;
    ax25_data[pos++] = fcs & 0xFF;
    ax25_data[pos++] = (fcs >> 8) & 0xFF;
    
    // Closing flag
    ax25_data[pos++] = 0x7E;
    
    *ax25_length = pos;
    return 0;
}

// Convert M17 packet to AX.25 UI frame
int m17_ax25_bridge_convert_m17_packet_to_ax25(m17_ax25_bridge_t* bridge, const uint8_t* m17_data, uint16_t m17_length,
                                              uint8_t* ax25_data, uint16_t* ax25_length) {
    if (!bridge || !m17_data || m17_length < 16 || !ax25_data || !ax25_length) {
        return -1;
    }
    
    if (*ax25_length < 50) {
        return -1; // Buffer too small
    }
    
    // Extract M17 packet data
    uint16_t packet_start = 3; // Skip frame markers and type
    uint16_t packet_length = m17_length - packet_start;
    
    // Create AX.25 UI frame
    int pos = 0;
    
    // Opening flag
    ax25_data[pos++] = 0x7E;
    
    // Destination address (broadcast)
    for (int i = 0; i < 6; i++) {
        ax25_data[pos++] = ('Q' << 1);
    }
    ax25_data[pos++] = 0x00;
    
    // Source address (use bridge callsign)
    for (int i = 0; i < 6; i++) {
        ax25_data[pos++] = (bridge->state.config.ax25_callsign[i] << 1);
    }
    ax25_data[pos++] = (bridge->state.config.ax25_ssid << 1) | 0x60;
    
    // Control field (UI frame)
    ax25_data[pos++] = 0x03;
    
    // PID (no layer 3)
    ax25_data[pos++] = 0xF0;
    
    // Copy M17 packet data as information field
    uint16_t info_len = (packet_length > 20) ? 20 : packet_length;
    for (uint16_t i = 0; i < info_len && pos < *ax25_length - 3; i++) {
        ax25_data[pos++] = m17_data[packet_start + i];
    }
    
    // FCS (simplified)
    ax25_data[pos++] = 0x00;
    ax25_data[pos++] = 0x00;
    
    // Closing flag
    ax25_data[pos++] = 0x7E;
    
    *ax25_length = pos;
    return 0;
}

// Convert AX.25 to M17
int m17_ax25_bridge_convert_ax25_to_m17(m17_ax25_bridge_t* bridge, const uint8_t* ax25_data, uint16_t ax25_length,
                                        uint8_t* m17_data, uint16_t* m17_length) {
    if (!bridge || !ax25_data || ax25_length == 0 || !m17_data || !m17_length) {
        return -1;
    }
    
    // Implement AX.25 to M17 conversion
    // This involves:
    // 1. Parse AX.25 frame
    // 2. Extract callsigns and data
    // 3. Create M17 frame
    // 4. Encode M17 frame
    
    // Basic validation
    if (ax25_length < 3) {
        return -1; // Invalid AX.25 frame
    }
    
    // Check for AX.25 frame markers
    if (ax25_data[0] != 0x7E) {
        return -1; // Not a valid AX.25 frame
    }
    
    // Find closing flag
    uint16_t frame_end = 0;
    for (uint16_t i = 1; i < ax25_length; i++) {
        if (ax25_data[i] == 0x7E) {
            frame_end = i;
            break;
        }
    }
    
    if (frame_end == 0) {
        return -1; // No closing flag found
    }
    
    // Create proper M17 frame from AX.25 data
    // Implement M17 frame encoding
    if (*m17_length < 16) {
        return -1; // Buffer too small
    }
    
    // Add M17 frame markers
    m17_data[0] = 0x5D;
    m17_data[1] = 0x5F;
    
    // Parse AX.25 frame properly
    ax25_frame_t ax25_frame;
    if (ax25_parse_frame(ax25_data, frame_end + 1, &ax25_frame) != 0) {
        return -1; // Invalid AX.25 frame
    }
    
    // Extract information field from AX.25 frame
    uint16_t info_start = 16; // Skip addresses and control/PID
    uint16_t info_len = (frame_end + 1) - info_start - 2; // Subtract FCS
    
    if (info_len > 0 && info_len <= 10) {
        for (uint16_t i = 0; i < info_len; i++) {
            m17_data[2 + i] = ax25_data[info_start + i];
        }
    }
    
    *m17_length = 2 + info_len;
    
    return 0;
}

// Add callsign mapping
int m17_ax25_bridge_add_mapping(m17_ax25_bridge_t* bridge, const char* m17_callsign, 
                                const char* ax25_callsign, uint8_t ax25_ssid) {
    if (!bridge || !m17_callsign || !ax25_callsign) {
        return -1;
    }
    
    if (bridge->num_mappings >= 16) {
        return -1; // No more mappings available
    }
    
    // Validate callsigns
    if (m17_ax25_bridge_validate_callsign(m17_callsign) != 0 ||
        m17_ax25_bridge_validate_callsign(ax25_callsign) != 0) {
        return -1;
    }
    
    // Add mapping
    strncpy(bridge->mappings[bridge->num_mappings].m17_callsign, m17_callsign, 9);
    bridge->mappings[bridge->num_mappings].m17_callsign[9] = '\0';
    
    strncpy(bridge->mappings[bridge->num_mappings].ax25_callsign, ax25_callsign, 6);
    bridge->mappings[bridge->num_mappings].ax25_callsign[6] = '\0';
    
    bridge->mappings[bridge->num_mappings].ax25_ssid = ax25_ssid;
    bridge->mappings[bridge->num_mappings].active = true;
    
    bridge->num_mappings++;
    return 0;
}

// Remove callsign mapping
int m17_ax25_bridge_remove_mapping(m17_ax25_bridge_t* bridge, const char* m17_callsign) {
    if (!bridge || !m17_callsign) {
        return -1;
    }
    
    for (int i = 0; i < bridge->num_mappings; i++) {
        if (m17_ax25_bridge_compare_callsigns(bridge->mappings[i].m17_callsign, m17_callsign) == 0) {
            // Remove mapping by shifting remaining mappings
            for (int j = i; j < bridge->num_mappings - 1; j++) {
                bridge->mappings[j] = bridge->mappings[j + 1];
            }
            bridge->num_mappings--;
            return 0;
        }
    }
    
    return -1; // Mapping not found
}

// Find callsign mapping
int m17_ax25_bridge_find_mapping(const m17_ax25_bridge_t* bridge, const char* m17_callsign,
                                 char* ax25_callsign, uint8_t* ax25_ssid) {
    if (!bridge || !m17_callsign || !ax25_callsign || !ax25_ssid) {
        return -1;
    }
    
    for (int i = 0; i < bridge->num_mappings; i++) {
        if (bridge->mappings[i].active &&
            m17_ax25_bridge_compare_callsigns(bridge->mappings[i].m17_callsign, m17_callsign) == 0) {
            strncpy(ax25_callsign, bridge->mappings[i].ax25_callsign, sizeof(ax25_callsign) - 1);
            ax25_callsign[sizeof(ax25_callsign) - 1] = '\0'; // Ensure null termination
            *ax25_ssid = bridge->mappings[i].ax25_ssid;
            return 0;
        }
    }
    
    return -1; // Mapping not found
}

// Process RX data
int m17_ax25_bridge_process_rx_data(m17_ax25_bridge_t* bridge, const uint8_t* data, uint16_t length) {
    if (!bridge || !data || length == 0) {
        return -1;
    }
    
    // Detect protocol if auto-detect is enabled
    if (bridge->state.config.auto_detect) {
        m17_ax25_bridge_detect_protocol(bridge, data, length);
    }
    
    // Process based on current protocol
    switch (bridge->state.current_protocol) {
        case PROTOCOL_M17:
            return m17_ax25_bridge_process_m17_frame(bridge, data, length);
        case PROTOCOL_AX25:
        case PROTOCOL_APRS:
            return m17_ax25_bridge_process_ax25_frame(bridge, data, length);
        case PROTOCOL_FX25:
            return m17_ax25_bridge_process_fx25_frame(bridge, data, length);
        case PROTOCOL_IL2P:
            return m17_ax25_bridge_process_il2p_frame(bridge, data, length);
        default:
            return -1; // Unknown protocol
    }
    
    bridge->state.last_activity = 0; // Reset activity timer
    return 0;
}

// Process M17 frame
int m17_ax25_bridge_process_m17_frame(m17_ax25_bridge_t* bridge, const uint8_t* data, uint16_t length) {
    if (!bridge || !data || length < 2) {
        return -1;
    }
    
    // Validate M17 frame markers
    if (data[0] != 0x5D || data[1] != 0x5F) {
        return -1; // Invalid M17 frame
    }
    
    // Parse M17 frame structure
    if (length < 4) {
        return -1; // Frame too short
    }
    
    // Extract frame type from third byte
    uint8_t frame_type = data[2];
    
    // Process based on frame type (per M17 specification)
    switch (frame_type) {
        case 0x00: // Link Setup Frame (LSF)
            return m17_ax25_bridge_process_m17_lsf(bridge, data, length);
        case 0x01: // Stream Frame
            return m17_ax25_bridge_process_m17_stream(bridge, data, length);
        case 0x02: // Packet Frame
            return m17_ax25_bridge_process_m17_packet(bridge, data, length);
        case 0x03: // BERT (Bit Error Rate Test)
            return m17_ax25_bridge_process_m17_bert(bridge, data, length);
        default:
            return -1; // Unknown frame type
    }
}

// Process M17 Link Setup Frame
int m17_ax25_bridge_process_m17_lsf(m17_ax25_bridge_t* bridge, const uint8_t* data, uint16_t length) {
    if (!bridge || !data || length < 30) {
        return -1;
    }
    
    // Extract callsigns from LSF
    char src_callsign[10] = {0};
    char dst_callsign[10] = {0};
    
    // Parse source callsign (bytes 3-12)
    for (int i = 0; i < 9; i++) {
        src_callsign[i] = data[3 + i];
    }
    
    // Parse destination callsign (bytes 13-22)
    for (int i = 0; i < 9; i++) {
        dst_callsign[i] = data[13 + i];
    }
    
    // Update bridge state with M17 callsigns
    bridge->state.m17_active = true;
    bridge->state.ax25_active = false;
    
    // Log M17 LSF reception
    printf("M17 LSF: %s -> %s\n", src_callsign, dst_callsign);
    
    return 0;
}

// Process M17 Stream Frame
int m17_ax25_bridge_process_m17_stream(m17_ax25_bridge_t* bridge, const uint8_t* data, uint16_t length) {
    if (!bridge || !data || length < 16) {
        return -1;
    }
    
    // Extract audio data from stream frame
    // M17 stream frames contain encoded audio data
    printf("M17 Stream Frame: %d bytes\n", length);
    
    // Implement audio decoding
    if (bridge->state.current_protocol == PROTOCOL_M17) {
        // M17 audio frame decoding
        m17_audio_frame_t audio_frame;
        if (m17_decode_audio_frame(data, length, &audio_frame) == 0) {
            // Convert M17 audio to standard format
            int16_t pcm_samples[160]; // 20ms at 8kHz
            if (m17_audio_to_pcm(&audio_frame, pcm_samples, 160) == 0) {
                // Output audio samples
                printf("M17 Audio: %d samples decoded\n", 160);
                return 0;
            }
        }
    } else if (bridge->state.current_protocol == PROTOCOL_AX25) {
        // AX.25 doesn't typically carry audio, but could carry digitized voice
        printf("AX.25 Data: %d bytes (no audio)\n", length);
        return 0;
    }
    
    return 0;
}

// Process M17 Packet Frame
int m17_ax25_bridge_process_m17_packet(m17_ax25_bridge_t* bridge, const uint8_t* data, uint16_t length) {
    if (!bridge || !data || length < 16) {
        return -1;
    }
    
    // Extract packet data
    printf("M17 Packet Frame: %d bytes\n", length);
    
    // Implement packet processing
    m17_packet_frame_t packet_frame;
    if (m17_decode_packet_frame(data, length, &packet_frame) == 0) {
        // Extract packet data
        uint8_t packet_data[256];
        uint16_t packet_length = packet_frame.length - 16; // Subtract header
        
        if (packet_length > 0 && packet_length <= 256) {
            memcpy(packet_data, data + 16, packet_length);
            
            // Validate packet data
            if (m17_validate_packet_data(packet_data, packet_length) == 0) {
                // Process application layer data
                printf("M17 Packet: %d bytes of data\n", packet_length);
                
                // Handle different packet types
                switch (packet_frame.type) {
                    case M17_PACKET_TYPE_DATA:
                        printf("Data packet received\n");
                        break;
                    case M17_PACKET_TYPE_APRS:
                        printf("APRS packet received\n");
                        break;
                    case M17_PACKET_TYPE_TEXT:
                        printf("Text packet received\n");
                        break;
                    default:
                        printf("Unknown packet type: %d\n", packet_frame.type);
                        break;
                }
                
                return 0;
            }
        }
    }
    
    return 0;
}

// Process M17 BERT (Bit Error Rate Test)
int m17_ax25_bridge_process_m17_bert(m17_ax25_bridge_t* bridge, const uint8_t* data, uint16_t length) {
    if (!bridge || !data || length < 4) {
        return -1;
    }
    
    printf("M17 BERT (Bit Error Rate Test)\n");
    
    // BERT frames are used for testing - no special processing needed
    return 0;
}

// Process AX.25 frame
int m17_ax25_bridge_process_ax25_frame(m17_ax25_bridge_t* bridge, const uint8_t* data, uint16_t length) {
    if (!bridge || !data || length < 3) {
        return -1;
    }
    
    // Validate AX.25 frame markers
    if (data[0] != 0x7E) {
        return -1; // Invalid AX.25 frame
    }
    
    // Find closing flag
    uint16_t frame_end = 0;
    for (uint16_t i = 1; i < length; i++) {
        if (data[i] == 0x7E) {
            frame_end = i;
            break;
        }
    }
    
    if (frame_end == 0) {
        return -1; // No closing flag found
    }
    
    // Parse AX.25 frame
    return m17_ax25_bridge_parse_ax25_frame(bridge, data, frame_end);
}

// Parse AX.25 frame
int m17_ax25_bridge_parse_ax25_frame(m17_ax25_bridge_t* bridge, const uint8_t* data, uint16_t length) {
    if (!bridge || !data || length < 18) {
        return -1;
    }
    
    // Extract addresses (minimum 2 addresses + control)
    char src_callsign[7] = {0};
    char dst_callsign[7] = {0};
    uint8_t src_ssid = 0;
    uint8_t dst_ssid = 0;
    
    // Parse destination address (bytes 1-7)
    for (int i = 0; i < 6; i++) {
        dst_callsign[i] = data[1 + i] >> 1;
    }
    dst_ssid = (data[7] >> 1) & 0x0F;  // SSID is in bits 4-1
    
    // Parse source address (bytes 8-14)
    for (int i = 0; i < 6; i++) {
        src_callsign[i] = data[8 + i] >> 1;
    }
    src_ssid = (data[14] >> 1) & 0x0F;  // SSID is in bits 4-1
    
    // Extract control field
    uint8_t control = data[15];
    
    // Process based on frame type
    if (control & 0x01) {
        // I-frame (Information frame)
        return m17_ax25_bridge_process_ax25_iframe(bridge, data, length, src_callsign, dst_callsign);
    } else if (control & 0x02) {
        // S-frame (Supervisory frame)
        return m17_ax25_bridge_process_ax25_sframe(bridge, data, length, src_callsign, dst_callsign);
    } else {
        // U-frame (Unnumbered frame)
        return m17_ax25_bridge_process_ax25_uframe(bridge, data, length, src_callsign, dst_callsign);
    }
}

// Process AX.25 I-frame
int m17_ax25_bridge_process_ax25_iframe(m17_ax25_bridge_t* bridge, const uint8_t* data, uint16_t length, 
                                       const char* src_callsign, const char* dst_callsign) {
    if (!bridge || !data || length < 18) {
        return -1;
    }
    
    // Extract information field
    uint16_t info_start = 16; // Skip addresses and control
    uint16_t info_length = length - info_start - 2; // Subtract FCS
    
    printf("AX.25 I-frame: %s -> %s (%d bytes)\n", src_callsign, dst_callsign, info_length);
    
    // Update bridge state
    bridge->state.ax25_active = true;
    bridge->state.m17_active = false;
    
    return 0;
}

// Process AX.25 S-frame
int m17_ax25_bridge_process_ax25_sframe(m17_ax25_bridge_t* bridge, const uint8_t* data, uint16_t length,
                                       const char* src_callsign, const char* dst_callsign) {
    if (!bridge || !data || length < 16) {
        return -1;
    }
    
    uint8_t control = data[15];
    const char* frame_type = "Unknown";
    
    // Determine S-frame type
    switch (control & 0x0C) {
        case 0x00: frame_type = "RR"; break;
        case 0x04: frame_type = "RNR"; break;
        case 0x08: frame_type = "REJ"; break;
        case 0x0C: frame_type = "SREJ"; break;
    }
    
    printf("AX.25 S-frame (%s): %s -> %s\n", frame_type, src_callsign, dst_callsign);
    
    return 0;
}

// Process M17 TX
int m17_ax25_bridge_process_m17_tx(m17_ax25_bridge_t* bridge, const uint8_t* data, uint16_t length) {
    if (!bridge || !data || length == 0) {
        return -1;
    }
    
    // Create M17 frame
    m17_frame_t m17_frame;
    if (m17_create_frame(data, length, &m17_frame) != 0) {
        return -1;
    }
    
    // Encode M17 frame
    uint8_t encoded_data[256];
    uint16_t encoded_length;
    if (m17_encode_frame(&m17_frame, encoded_data, &encoded_length) != 0) {
        return -1;
    }
    
    // Send via KISS TNC
    if (kiss_serial_send(&bridge->kiss_tnc, encoded_data, encoded_length) != 0) {
        return -1;
    }
    
    printf("M17 TX: %d bytes transmitted\n", encoded_length);
    return 0;
}

// Process AX.25 TX
int m17_ax25_bridge_process_ax25_tx(m17_ax25_bridge_t* bridge, const uint8_t* data, uint16_t length) {
    if (!bridge || !data || length == 0) {
        return -1;
    }
    
    // Create AX.25 frame
    ax25_frame_t ax25_frame;
    ax25_address_t src_addr, dst_addr;
    
    // Set default addresses
    ax25_set_address(&src_addr, "N0CALL", 0, false);
    ax25_set_address(&dst_addr, "N0CALL", 0, false);
    
    if (ax25_create_frame(&ax25_frame, &src_addr, &dst_addr, 0x03, 0xF0, data, length) != 0) {
        return -1;
    }
    
    // Encode AX.25 frame
    uint8_t encoded_data[256];
    uint16_t encoded_length;
    if (ax25_encode_frame(&ax25_frame, encoded_data, &encoded_length) != 0) {
        return -1;
    }
    
    // Send via KISS TNC
    if (kiss_serial_send(&bridge->kiss_tnc, encoded_data, encoded_length) != 0) {
        return -1;
    }
    
    printf("AX.25 TX: %d bytes transmitted\n", encoded_length);
    return 0;
}

// M17 Helper Functions
int m17_decode_audio_frame(const uint8_t* data, uint16_t length, m17_audio_frame_t* frame) {
    if (!data || length < 16 || !frame) {
        return -1;
    }
    
    memcpy(frame->data, data, 16);
    frame->length = 16;
    return 0;
}

int m17_audio_to_pcm(const m17_audio_frame_t* frame, int16_t* pcm_samples, uint16_t sample_count) {
    if (!frame || !pcm_samples || sample_count == 0) {
        return -1;
    }
    
    // Simple conversion - in real implementation, this would decode M17 audio
    for (uint16_t i = 0; i < sample_count && i < 160; i++) {
        pcm_samples[i] = (int16_t)(frame->data[i % 16] * 256 - 32768);
    }
    
    return 0;
}

int m17_decode_packet_frame(const uint8_t* data, uint16_t length, m17_packet_frame_t* frame) {
    if (!data || length < 16 || !frame) {
        return -1;
    }
    
    frame->type = data[0];
    frame->length = length;
    memcpy(frame->data, data, length);
    return 0;
}

int m17_validate_packet_data(const uint8_t* data, uint16_t length) {
    if (!data || length == 0) {
        return -1;
    }
    
    // Basic validation - check for reasonable data
    for (uint16_t i = 0; i < length; i++) {
        if (data[i] == 0xFF && i > 0 && data[i-1] == 0xFF) {
            return -1; // Invalid sequence
        }
    }
    
    return 0;
}

int m17_create_frame(const uint8_t* data, uint16_t length, m17_frame_t* frame) {
    if (!data || length == 0 || !frame) {
        return -1;
    }
    
    frame->length = length;
    memcpy(frame->data, data, length);
    return 0;
}

int m17_encode_frame(const m17_frame_t* frame, uint8_t* data, uint16_t* length) {
    if (!frame || !data || !length) {
        return -1;
    }
    
    *length = frame->length;
    memcpy(data, frame->data, frame->length);
    return 0;
}

// Process AX.25 U-frame
int m17_ax25_bridge_process_ax25_uframe(m17_ax25_bridge_t* bridge, const uint8_t* data, uint16_t length,
                                       const char* src_callsign, const char* dst_callsign) {
    if (!bridge || !data || length < 16) {
        return -1;
    }
    
    uint8_t control = data[15];
    const char* frame_type = "Unknown";
    
    // Determine U-frame type
    switch (control) {
        case 0x2F: frame_type = "SABM"; break;
        case 0x43: frame_type = "DISC"; break;
        case 0x63: frame_type = "UA"; break;
        case 0x87: frame_type = "DM"; break;
        case 0x03: frame_type = "UI"; break;
        case 0xAF: frame_type = "FRMR"; break;
    }
    
    printf("AX.25 U-frame (%s): %s -> %s\n", frame_type, src_callsign, dst_callsign);
    
    // Check for APRS (UI frame with PID 0xF0)
    if (control == 0x03 && length > 18) {
        uint8_t pid = data[16];
        if (pid == 0xF0) {
            return m17_ax25_bridge_process_aprs_frame(bridge, data, length, src_callsign, dst_callsign);
        }
    }
    
    return 0;
}

// Process APRS frame
int m17_ax25_bridge_process_aprs_frame(m17_ax25_bridge_t* bridge, const uint8_t* data, uint16_t length,
                                      const char* src_callsign, const char* dst_callsign) {
    if (!bridge || !data || length < 20) {
        return -1;
    }
    
    // Extract APRS data (after PID)
    uint16_t aprs_start = 17;
    uint16_t aprs_length = length - aprs_start - 2; // Subtract FCS
    
    printf("APRS: %s -> %s (%d bytes)\n", src_callsign, dst_callsign, aprs_length);
    
    // Update bridge state
    bridge->state.current_protocol = PROTOCOL_APRS;
    bridge->state.ax25_active = true;
    bridge->state.m17_active = false;
    
    return 0;
}

// Process TX data
int m17_ax25_bridge_process_tx_data(m17_ax25_bridge_t* bridge, const uint8_t* data, uint16_t length, 
                                   protocol_type_t protocol) {
    if (!bridge || !data || length == 0) {
        return -1;
    }
    
    // Set protocol
    m17_ax25_bridge_set_protocol(bridge, protocol);
    
    // Process based on protocol
    switch (protocol) {
        case PROTOCOL_M17:
            // Process M17 TX
            if (m17_ax25_bridge_process_m17_tx(bridge, data, length) != 0) {
                return -1;
            }
            break;
        case PROTOCOL_AX25:
        case PROTOCOL_APRS:
            // Process AX.25 TX
            if (m17_ax25_bridge_process_ax25_tx(bridge, data, length) != 0) {
                return -1;
            }
            break;
        default:
            return -1; // Unknown protocol
    }
    
    return 0;
}

// Send APRS position
// Note: Current hardware does not have GPS capability
// This function accepts manual coordinates for testing/configuration
// Future GPS module can be added via USB or I2C interface
int m17_ax25_bridge_send_aprs_position(m17_ax25_bridge_t* bridge, const char* callsign, 
                                      double latitude, double longitude, int altitude,
                                      const char* comment) {
    if (!bridge || !callsign) {
        return -1;
    }
    
    // Implement APRS position transmission
    // This involves:
    // 1. Format APRS position packet
    // 2. Create AX.25 UI frame
    // 3. Send via KISS interface
    
    // Create APRS position packet
    char aprs_packet[256];
    int packet_len = snprintf(aprs_packet, sizeof(aprs_packet),
        "!%08.2f%c/%09.2f%c%c%03d%c%s",
        fabs(latitude), (latitude >= 0) ? 'N' : 'S',
        fabs(longitude), (longitude >= 0) ? 'E' : 'W',
        (altitude > 0) ? '/' : ' ',
        altitude,
        (altitude > 0) ? ' ' : '\0',
        comment ? comment : "");
    
    if (packet_len >= sizeof(aprs_packet)) {
        return -1; // Packet too long
    }
    
    // Create AX.25 addresses
    ax25_address_t src_addr, dst_addr;
    
    // Set source address
    if (ax25_set_address(&src_addr, callsign, 0, true) != 0) {
        return -1;
    }
    
    // Set destination address (APRS-IS)
    if (ax25_set_address(&dst_addr, "APRS", 0, false) != 0) {
        return -1;
    }
    
    // Send UI frame via AX.25 TNC
    if (ax25_send_ui_frame(&bridge->ax25_tnc, &src_addr, &dst_addr,
                          NULL, 0, AX25_PID_NONE,
                          (const uint8_t*)aprs_packet, packet_len) != 0) {
        return -1;
    }
    
    return 0;
}

// Send APRS status
int m17_ax25_bridge_send_aprs_status(m17_ax25_bridge_t* bridge, const char* callsign, 
                                    const char* status) {
    if (!bridge || !callsign || !status) {
        return -1;
    }
    
    // Implement APRS status transmission
    // This involves:
    // 1. Format APRS status packet
    // 2. Create AX.25 UI frame
    // 3. Send via KISS interface
    
    // Create APRS status packet
    char aprs_packet[256];
    int packet_len = snprintf(aprs_packet, sizeof(aprs_packet),
        ">%s", status ? status : "Status");
    
    if (packet_len >= sizeof(aprs_packet)) {
        return -1; // Packet too long
    }
    
    // Create AX.25 addresses
    ax25_address_t src_addr, dst_addr;
    
    // Set source address
    if (ax25_set_address(&src_addr, callsign, 0, true) != 0) {
        return -1;
    }
    
    // Set destination address (APRS-IS)
    if (ax25_set_address(&dst_addr, "APRS", 0, false) != 0) {
        return -1;
    }
    
    // Send UI frame via AX.25 TNC
    if (ax25_send_ui_frame(&bridge->ax25_tnc, &src_addr, &dst_addr,
                          NULL, 0, AX25_PID_NONE,
                          (const uint8_t*)aprs_packet, packet_len) != 0) {
        return -1;
    }
    
    return 0;
}

// Send APRS message
int m17_ax25_bridge_send_aprs_message(m17_ax25_bridge_t* bridge, const char* from_callsign,
                                      const char* to_callsign, const char* message) {
    if (!bridge || !from_callsign || !to_callsign || !message) {
        return -1;
    }
    
    // Implement APRS message transmission
    // This involves:
    // 1. Format APRS message packet
    // 2. Create AX.25 UI frame
    // 3. Send via KISS interface
    
    // Create APRS message packet
    char aprs_packet[256];
    int packet_len = snprintf(aprs_packet, sizeof(aprs_packet),
        ":%-9s:%s", to_callsign, message ? message : "");
    
    if (packet_len >= sizeof(aprs_packet)) {
        return -1; // Packet too long
    }
    
    // Create AX.25 addresses
    ax25_address_t src_addr, dst_addr;
    
    // Set source address
    if (ax25_set_address(&src_addr, from_callsign, 0, true) != 0) {
        return -1;
    }
    
    // Set destination address (APRS-IS)
    if (ax25_set_address(&dst_addr, "APRS", 0, false) != 0) {
        return -1;
    }
    
    // Send UI frame via AX.25 TNC
    if (ax25_send_ui_frame(&bridge->ax25_tnc, &src_addr, &dst_addr,
                          NULL, 0, AX25_PID_NONE,
                          (const uint8_t*)aprs_packet, packet_len) != 0) {
        return -1;
    }
    
    return 0;
}

// Validate callsign
int m17_ax25_bridge_validate_callsign(const char* callsign) {
    if (!callsign) {
        return -1;
    }
    
    int len = strlen(callsign);
    if (len < 3 || len > 9) {
        return -1; // Invalid length
    }
    
    // Check for valid characters (A-Z, 0-9)
    for (int i = 0; i < len; i++) {
        char c = callsign[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) {
            return -1; // Invalid character
        }
    }
    
    return 0;
}

// Normalize callsign
int m17_ax25_bridge_normalize_callsign(char* callsign) {
    if (!callsign) {
        return -1;
    }
    
    // Convert to uppercase
    for (int i = 0; callsign[i]; i++) {
        if (callsign[i] >= 'a' && callsign[i] <= 'z') {
            callsign[i] = callsign[i] - 'a' + 'A';
        }
    }
    
    return 0;
}

// Compare callsigns
int m17_ax25_bridge_compare_callsigns(const char* callsign1, const char* callsign2) {
    if (!callsign1 || !callsign2) {
        return -1;
    }
    
    return strcmp(callsign1, callsign2);
}

// Get statistics
int m17_ax25_bridge_get_statistics(const m17_ax25_bridge_t* bridge, bridge_statistics_t* stats) {
    if (!bridge || !stats) {
        return -1;
    }
    
    // Implement statistics collection
    // For now, return zero statistics
    // In a real implementation, you would track:
    // - Frame counts for each protocol
    // - Protocol switches
    // - Conversion errors
    // - Performance metrics
    
    stats->m17_frames_rx = 0;
    stats->m17_frames_tx = 0;
    stats->ax25_frames_rx = 0;
    stats->ax25_frames_tx = 0;
    stats->aprs_frames_rx = 0;
    stats->aprs_frames_tx = 0;
    stats->protocol_switches = 0;
    stats->conversion_errors = 0;
    
    return 0;
}

// Reset statistics
int m17_ax25_bridge_reset_statistics(m17_ax25_bridge_t* bridge) {
    if (!bridge) {
        return -1;
    }
    
    // Reset statistics
    // In a real implementation, you would reset all counters to zero
    // For now, just return success
    return 0;
}

// Register event handler
int m17_ax25_bridge_register_event_handler(m17_ax25_bridge_t* bridge, bridge_event_handler_t handler) {
    if (!bridge || !handler) {
        return -1;
    }
    
    // Implement event handler registration
    bridge->event_handler = handler;
    bridge->event_handler_registered = true;
    
    return 0;
}

// Unregister event handler
int m17_ax25_bridge_unregister_event_handler(m17_ax25_bridge_t* bridge) {
    if (!bridge) {
        return -1;
    }
    
    // Implement event handler unregistration
    bridge->event_handler = NULL;
    bridge->event_handler_registered = false;
    return 0;
}

// Load configuration
int m17_ax25_bridge_load_config(m17_ax25_bridge_t* bridge, const char* config_file) {
    if (!bridge || !config_file) {
        return -1;
    }
    
    // Implement configuration loading
    FILE* file = fopen(config_file, "r");
    if (!file) {
        return -1;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        // Parse configuration line
        if (strncmp(line, "m17_enabled=", 12) == 0) {
            bridge->state.config.m17_enabled = (strcmp(line + 12, "true\n") == 0);
        } else if (strncmp(line, "ax25_enabled=", 13) == 0) {
            bridge->state.config.ax25_enabled = (strcmp(line + 13, "true\n") == 0);
        } else if (strncmp(line, "m17_frequency=", 14) == 0) {
            bridge->state.config.m17_frequency = atoi(line + 14);
        } else if (strncmp(line, "ax25_frequency=", 15) == 0) {
            bridge->state.config.ax25_frequency = atoi(line + 15);
        }
    }
    
    fclose(file);
    
    return 0;
}

// Save configuration
int m17_ax25_bridge_save_config(const m17_ax25_bridge_t* bridge, const char* config_file) {
    if (!bridge || !config_file) {
        return -1;
    }
    
    // Implement configuration saving
    FILE* file = fopen(config_file, "w");
    if (!file) {
        return -1;
    }
    
    fprintf(file, "m17_enabled=%s\n", bridge->state.config.m17_enabled ? "true" : "false");
    fprintf(file, "ax25_enabled=%s\n", bridge->state.config.ax25_enabled ? "true" : "false");
    fprintf(file, "m17_frequency=%u\n", bridge->state.config.m17_frequency);
    fprintf(file, "ax25_frequency=%u\n", bridge->state.config.ax25_frequency);
    fprintf(file, "m17_can=%u\n", bridge->state.config.m17_can);
    fprintf(file, "ax25_callsign=%s\n", bridge->state.config.ax25_callsign);
    fprintf(file, "ax25_ssid=%u\n", bridge->state.config.ax25_ssid);
    
    fclose(file);
    
    return 0;
}

// Enable debug
int m17_ax25_bridge_enable_debug(m17_ax25_bridge_t* bridge, bool enable) {
    if (!bridge) {
        return -1;
    }
    
    // Implement debug enable/disable
    bridge->debug_enabled = enable;
    if (enable) {
        printf("M17-AX.25 Bridge: Debug enabled\n");
    } else {
        printf("M17-AX.25 Bridge: Debug disabled\n");
    }
    
    return 0;
}

// Set debug level
int m17_ax25_bridge_set_debug_level(m17_ax25_bridge_t* bridge, int level) {
    if (!bridge) {
        return -1;
    }
    
    // Implement debug level setting
    bridge->debug_level = level;
    printf("M17-AX.25 Bridge: Debug level set to %d\n", level);
    
    return 0;
}

// Print status
int m17_ax25_bridge_print_status(const m17_ax25_bridge_t* bridge) {
    if (!bridge) {
        return -1;
    }
    
    printf("M17-AX.25 Bridge Status:\n");
    printf("  M17 Enabled: %s\n", bridge->state.config.m17_enabled ? "Yes" : "No");
    printf("  AX.25 Enabled: %s\n", bridge->state.config.ax25_enabled ? "Yes" : "No");
    printf("  FX.25 Enabled: %s\n", bridge->state.config.fx25_enabled ? "Yes" : "No");
    printf("  IL2P Enabled: %s\n", bridge->state.config.il2p_enabled ? "Yes" : "No");
    printf("  Auto Detect: %s\n", bridge->state.config.auto_detect ? "Yes" : "No");
    printf("  Current Protocol: %d\n", bridge->state.current_protocol);
    printf("  M17 Active: %s\n", bridge->state.m17_active ? "Yes" : "No");
    printf("  AX.25 Active: %s\n", bridge->state.ax25_active ? "Yes" : "No");
    printf("  FX.25 Active: %s\n", bridge->state.fx25_active ? "Yes" : "No");
    printf("  IL2P Active: %s\n", bridge->state.il2p_active ? "Yes" : "No");
    printf("  Mappings: %d\n", bridge->num_mappings);
    
    return 0;
}

// FX.25 Frame Processing
int m17_ax25_bridge_process_fx25_frame(m17_ax25_bridge_t* bridge, const uint8_t* data, uint16_t length) {
    if (!bridge || !data || length == 0) {
        return -1;
    }
    
    // Detect FX.25 frame
    int frame_pos = fx25_detect_frame(data, length);
    if (frame_pos < 0) {
        return -1;
    }
    
    // Extract FX.25 frame
    fx25_frame_t fx25_frame;
    if (fx25_extract_frame(data + frame_pos, length - frame_pos, &fx25_frame) != 0) {
        return -1;
    }
    
    // Decode FX.25 frame to AX.25
    uint8_t ax25_data[FX25_MAX_FRAME_SIZE];
    uint16_t ax25_length;
    if (fx25_decode_frame(&bridge->state.fx25_ctx, &fx25_frame, ax25_data, &ax25_length) != 0) {
        return -1;
    }
    
    // Process as AX.25 frame
    return m17_ax25_bridge_process_ax25_frame(bridge, ax25_data, ax25_length);
}

int m17_ax25_bridge_encode_fx25_frame(m17_ax25_bridge_t* bridge, const uint8_t* ax25_data, uint16_t ax25_length,
                                       uint8_t* fx25_data, uint16_t* fx25_length) {
    if (!bridge || !ax25_data || !fx25_data || !fx25_length) {
        return -1;
    }
    
    // Encode AX.25 frame to FX.25
    fx25_frame_t fx25_frame;
    if (fx25_encode_frame(&bridge->state.fx25_ctx, ax25_data, ax25_length, &fx25_frame) != 0) {
        return -1;
    }
    
    // Convert FX.25 frame to byte stream
    int offset = 0;
    
    // Preamble
    memcpy(fx25_data + offset, fx25_frame.preamble, FX25_PREAMBLE_LEN);
    offset += FX25_PREAMBLE_LEN;
    
    // Sync word
    memcpy(fx25_data + offset, fx25_frame.sync_word, FX25_SYNC_WORD_LEN);
    offset += FX25_SYNC_WORD_LEN;
    
    // Header
    memcpy(fx25_data + offset, fx25_frame.header, FX25_HEADER_LEN);
    offset += FX25_HEADER_LEN;
    
    // Data
    memcpy(fx25_data + offset, fx25_frame.data, fx25_frame.data_length);
    offset += fx25_frame.data_length;
    
    // Parity
    memcpy(fx25_data + offset, fx25_frame.parity, fx25_frame.parity_length);
    offset += fx25_frame.parity_length;
    
    // CRC
    memcpy(fx25_data + offset, fx25_frame.crc, FX25_CRC_LEN);
    offset += FX25_CRC_LEN;
    
    *fx25_length = offset;
    
    return 0;
}

int m17_ax25_bridge_decode_fx25_frame(m17_ax25_bridge_t* bridge, const uint8_t* fx25_data, uint16_t fx25_length,
                                       uint8_t* ax25_data, uint16_t* ax25_length) {
    if (!bridge || !fx25_data || !ax25_data || !ax25_length) {
        return -1;
    }
    
    // Extract FX.25 frame
    fx25_frame_t fx25_frame;
    if (fx25_extract_frame(fx25_data, fx25_length, &fx25_frame) != 0) {
        return -1;
    }
    
    // Decode FX.25 frame to AX.25
    return fx25_decode_frame(&bridge->state.fx25_ctx, &fx25_frame, ax25_data, ax25_length);
}

// IL2P Frame Processing
int m17_ax25_bridge_process_il2p_frame(m17_ax25_bridge_t* bridge, const uint8_t* data, uint16_t length) {
    if (!bridge || !data || length == 0) {
        return -1;
    }
    
    // Detect IL2P frame
    int frame_pos = il2p_detect_frame(data, length);
    if (frame_pos < 0) {
        return -1;
    }
    
    // Extract IL2P frame
    il2p_frame_t il2p_frame;
    if (il2p_extract_frame(data + frame_pos, length - frame_pos, &il2p_frame) != 0) {
        return -1;
    }
    
    // Decode IL2P frame
    uint8_t decoded_data[IL2P_MAX_PAYLOAD_SIZE];
    uint16_t decoded_length;
    if (il2p_decode_frame(&bridge->state.il2p_ctx, &il2p_frame, decoded_data, &decoded_length) != 0) {
        return -1;
    }
    
    // Process as data frame
    printf("IL2P frame received: %d bytes\n", decoded_length);
    
    return 0;
}

int m17_ax25_bridge_encode_il2p_frame(m17_ax25_bridge_t* bridge, const uint8_t* data, uint16_t length,
                                      uint8_t* il2p_data, uint16_t* il2p_length) {
    if (!bridge || !data || !il2p_data || !il2p_length) {
        return -1;
    }
    
    // Encode data to IL2P frame
    il2p_frame_t il2p_frame;
    if (il2p_encode_frame(&bridge->state.il2p_ctx, data, length, &il2p_frame) != 0) {
        return -1;
    }
    
    // Convert IL2P frame to byte stream
    int offset = 0;
    
    // Preamble
    il2p_data[offset++] = il2p_frame.preamble;
    
    // Sync word
    memcpy(il2p_data + offset, il2p_frame.sync_word, IL2P_SYNC_WORD_SIZE);
    offset += IL2P_SYNC_WORD_SIZE;
    
    // Header
    memcpy(il2p_data + offset, il2p_frame.header, IL2P_HEADER_SIZE);
    offset += IL2P_HEADER_SIZE;
    
    // Header parity
    memcpy(il2p_data + offset, il2p_frame.header_parity, IL2P_HEADER_PARITY);
    offset += IL2P_HEADER_PARITY;
    
    // Payload
    memcpy(il2p_data + offset, il2p_frame.payload, il2p_frame.payload_length);
    offset += il2p_frame.payload_length;
    
    // Payload parity
    memcpy(il2p_data + offset, il2p_frame.payload_parity, il2p_frame.parity_length);
    offset += il2p_frame.parity_length;
    
    *il2p_length = offset;
    
    return 0;
}

int m17_ax25_bridge_decode_il2p_frame(m17_ax25_bridge_t* bridge, const uint8_t* il2p_data, uint16_t il2p_length,
                                      uint8_t* data, uint16_t* length) {
    if (!bridge || !il2p_data || !data || !length) {
        return -1;
    }
    
    // Extract IL2P frame
    il2p_frame_t il2p_frame;
    if (il2p_extract_frame(il2p_data, il2p_length, &il2p_frame) != 0) {
        return -1;
    }
    
    // Decode IL2P frame
    return il2p_decode_frame(&bridge->state.il2p_ctx, &il2p_frame, data, length);
}
