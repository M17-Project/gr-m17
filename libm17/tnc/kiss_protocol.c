//--------------------------------------------------------------------
// KISS Protocol Implementation for M17
//
// KISS (Keep It Simple Stupid) protocol implementation
// Compatible with Dire Wolf and traditional TNC software
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------
#include "kiss_protocol.h"
#include <string.h>
#include <stdlib.h>

// Initialize KISS TNC
int kiss_init(kiss_tnc_t* tnc) {
    if (!tnc) {
        return -1;
    }
    
    // Initialize state
    tnc->state = KISS_STATE_IDLE;
    tnc->buffer_pos = 0;
    tnc->frame_ready = false;
    
    // Set default configuration
    tnc->config.tx_delay = 50;      // 500ms
    tnc->config.persistence = 63;   // 63/256
    tnc->config.slot_time = 10;     // 100ms
    tnc->config.tx_tail = 5;        // 50ms
    tnc->config.full_duplex = false;
    tnc->config.hardware_id = 0;
    
    // Initialize current frame
    tnc->current_frame.data = NULL;
    tnc->current_frame.length = 0;
    tnc->current_frame.command = 0;
    tnc->current_frame.port = 0;
    tnc->current_frame.escaped = false;
    
    return 0;
}

// Cleanup KISS TNC
int kiss_cleanup(kiss_tnc_t* tnc) {
    if (!tnc) {
        return -1;
    }
    
    // Free allocated memory
    if (tnc->current_frame.data) {
        free(tnc->current_frame.data);
        tnc->current_frame.data = NULL;
    }
    
    // Reset state
    tnc->state = KISS_STATE_IDLE;
    tnc->buffer_pos = 0;
    tnc->frame_ready = false;
    
    return 0;
}

// Set KISS configuration
int kiss_set_config(kiss_tnc_t* tnc, const kiss_config_t* config) {
    if (!tnc || !config) {
        return -1;
    }
    
    tnc->config = *config;
    return 0;
}

// Get KISS configuration
int kiss_get_config(const kiss_tnc_t* tnc, kiss_config_t* config) {
    if (!tnc || !config) {
        return -1;
    }
    
    *config = tnc->config;
    return 0;
}

// Send KISS frame
int kiss_send_frame(kiss_tnc_t* tnc, const uint8_t* data, uint16_t length, uint8_t port) {
    if (!tnc || !data || length == 0) {
        return -1;
    }
    
    // Allocate frame buffer
    uint8_t* frame = malloc(length + 3); // FEND + command + data + FEND
    if (!frame) {
        return -1;
    }
    
    uint16_t pos = 0;
    
    // Start frame
    frame[pos++] = KISS_FEND;
    
    // Command byte (port + command)
    frame[pos++] = (port << 4) | KISS_CMD_DATA;
    
    // Escape and add data
    uint16_t escaped_len;
    if (kiss_escape_data(data, length, &frame[pos], &escaped_len) != 0) {
        free(frame);
        return -1;
    }
    pos += escaped_len;
    
    // End frame
    frame[pos++] = KISS_FEND;
    
    // Send frame (implementation depends on interface)
    int result = kiss_serial_send(tnc, frame, pos);
    
    free(frame);
    return result;
}

// Receive KISS frame
int kiss_receive_frame(kiss_tnc_t* tnc, uint8_t* data, uint16_t* length, uint8_t* port) {
    if (!tnc || !data || !length) {
        return -1;
    }
    
    if (!tnc->frame_ready) {
        return 0; // No frame ready
    }
    
    // Copy frame data
    if (tnc->current_frame.length > *length) {
        return -1; // Buffer too small
    }
    
    memcpy(data, tnc->current_frame.data, tnc->current_frame.length);
    *length = tnc->current_frame.length;
    
    if (port) {
        *port = tnc->current_frame.port;
    }
    
    // Reset frame ready flag
    tnc->frame_ready = false;
    
    return tnc->current_frame.length;
}

