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
    strcpy(bridge->state.config.ax25_callsign, "N0CALL");
    bridge->state.config.ax25_ssid = 0;
    
    bridge->state.current_protocol = PROTOCOL_UNKNOWN;
    bridge->state.m17_active = false;
    bridge->state.ax25_active = false;
    bridge->state.last_activity = 0;
    bridge->state.protocol_timeout = 5000; // 5 seconds
    
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
    
    // Check for M17 frame markers
    if (length >= 2 && data[0] == 0x5D && data[1] == 0x5F) {
        bridge->state.current_protocol = PROTOCOL_M17;
        bridge->state.m17_active = true;
        bridge->state.ax25_active = false;
        return 0;
    }
    
    // Check for AX.25 frame markers (flag 0x7E)
    if (data[0] == 0x7E) {
        bridge->state.current_protocol = PROTOCOL_AX25;
        bridge->state.ax25_active = true;
        bridge->state.m17_active = false;
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
    
    // For now, create a simple AX.25 UI frame
    // This is a placeholder implementation
    if (*ax25_length < 32) {
        return -1; // Buffer too small
    }
    
    // Create basic AX.25 frame structure
    ax25_data[0] = 0x7E; // Opening flag
    ax25_data[1] = 0x7E; // Second flag for APRS
    
    // Add basic address fields (placeholder)
    // In real implementation, extract from M17 frame
    for (int i = 2; i < 16; i++) {
        ax25_data[i] = 0x40; // Placeholder address bytes
    }
    
    ax25_data[16] = 0x03; // Control field (UI frame)
    ax25_data[17] = 0xF0; // PID (no layer 3)
    
    // Copy M17 data as information field (truncated)
    uint16_t info_len = (m17_length > 10) ? 10 : m17_length;
    for (uint16_t i = 0; i < info_len; i++) {
        ax25_data[18 + i] = m17_data[i];
    }
    
    // Add FCS (simplified)
    ax25_data[18 + info_len] = 0x00;
    ax25_data[18 + info_len + 1] = 0x00;
    
    // Add closing flag
    ax25_data[18 + info_len + 2] = 0x7E;
    
    *ax25_length = 18 + info_len + 3;
    
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
    
    // For now, create a simple M17 frame
    // This is a placeholder implementation
    if (*m17_length < 16) {
        return -1; // Buffer too small
    }
    
    // Add M17 frame markers
    m17_data[0] = 0x5D;
    m17_data[1] = 0x5F;
    
    // Copy AX.25 information field as M17 data (truncated)
    // In real implementation, you would parse the AX.25 frame properly
    uint16_t info_start = 18; // Skip address fields and control/PID
    uint16_t info_len = (frame_end - info_start > 10) ? 10 : (frame_end - info_start);
    
    if (info_len > 0) {
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
            strcpy(ax25_callsign, bridge->mappings[i].ax25_callsign);
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
            // TODO: Process M17 frame
            break;
        case PROTOCOL_AX25:
        case PROTOCOL_APRS:
            // TODO: Process AX.25 frame
            break;
        default:
            return -1; // Unknown protocol
    }
    
    bridge->state.last_activity = 0; // Reset activity timer
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
            // TODO: Process M17 TX
            break;
        case PROTOCOL_AX25:
        case PROTOCOL_APRS:
            // TODO: Process AX.25 TX
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
    
    // TODO: Implement event handler registration
    (void)handler;
    
    return 0;
}

// Unregister event handler
int m17_ax25_bridge_unregister_event_handler(m17_ax25_bridge_t* bridge) {
    if (!bridge) {
        return -1;
    }
    
    // TODO: Implement event handler unregistration
    return 0;
}

// Load configuration
int m17_ax25_bridge_load_config(m17_ax25_bridge_t* bridge, const char* config_file) {
    if (!bridge || !config_file) {
        return -1;
    }
    
    // TODO: Implement configuration loading
    (void)config_file;
    
    return 0;
}

// Save configuration
int m17_ax25_bridge_save_config(const m17_ax25_bridge_t* bridge, const char* config_file) {
    if (!bridge || !config_file) {
        return -1;
    }
    
    // TODO: Implement configuration saving
    (void)config_file;
    
    return 0;
}

// Enable debug
int m17_ax25_bridge_enable_debug(m17_ax25_bridge_t* bridge, bool enable) {
    if (!bridge) {
        return -1;
    }
    
    // TODO: Implement debug enable/disable
    (void)enable;
    
    return 0;
}

// Set debug level
int m17_ax25_bridge_set_debug_level(m17_ax25_bridge_t* bridge, int level) {
    if (!bridge) {
        return -1;
    }
    
    // TODO: Implement debug level setting
    (void)level;
    
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
    printf("  Auto Detect: %s\n", bridge->state.config.auto_detect ? "Yes" : "No");
    printf("  Current Protocol: %d\n", bridge->state.current_protocol);
    printf("  M17 Active: %s\n", bridge->state.m17_active ? "Yes" : "No");
    printf("  AX.25 Active: %s\n", bridge->state.ax25_active ? "Yes" : "No");
    printf("  Mappings: %d\n", bridge->num_mappings);
    
    return 0;
}
