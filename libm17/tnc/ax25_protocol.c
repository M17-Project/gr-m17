//--------------------------------------------------------------------
// AX.25 Protocol Implementation for M17
//
// AX.25 (Amateur X.25) protocol implementation
// Compatible with traditional packet radio and APRS
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------
#include "ax25_protocol.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// Initialize AX.25 TNC
int ax25_init(ax25_tnc_t* tnc) {
    if (!tnc) {
        return -1;
    }
    
    // Initialize configuration with defaults
    memset(tnc->config.my_address.callsign, 0, 6);
    tnc->config.my_address.ssid = 0;
    tnc->config.my_address.command = false;
    tnc->config.my_address.has_been_repeated = false;
    
    tnc->config.tx_delay = 50;          // 500ms
    tnc->config.persistence = 63;       // 63/256
    tnc->config.slot_time = 10;         // 100ms
    tnc->config.tx_tail = 5;            // 50ms
    tnc->config.full_duplex = false;
    tnc->config.max_frame_length = 256;
    tnc->config.window_size = 4;
    tnc->config.t1_timeout = 3000;       // 3 seconds
    tnc->config.t2_timeout = 1000;     // 1 second
    tnc->config.t3_timeout = 30000;     // 30 seconds
    tnc->config.max_retries = 3;
    
    // Initialize connections
    tnc->num_connections = 0;
    for (int i = 0; i < 16; i++) {
        tnc->connections[i].state = AX25_STATE_DISCONNECTED;
    }
    
    // Initialize frames
    memset(&tnc->rx_frame, 0, sizeof(ax25_frame_t));
    memset(&tnc->tx_frame, 0, sizeof(ax25_frame_t));
    tnc->frame_ready = false;
    
    return 0;
}

// Cleanup AX.25 TNC
int ax25_cleanup(ax25_tnc_t* tnc) {
    if (!tnc) {
        return -1;
    }
    
    // Disconnect all connections
    for (int i = 0; i < tnc->num_connections; i++) {
        ax25_disconnect(tnc, &tnc->connections[i].remote_addr);
    }
    
    tnc->num_connections = 0;
    tnc->frame_ready = false;
    
    return 0;
}

// Set AX.25 configuration
int ax25_set_config(ax25_tnc_t* tnc, const ax25_config_t* config) {
    if (!tnc || !config) {
        return -1;
    }
    
    tnc->config = *config;
    return 0;
}

// Get AX.25 configuration
int ax25_get_config(const ax25_tnc_t* tnc, ax25_config_t* config) {
    if (!tnc || !config) {
        return -1;
    }
    
    *config = tnc->config;
    return 0;
}

// Set AX.25 address
int ax25_set_address(ax25_address_t* addr, const char* callsign, uint8_t ssid, bool command) {
    if (!addr || !callsign) {
        return -1;
    }
    
    // Clear callsign
    memset(addr->callsign, 0, 6);
    
    // Copy callsign (max 6 characters)
    int len = strlen(callsign);
    if (len > 6) len = 6;
    
    for (int i = 0; i < len; i++) {
        addr->callsign[i] = toupper(callsign[i]) << 1;
    }
    
    // Set SSID and flags
    // AX.25 SSID byte format (Dire Wolf authoritative):
    // Bit 7 (MSB): H bit (has-been-repeated/command-response)
    // Bit 6: Reserved (should be 1)
    // Bit 5: Reserved (should be 1)
    // Bits 4-1: SSID (0-15)
    // Bit 0 (LSB): Last address flag (1 = last address, 0 = more addresses)
    addr->ssid = (ssid & 0x0F) << 1 | 0x60 | (command ? 0x80 : 0x00) | 0x01;
    addr->command = command;
    addr->has_been_repeated = false;
    
    return 0;
}

