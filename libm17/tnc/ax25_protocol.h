//--------------------------------------------------------------------
// AX.25 Protocol Implementation for M17
//
// AX.25 (Amateur X.25) protocol implementation
// Compatible with traditional packet radio and APRS
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

// AX.25 Protocol Constants
#define AX25_FLAG        0x7E    // Frame flag
#define AX25_ADDR_LEN    7       // Address length
#define AX25_MAX_ADDRS   9       // Maximum addresses
#define AX25_MAX_INFO    256     // Maximum information field length

// AX.25 Frame Types
#define AX25_FRAME_I     0x00    // Information frame
#define AX25_FRAME_S     0x01    // Supervisory frame
#define AX25_FRAME_U     0x03    // Unnumbered frame

// AX.25 Control Field Types
#define AX25_CTRL_I      0x00    // Information
#define AX25_CTRL_RR     0x01    // Receive Ready
#define AX25_CTRL_RNR    0x05    // Receive Not Ready
#define AX25_CTRL_REJ    0x09    // Reject
#define AX25_CTRL_SABM   0x2F    // Set Asynchronous Balanced Mode
#define AX25_CTRL_SABME  0x6F    // Set Asynchronous Balanced Mode Extended
#define AX25_CTRL_DISC   0x43    // Disconnect
#define AX25_CTRL_DM     0x0F    // Disconnected Mode
#define AX25_CTRL_UA     0x63    // Unnumbered Acknowledge
#define AX25_CTRL_FRMR   0x87    // Frame Reject
#define AX25_CTRL_UI     0x03    // Unnumbered Information

// AX.25 PID Types
#define AX25_PID_NONE    0xF0    // No layer 3 protocol
#define AX25_PID_IP      0xCC    // Internet Protocol
#define AX25_PID_ARP     0xCD    // Address Resolution Protocol
#define AX25_PID_NETROM  0xCF    // NET/ROM
#define AX25_PID_NO_L3   0xF0    // No layer 3

// AX.25 Address Structure
typedef struct {
    uint8_t callsign[6];     // 6-byte callsign
    uint8_t ssid;            // SSID (4 bits) + C/R, H bits
    bool command;            // Command/Response bit
    bool has_been_repeated;  // Has Been Repeated bit
} ax25_address_t;

// AX.25 Frame Structure
typedef struct {
    ax25_address_t addresses[AX25_MAX_ADDRS];  // Address fields
    uint8_t num_addresses;                     // Number of addresses
    uint8_t control;                           // Control field
    uint8_t pid;                               // Protocol ID
    uint8_t info[AX25_MAX_INFO];               // Information field
    uint16_t info_length;                      // Information length
    uint16_t fcs;                              // Frame Check Sequence
    bool valid;                                // Frame validity
} ax25_frame_t;

// AX.25 Connection State
typedef enum {
    AX25_STATE_DISCONNECTED,
    AX25_STATE_CONNECTING,
    AX25_STATE_CONNECTED,
    AX25_STATE_DISCONNECTING
} ax25_state_t;

// AX.25 Connection
typedef struct {
    ax25_address_t local_addr;
    ax25_address_t remote_addr;
    ax25_state_t state;
    uint8_t send_seq;        // Send sequence number
    uint8_t recv_seq;        // Receive sequence number
    uint8_t window_size;     // Window size
    uint32_t timeout;        // Connection timeout
    uint32_t retry_count;    // Retry counter
} ax25_connection_t;

// AX.25 TNC Configuration
typedef struct {
    ax25_address_t my_address;       // My callsign
    uint16_t tx_delay;               // TX delay (10ms units)
    uint8_t persistence;             // Persistence factor
    uint16_t slot_time;              // Slot time (10ms units)
    uint8_t tx_tail;                 // TX tail (10ms units)
    bool full_duplex;                // Full duplex mode
    uint8_t max_frame_length;        // Maximum frame length
    uint8_t window_size;             // Window size
    uint32_t t1_timeout;             // T1 timeout (ms)
    uint32_t t2_timeout;             // T2 timeout (ms)
    uint32_t t3_timeout;             // T3 timeout (ms)
    uint8_t max_retries;             // Maximum retries
} ax25_config_t;

// AX.25 TNC Interface
typedef struct {
    ax25_config_t config;
    ax25_connection_t connections[16];  // Multiple connections
    uint8_t num_connections;
    ax25_frame_t rx_frame;
    ax25_frame_t tx_frame;
    bool frame_ready;
} ax25_tnc_t;

// AX.25 Protocol Functions
int ax25_init(ax25_tnc_t* tnc);
int ax25_cleanup(ax25_tnc_t* tnc);
int ax25_set_config(ax25_tnc_t* tnc, const ax25_config_t* config);
int ax25_get_config(const ax25_tnc_t* tnc, ax25_config_t* config);

// Address Functions
int ax25_set_address(ax25_address_t* addr, const char* callsign, uint8_t ssid, bool command);
int ax25_get_address(const ax25_address_t* addr, char* callsign, uint8_t* ssid, bool* command);
int ax25_address_equal(const ax25_address_t* addr1, const ax25_address_t* addr2);

// Frame Functions
int ax25_create_frame(ax25_frame_t* frame, const ax25_address_t* src, const ax25_address_t* dst, 
                     uint8_t control, uint8_t pid, const uint8_t* info, uint16_t info_len);
int ax25_parse_frame(const uint8_t* data, uint16_t length, ax25_frame_t* frame);
int ax25_encode_frame(const ax25_frame_t* frame, uint8_t* data, uint16_t* length);
int ax25_validate_frame(const ax25_frame_t* frame);

// Connection Functions
int ax25_connect(ax25_tnc_t* tnc, const ax25_address_t* remote_addr);
int ax25_disconnect(ax25_tnc_t* tnc, const ax25_address_t* remote_addr);
int ax25_send_data(ax25_tnc_t* tnc, const ax25_address_t* remote_addr, 
                   const uint8_t* data, uint16_t length);
int ax25_receive_data(ax25_tnc_t* tnc, ax25_address_t* remote_addr, 
                      uint8_t* data, uint16_t* length);

// UI Frame Functions (for APRS)
int ax25_send_ui_frame(ax25_tnc_t* tnc, const ax25_address_t* src, const ax25_address_t* dst,
                       const ax25_address_t* digipeaters, uint8_t num_digipeaters,
                       uint8_t pid, const uint8_t* info, uint16_t info_len);
int ax25_receive_ui_frame(ax25_tnc_t* tnc, ax25_address_t* src, ax25_address_t* dst,
                          ax25_address_t* digipeaters, uint8_t* num_digipeaters,
                          uint8_t* pid, uint8_t* info, uint16_t* info_len);

// FCS Functions
uint16_t ax25_calculate_fcs(const uint8_t* data, uint16_t length);
bool ax25_check_fcs(const uint8_t* data, uint16_t length, uint16_t fcs);

// Utility Functions
int ax25_bit_stuff(const uint8_t* input, uint16_t input_len, uint8_t* output, uint16_t* output_len);
int ax25_bit_unstuff(const uint8_t* input, uint16_t input_len, uint8_t* output, uint16_t* output_len);
int ax25_add_flags(uint8_t* data, uint16_t* length, uint16_t max_length);

#ifdef __cplusplus
}
#endif
