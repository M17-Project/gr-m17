//--------------------------------------------------------------------
// KISS Protocol Implementation for M17
//
// KISS (Keep It Simple Stupid) protocol implementation
// Compatible with Dire Wolf and traditional TNC software
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

// KISS Protocol Constants
#define KISS_FEND    0xC0    // Frame End
#define KISS_FESC    0xDB    // Frame Escape
#define KISS_TFEND   0xDC    // Transposed Frame End
#define KISS_TFESC   0xDD    // Transposed Frame Escape

// KISS Command Types
#define KISS_CMD_DATA    0x00    // Data frame
#define KISS_CMD_TXDELAY 0x01    // TX Delay
#define KISS_CMD_P       0x02    // Persistence
#define KISS_CMD_SLOTTIME 0x03   // Slot Time
#define KISS_CMD_TXTAIL  0x04    // TX Tail
#define KISS_CMD_FULLDUP 0x05    // Full Duplex
#define KISS_CMD_SETHARD 0x06    // Set Hardware
#define KISS_CMD_RETURN  0xFF    // Return

// KISS Frame Structure
typedef struct {
    uint8_t* data;          // Frame data
    uint16_t length;        // Frame length
    uint8_t command;        // KISS command
    uint8_t port;           // Port number
    bool escaped;           // Escape sequence flag
} kiss_frame_t;

// KISS TNC State
typedef enum {
    KISS_STATE_IDLE,
    KISS_STATE_FEND,
    KISS_STATE_DATA,
    KISS_STATE_ESCAPE
} kiss_state_t;

// KISS TNC Configuration
typedef struct {
    uint16_t tx_delay;      // TX Delay (10ms units)
    uint8_t persistence;    // Persistence factor
    uint16_t slot_time;     // Slot time (10ms units)
    uint8_t tx_tail;        // TX Tail (10ms units)
    bool full_duplex;       // Full duplex mode
    uint8_t hardware_id;    // Hardware ID
} kiss_config_t;

// KISS TNC Interface
typedef struct {
    kiss_state_t state;
    kiss_config_t config;
    uint8_t buffer[1024];   // Receive buffer
    uint16_t buffer_pos;
    kiss_frame_t current_frame;
    bool frame_ready;
    int serial_fd;          // File descriptor for USB CDC serial
    int tcp_socket;         // Socket descriptor for TCP interface
} kiss_tnc_t;

// KISS Protocol Functions
int kiss_init(kiss_tnc_t* tnc);
int kiss_cleanup(kiss_tnc_t* tnc);
int kiss_set_config(kiss_tnc_t* tnc, const kiss_config_t* config);
int kiss_get_config(const kiss_tnc_t* tnc, kiss_config_t* config);

// Frame Processing
int kiss_send_frame(kiss_tnc_t* tnc, const uint8_t* data, uint16_t length, uint8_t port);
int kiss_receive_frame(kiss_tnc_t* tnc, uint8_t* data, uint16_t* length, uint8_t* port);
int kiss_process_byte(kiss_tnc_t* tnc, uint8_t byte);
int kiss_frame_ready(const kiss_tnc_t* tnc);

// USB Serial Interface (Primary interface for current hardware)
int kiss_serial_send(kiss_tnc_t* tnc, const uint8_t* data, uint16_t length);
int kiss_serial_receive(kiss_tnc_t* tnc, uint8_t* data, uint16_t* length);

// TCP/IP Interface
int kiss_tcp_send(kiss_tnc_t* tnc, const uint8_t* data, uint16_t length);
int kiss_tcp_receive(kiss_tnc_t* tnc, uint8_t* data, uint16_t* length);

// Bluetooth Interface (FUTURE FEATURE - not available on current hardware)
int kiss_bt_send(kiss_tnc_t* tnc, const uint8_t* data, uint16_t length);
int kiss_bt_receive(kiss_tnc_t* tnc, uint8_t* data, uint16_t* length);

// Utility Functions
int kiss_escape_data(const uint8_t* input, uint16_t input_len, uint8_t* output, uint16_t* output_len);
int kiss_unescape_data(const uint8_t* input, uint16_t input_len, uint8_t* output, uint16_t* output_len);
int kiss_validate_frame(const kiss_frame_t* frame);

#ifdef __cplusplus
}
#endif