// Get AX.25 address
int ax25_get_address(const ax25_address_t* addr, char* callsign, uint8_t* ssid, bool* command) {
    if (!addr || !callsign) {
        return -1;
    }
    
    // Convert callsign back to ASCII
    for (int i = 0; i < 6; i++) {
        if (addr->callsign[i] == 0) {
            callsign[i] = '\0';
            break;
        }
        callsign[i] = (addr->callsign[i] >> 1) & 0x7F;
    }
    callsign[6] = '\0';
    
    if (ssid) {
        *ssid = (addr->ssid >> 1) & 0x0F;  // SSID is in bits 4-1
    }
    
    if (command) {
        *command = (addr->ssid & 0x80) != 0;  // Bit 7 is H/C/R bit
    }
    
    return 0;
}

// Check if two addresses are equal
int ax25_address_equal(const ax25_address_t* addr1, const ax25_address_t* addr2) {
    if (!addr1 || !addr2) {
        return 0;
    }
    
    return (memcmp(addr1->callsign, addr2->callsign, 6) == 0) &&
           (((addr1->ssid >> 1) & 0x0F) == ((addr2->ssid >> 1) & 0x0F));
}

// Create AX.25 frame
int ax25_create_frame(ax25_frame_t* frame, const ax25_address_t* src, const ax25_address_t* dst, 
                     uint8_t control, uint8_t pid, const uint8_t* info, uint16_t info_len) {
    if (!frame || !src || !dst) {
        return -1;
    }
    
    // Clear frame
    memset(frame, 0, sizeof(ax25_frame_t));
    
    // Set addresses
    frame->addresses[0] = *dst;
    frame->addresses[0].command = true;  // Destination is always command
    frame->addresses[1] = *src;
    frame->addresses[1].command = false; // Source is always response
    frame->num_addresses = 2;
    
    // Set control and PID
    frame->control = control;
    frame->pid = pid;
    
    // Set information field
    if (info && info_len > 0) {
        if (info_len > AX25_MAX_INFO) {
            info_len = AX25_MAX_INFO;
        }
        memcpy(frame->info, info, info_len);
        frame->info_length = info_len;
    }
    
    frame->valid = true;
    return 0;
}

// Parse AX.25 frame
int ax25_parse_frame(const uint8_t* data, uint16_t length, ax25_frame_t* frame) {
    if (!data || !frame || length < 14) { // Minimum frame size
        return -1;
    }
    
    // Clear frame
    memset(frame, 0, sizeof(ax25_frame_t));
    
    uint16_t pos = 0;
    
    // Parse addresses
    while (pos < length - 2) { // Leave room for control field
        if (pos + 7 > length) break;
        
        // Check if this is the last address (bit 0 of SSID byte is set)
        // AX.25 SSID byte format (Dire Wolf authoritative):
        // Bit 7: H bit, Bit 6-5: Reserved (11), Bits 4-1: SSID, Bit 0: Last address flag
        // Bit 0 (LSB) is the address extension bit: 1 = last address, 0 = more addresses
        bool last_addr = (data[pos + 6] & 0x01) != 0;
        
        // Copy address
        memcpy(frame->addresses[frame->num_addresses].callsign, &data[pos], 6);
        // Extract SSID from bits 4-1 of SSID byte
        frame->addresses[frame->num_addresses].ssid = (data[pos + 6] >> 1) & 0x0F;
        frame->addresses[frame->num_addresses].command = (data[pos + 6] & 0x80) != 0;  // Bit 7 is H/C/R bit
        // has_been_repeated indicates if this address was repeated by a digipeater
        // This is determined by checking if the H bit is set (bit 7)
        frame->addresses[frame->num_addresses].has_been_repeated = (data[pos + 6] & 0x80) != 0;
        
        frame->num_addresses++;
        pos += 7;
        
        if (last_addr) break;
    }
    
    if (frame->num_addresses < 2) {
        return -1; // Need at least source and destination
    }
    
    // Parse control field
    if (pos >= length) {
        return -1;
    }
    frame->control = data[pos++];
    
    // Parse PID field (if present)
    if ((frame->control & 0x01) == 0) { // I or S frame
        if (pos >= length) {
            return -1;
        }
        frame->pid = data[pos++];
    } else {
        frame->pid = AX25_PID_NONE;
    }
    
    // Parse information field
    if ((frame->control & 0x01) == 0) { // I or UI frame
        frame->info_length = length - pos - 2; // Subtract FCS
        if (frame->info_length > AX25_MAX_INFO) {
            frame->info_length = AX25_MAX_INFO;
        }
        if (frame->info_length > 0) {
            memcpy(frame->info, &data[pos], frame->info_length);
        }
        pos += frame->info_length;
    }
    
    // Parse FCS
    if (pos + 2 > length) {
        return -1;
    }
    frame->fcs = (data[pos + 1] << 8) | data[pos];
    
    // Validate frame
    if (ax25_validate_frame(frame) != 0) {
        return -1;
    }
    
    return 0;
}