// Process incoming byte
int kiss_process_byte(kiss_tnc_t* tnc, uint8_t byte) {
    if (!tnc) {
        return -1;
    }
    
    switch (tnc->state) {
        case KISS_STATE_IDLE:
            if (byte == KISS_FEND) {
                tnc->state = KISS_STATE_FEND;
                tnc->buffer_pos = 0;
            }
            break;
            
        case KISS_STATE_FEND:
            if (byte == KISS_FEND) {
                // Empty frame, ignore
                tnc->state = KISS_STATE_IDLE;
            } else {
                // Command byte
                tnc->current_frame.port = (byte >> 4) & 0x0F;
                tnc->current_frame.command = byte & 0x0F;
                tnc->state = KISS_STATE_DATA;
            }
            break;
            
        case KISS_STATE_DATA:
            if (byte == KISS_FEND) {
                // End of frame
                if (tnc->buffer_pos > 0) {
                    // Unescape data
                    uint8_t* unescaped = malloc(tnc->buffer_pos);
                    if (!unescaped) {
                        tnc->state = KISS_STATE_IDLE;
                        return -1;
                    }
                    
                    uint16_t unescaped_len;
                    if (kiss_unescape_data(tnc->buffer, tnc->buffer_pos, unescaped, &unescaped_len) == 0) {
                        // Free old data
                        if (tnc->current_frame.data) {
                            free(tnc->current_frame.data);
                        }
                        
                        // Set new frame data
                        tnc->current_frame.data = unescaped;
                        tnc->current_frame.length = unescaped_len;
                        tnc->frame_ready = true;
                    } else {
                        free(unescaped);
                    }
                }
                tnc->state = KISS_STATE_IDLE;
            } else if (byte == KISS_FESC) {
                tnc->state = KISS_STATE_ESCAPE;
            } else {
                // Regular data byte
                if (tnc->buffer_pos < sizeof(tnc->buffer)) {
                    tnc->buffer[tnc->buffer_pos++] = byte;
                }
            }
            break;
            
        case KISS_STATE_ESCAPE:
            if (byte == KISS_TFEND) {
                tnc->buffer[tnc->buffer_pos++] = KISS_FEND;
            } else if (byte == KISS_TFESC) {
                tnc->buffer[tnc->buffer_pos++] = KISS_FESC;
            } else {
                // Invalid escape sequence
                tnc->state = KISS_STATE_IDLE;
                return -1;
            }
            tnc->state = KISS_STATE_DATA;
            break;
    }
    
    return 0;
}

// Check if frame is ready
int kiss_frame_ready(const kiss_tnc_t* tnc) {
    if (!tnc) {
        return 0;
    }
    
    return tnc->frame_ready ? 1 : 0;
}

// Escape data for KISS transmission
int kiss_escape_data(const uint8_t* input, uint16_t input_len, uint8_t* output, uint16_t* output_len) {
    if (!input || !output || !output_len) {
        return -1;
    }
    
    uint16_t out_pos = 0;
    
    for (uint16_t i = 0; i < input_len; i++) {
        if (out_pos >= *output_len) {
            return -1; // Output buffer too small
        }
        
        if (input[i] == KISS_FEND) {
            output[out_pos++] = KISS_FESC;
            output[out_pos++] = KISS_TFEND;
        } else if (input[i] == KISS_FESC) {
            output[out_pos++] = KISS_FESC;
            output[out_pos++] = KISS_TFESC;
        } else {
            output[out_pos++] = input[i];
        }
    }
    
    *output_len = out_pos;
    return 0;
}

// Unescape data from KISS transmission
int kiss_unescape_data(const uint8_t* input, uint16_t input_len, uint8_t* output, uint16_t* output_len) {
    if (!input || !output || !output_len) {
        return -1;
    }
    
    uint16_t out_pos = 0;
    bool escaped = false;
    
    for (uint16_t i = 0; i < input_len; i++) {
        if (out_pos >= *output_len) {
            return -1; // Output buffer too small
        }
        
        if (escaped) {
            if (input[i] == KISS_TFEND) {
                output[out_pos++] = KISS_FEND;
            } else if (input[i] == KISS_TFESC) {
                output[out_pos++] = KISS_FESC;
            } else {
                return -1; // Invalid escape sequence
            }
            escaped = false;
        } else if (input[i] == KISS_FESC) {
            escaped = true;
        } else {
            output[out_pos++] = input[i];
        }
    }
    
    if (escaped) {
        return -1; // Incomplete escape sequence
    }
    
    *output_len = out_pos;
    return 0;
}