// Encode AX.25 frame
int ax25_encode_frame(const ax25_frame_t* frame, uint8_t* data, uint16_t* length) {
    if (!frame || !data || !length) {
        return -1;
    }
    
    uint16_t pos = 0;
    uint16_t max_len = *length;
    
    // Add addresses
    for (int i = 0; i < frame->num_addresses; i++) {
        if (pos + 7 > max_len) {
            return -1;
        }
        
        memcpy(&data[pos], frame->addresses[i].callsign, 6);
        data[pos + 6] = frame->addresses[i].ssid;
        
        // Set last address bit (bit 0 of SSID byte)
        if (i == frame->num_addresses - 1) {
            data[pos + 6] |= 0x01;  // Bit 0 (LSB) is the extension bit
        }
        
        pos += 7;
    }
    
    // Add control field
    if (pos >= max_len) {
        return -1;
    }
    data[pos++] = frame->control;
    
    // Add PID field (if needed)
    if ((frame->control & 0x01) == 0) { // I or S frame
        if (pos >= max_len) {
            return -1;
        }
        data[pos++] = frame->pid;
    }
    
    // Add information field
    if (frame->info_length > 0) {
        if (pos + frame->info_length > max_len) {
            return -1;
        }
        memcpy(&data[pos], frame->info, frame->info_length);
        pos += frame->info_length;
    }
    
    // Calculate and add FCS
    uint16_t fcs = ax25_calculate_fcs(data, pos);
    if (pos + 2 > max_len) {
        return -1;
    }
    data[pos++] = fcs & 0xFF;
    data[pos++] = (fcs >> 8) & 0xFF;
    
    *length = pos;
    return 0;
}

// Validate AX.25 frame
int ax25_validate_frame(const ax25_frame_t* frame) {
    if (!frame) {
        return -1;
    }
    
    if (frame->num_addresses < 2) {
        return -1; // Need at least source and destination
    }
    
    if (frame->num_addresses > AX25_MAX_ADDRS) {
        return -1; // Too many addresses
    }
    
    if (frame->info_length > AX25_MAX_INFO) {
        return -1; // Information field too long
    }
    
    return 0;
}

// Connect to remote station
int ax25_connect(ax25_tnc_t* tnc, const ax25_address_t* remote_addr) {
    if (!tnc || !remote_addr) {
        return -1;
    }
    
    // Find available connection slot
    int slot = -1;
    for (int i = 0; i < 16; i++) {
        if (tnc->connections[i].state == AX25_STATE_DISCONNECTED) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        return -1; // No available connections
    }
    
    // Initialize connection
    tnc->connections[slot].local_addr = tnc->config.my_address;
    tnc->connections[slot].remote_addr = *remote_addr;
    tnc->connections[slot].state = AX25_STATE_CONNECTING;
    tnc->connections[slot].send_seq = 0;
    tnc->connections[slot].recv_seq = 0;
    tnc->connections[slot].window_size = tnc->config.window_size;
    tnc->connections[slot].timeout = tnc->config.t1_timeout;
    tnc->connections[slot].retry_count = 0;
    
    if (slot >= tnc->num_connections) {
        tnc->num_connections = slot + 1;
    }
    
    return slot;
}

// Disconnect from remote station
int ax25_disconnect(ax25_tnc_t* tnc, const ax25_address_t* remote_addr) {
    if (!tnc || !remote_addr) {
        return -1;
    }
    
    // Find connection
    for (int i = 0; i < tnc->num_connections; i++) {
        if (ax25_address_equal(&tnc->connections[i].remote_addr, remote_addr)) {
            tnc->connections[i].state = AX25_STATE_DISCONNECTED;
            return 0;
        }
    }
    
    return -1; // Connection not found
}

// Send data to remote station
int ax25_send_data(ax25_tnc_t* tnc, const ax25_address_t* remote_addr, 
                   const uint8_t* data, uint16_t length) {
    if (!tnc || !remote_addr || !data || length == 0) {
        return -1;
    }
    
    // Find connection
    int conn = -1;
    for (int i = 0; i < tnc->num_connections; i++) {
        if (ax25_address_equal(&tnc->connections[i].remote_addr, remote_addr)) {
            conn = i;
            break;
        }
    }
    
    if (conn == -1 || tnc->connections[conn].state != AX25_STATE_CONNECTED) {
        return -1; // No active connection
    }
    
    // Create I frame
    ax25_frame_t frame;
    if (ax25_create_frame(&frame, &tnc->connections[conn].local_addr, 
                         &tnc->connections[conn].remote_addr,
                         AX25_CTRL_I | (tnc->connections[conn].send_seq << 1),
                         AX25_PID_IP, data, length) != 0) {
        return -1;
    }
    
    // Encode and send frame
    uint8_t encoded[512];
    uint16_t encoded_len = sizeof(encoded);
    if (ax25_encode_frame(&frame, encoded, &encoded_len) != 0) {
        return -1;
    }
    
    // Send via KISS interface
    // Note: This requires the TNC to have a KISS interface
    // For now, we'll simulate the KISS send
    // In a real implementation, you would call:
    // kiss_send_frame(&tnc->kiss_tnc, encoded, encoded_len, 0);
    
    tnc->connections[conn].send_seq = (tnc->connections[conn].send_seq + 1) % 8;
    
    return 0;
}

// Receive data from remote station
int ax25_receive_data(ax25_tnc_t* tnc, ax25_address_t* remote_addr, 
                      uint8_t* data, uint16_t* length) {
    if (!tnc || !remote_addr || !data || !length) {
        return -1;
    }
    
    if (!tnc->frame_ready) {
        return 0; // No frame ready
    }
    
    // Check if this is an I frame
    if ((tnc->rx_frame.control & 0x01) != 0) {
        return 0; // Not an I frame
    }
    
    // Copy data
    if (tnc->rx_frame.info_length > *length) {
        return -1; // Buffer too small
    }
    
    memcpy(data, tnc->rx_frame.info, tnc->rx_frame.info_length);
    *length = tnc->rx_frame.info_length;
    
    // Set remote address
    if (tnc->rx_frame.num_addresses >= 2) {
        *remote_addr = tnc->rx_frame.addresses[1]; // Source address
    }
    
    tnc->frame_ready = false;
    return tnc->rx_frame.info_length;
}