// Validate KISS frame
int kiss_validate_frame(const kiss_frame_t* frame) {
    if (!frame) {
        return -1;
    }
    
    if (frame->command > 0x0F) {
        return -1; // Invalid command
    }
    
    if (frame->port > 0x0F) {
        return -1; // Invalid port
    }
    
    if (frame->length > 0 && !frame->data) {
        return -1; // No data for non-empty frame
    }
    
    return 0;
}

// USB Serial interface implementation
int kiss_serial_send(kiss_tnc_t* tnc, const uint8_t* data, uint16_t length) {
    if (!tnc || !data || length == 0) {
        return -1;
    }
    
    // Write to USB CDC (USB Serial) port
    // This is a placeholder for actual USB CDC communication
    // In a real implementation, this would use:
    // - USB CDC driver for MCM-iMX93
    // - USB CDC ACM (Abstract Control Model) for serial emulation
    // - Direct USB bulk transfer for high-speed data
    
    // For now, simulate successful transmission
    return length;
}

int kiss_serial_receive(kiss_tnc_t* tnc, uint8_t* data, uint16_t* length) {
    if (!tnc || !data || !length) {
        return -1;
    }
    
    // Read from USB CDC (USB Serial) port
    // This is a placeholder for actual USB CDC communication
    // In a real implementation, this would use:
    // - USB CDC driver for MCM-iMX93
    // - USB CDC ACM (Abstract Control Model) for serial emulation
    // - Direct USB bulk transfer for high-speed data
    
    // For now, simulate no data available
    *length = 0;
    return 0;
}

// TCP/IP interface implementation
int kiss_tcp_send(kiss_tnc_t* tnc, const uint8_t* data, uint16_t length) {
    if (!tnc || !data || length == 0) {
        return -1;
    }
    
    // Send data via TCP socket (implement based on your network stack)
    // This is a placeholder for actual TCP communication
    // In a real implementation, this would use:
    // - BSD sockets API
    // - Network stack on MCM-iMX93
    // - KISS over TCP (typically port 8001)
    
    // For now, simulate successful transmission
    return length;
}

int kiss_tcp_receive(kiss_tnc_t* tnc, uint8_t* data, uint16_t* length) {
    if (!tnc || !data || !length) {
        return -1;
    }
    
    // Receive data via TCP socket (implement based on your network stack)
    // This is a placeholder for actual TCP communication
    // In a real implementation, this would use:
    // - BSD sockets API
    // - Network stack on MCM-iMX93
    // - KISS over TCP (typically port 8001)
    
    // For now, simulate no data available
    *length = 0;
    return 0;
}

// Bluetooth interface implementation (FUTURE FEATURE)
// Note: Current hardware does not have Bluetooth capability
// This interface is prepared for future Bluetooth module addition
int kiss_bt_send(kiss_tnc_t* tnc, const uint8_t* data, uint16_t length) {
    if (!tnc || !data || length == 0) {
        return -1;
    }
    
    // FUTURE: Send data via Bluetooth SPP when Bluetooth module is added
    // This would use:
    // - BlueZ stack on Linux
    // - Bluetooth SPP (Serial Port Profile)
    // - RFCOMM socket interface
    // - External Bluetooth module via USB or UART
    
    // Currently not supported - return error
    return -1; // Bluetooth not available
}

int kiss_bt_receive(kiss_tnc_t* tnc, uint8_t* data, uint16_t* length) {
    if (!tnc || !data || !length) {
        return -1;
    }
    
    // FUTURE: Receive data via Bluetooth SPP when Bluetooth module is added
    // This would use:
    // - BlueZ stack on Linux
    // - Bluetooth SPP (Serial Port Profile)
    // - RFCOMM socket interface
    // - External Bluetooth module via USB or UART
    
    // Currently not supported - return error
    *length = 0;
    return -1; // Bluetooth not available
}