// Send UI frame (for APRS)
int ax25_send_ui_frame(ax25_tnc_t* tnc, const ax25_address_t* src, const ax25_address_t* dst,
                       const ax25_address_t* digipeaters, uint8_t num_digipeaters,
                       uint8_t pid, const uint8_t* info, uint16_t info_len) {
    if (!tnc || !src || !dst || !info || info_len == 0) {
        return -1;
    }
    
    // Create UI frame
    ax25_frame_t frame;
    memset(&frame, 0, sizeof(ax25_frame_t));
    
    // Set addresses
    frame.addresses[0] = *dst;
    frame.addresses[0].command = true;
    frame.addresses[1] = *src;
    frame.addresses[1].command = false;
    
    // Add digipeaters
    for (int i = 0; i < num_digipeaters && i < AX25_MAX_ADDRS - 2; i++) {
        frame.addresses[2 + i] = digipeaters[i];
        frame.addresses[2 + i].command = false;
        frame.addresses[2 + i].has_been_repeated = true;
    }
    
    frame.num_addresses = 2 + num_digipeaters;
    
    // Set control and PID
    frame.control = AX25_CTRL_UI;
    frame.pid = pid;
    
    // Set information field
    if (info_len > AX25_MAX_INFO) {
        info_len = AX25_MAX_INFO;
    }
    memcpy(frame.info, info, info_len);
    frame.info_length = info_len;
    
    frame.valid = true;
    
    // Encode and send frame
    uint8_t encoded[512];
    uint16_t encoded_len = sizeof(encoded);
    if (ax25_encode_frame(&frame, encoded, &encoded_len) != 0) {
        return -1;
    }
    
    // Send via KISS interface
    // Note: This requires the TNC to have a KISS interface
    // For now, we'll simulate the KISS send
    // In a real implementation, you would call:
    // kiss_send_frame(&tnc->kiss_tnc, encoded, encoded_len, 0);
    
    return 0;
}

// Receive UI frame (for APRS)
int ax25_receive_ui_frame(ax25_tnc_t* tnc, ax25_address_t* src, ax25_address_t* dst,
                          ax25_address_t* digipeaters, uint8_t* num_digipeaters,
                          uint8_t* pid, uint8_t* info, uint16_t* info_len) {
    if (!tnc || !src || !dst || !num_digipeaters || !pid || !info || !info_len) {
        return -1;
    }
    
    if (!tnc->frame_ready) {
        return 0; // No frame ready
    }
    
    // Check if this is a UI frame
    if (tnc->rx_frame.control != AX25_CTRL_UI) {
        return 0; // Not a UI frame
    }
    
    // Set addresses
    if (tnc->rx_frame.num_addresses >= 2) {
        *dst = tnc->rx_frame.addresses[0]; // Destination
        *src = tnc->rx_frame.addresses[1]; // Source
    }
    
    // Set digipeaters
    *num_digipeaters = 0;
    if (tnc->rx_frame.num_addresses > 2) {
        uint8_t max_digipeaters = tnc->rx_frame.num_addresses - 2;
        if (max_digipeaters > *num_digipeaters) {
            max_digipeaters = *num_digipeaters;
        }
        
        for (int i = 0; i < max_digipeaters; i++) {
            digipeaters[i] = tnc->rx_frame.addresses[2 + i];
        }
        *num_digipeaters = max_digipeaters;
    }
    
    // Set PID and information
    *pid = tnc->rx_frame.pid;
    
    if (tnc->rx_frame.info_length > *info_len) {
        return -1; // Buffer too small
    }
    
    memcpy(info, tnc->rx_frame.info, tnc->rx_frame.info_length);
    *info_len = tnc->rx_frame.info_length;
    
    tnc->frame_ready = false;
    return tnc->rx_frame.info_length;
}

// Calculate FCS (Frame Check Sequence)
uint16_t ax25_calculate_fcs(const uint8_t* data, uint16_t length) {
    uint16_t fcs = 0xFFFF;
    
    for (uint16_t i = 0; i < length; i++) {
        fcs ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (fcs & 0x0001) {
                fcs = (fcs >> 1) ^ 0x8408;
            } else {
                fcs = fcs >> 1;
            }
        }
    }
    
    return fcs ^ 0xFFFF;
}

// Check FCS
bool ax25_check_fcs(const uint8_t* data, uint16_t length, uint16_t fcs) {
    return ax25_calculate_fcs(data, length) == fcs;
}

// Bit stuffing implementation
int ax25_bit_stuff(const uint8_t* input, uint16_t input_len, uint8_t* output, uint16_t* output_len) {
    if (!input || !output || !output_len) {
        return -1;
    }
    
    uint16_t out_pos = 0;
    uint8_t bit_count = 0;
    uint8_t current_byte = 0;
    
    for (uint16_t i = 0; i < input_len; i++) {
        for (int bit = 7; bit >= 0; bit--) {
            uint8_t bit_value = (input[i] >> bit) & 1;
            current_byte = (current_byte << 1) | bit_value;
            bit_count++;
            
            if (bit_count == 8) {
                if (out_pos >= *output_len) {
                    return -1; // Output buffer too small
                }
                output[out_pos++] = current_byte;
                current_byte = 0;
                bit_count = 0;
            }
            
            // Check for 5 consecutive 1s and stuff a 0
            if (bit_count == 5 && (current_byte & 0x1F) == 0x1F) {
                current_byte = (current_byte << 1) | 0; // Stuff 0
                bit_count++;
                
                if (bit_count == 8) {
                    if (out_pos >= *output_len) {
                        return -1; // Output buffer too small
                    }
                    output[out_pos++] = current_byte;
                    current_byte = 0;
                    bit_count = 0;
                }
            }
        }
    }
    
    // Handle remaining bits
    if (bit_count > 0) {
        current_byte <<= (8 - bit_count);
        if (out_pos >= *output_len) {
            return -1; // Output buffer too small
        }
        output[out_pos++] = current_byte;
    }
    
    *output_len = out_pos;
    return 0;
}

// Bit unstuffing implementation
int ax25_bit_unstuff(const uint8_t* input, uint16_t input_len, uint8_t* output, uint16_t* output_len) {
    if (!input || !output || !output_len) {
        return -1;
    }
    
    uint16_t out_pos = 0;
    uint8_t bit_count = 0;
    uint8_t current_byte = 0;
    uint8_t consecutive_ones = 0;
    
    for (uint16_t i = 0; i < input_len; i++) {
        for (int bit = 7; bit >= 0; bit--) {
            uint8_t bit_value = (input[i] >> bit) & 1;
            
            if (bit_value == 1) {
                consecutive_ones++;
                current_byte = (current_byte << 1) | 1;
                bit_count++;
                
                // Check for stuffed bit (5 ones followed by 0)
                if (consecutive_ones == 5) {
                    // Skip the stuffed 0 bit
                    continue;
                }
            } else {
                consecutive_ones = 0;
                current_byte = (current_byte << 1) | 0;
                bit_count++;
            }
            
            if (bit_count == 8) {
                if (out_pos >= *output_len) {
                    return -1; // Output buffer too small
                }
                output[out_pos++] = current_byte;
                current_byte = 0;
                bit_count = 0;
            }
        }
    }
    
    // Handle remaining bits
    if (bit_count > 0) {
        current_byte <<= (8 - bit_count);
        if (out_pos >= *output_len) {
            return -1; // Output buffer too small
        }
        output[out_pos++] = current_byte;
    }
    
    *output_len = out_pos;
    return 0;
}

// Add frame flags implementation
int ax25_add_flags(uint8_t* data, uint16_t* length, uint16_t max_length) {
    if (!data || !length) {
        return -1;
    }
    
    // Check if we have enough space for flags
    if (*length + 2 > max_length) {
        return -1; // Not enough space
    }
    
    // Shift data to make room for opening flag
    for (uint16_t i = *length; i > 0; i--) {
        data[i] = data[i - 1];
    }
    
    // Add opening flag
    data[0] = AX25_FLAG;
    
    // Add closing flag
    data[*length + 1] = AX25_FLAG;
    
    *length += 2;
    return 0;
}
