//--------------------------------------------------------------------
// M17 ↔ AX.25 Protocol Bridge
//
// Protocol bridge for dual-mode operation
// Supports both M17 and AX.25 packet radio
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../tnc/kiss_protocol.h"
#include "../tnc/ax25_protocol.h"
#include "../tnc/fx25_protocol.h"
#include "../tnc/il2p_protocol.h"

// M17 Type Definitions
typedef struct {
    uint8_t data[16];
    uint16_t length;
} m17_audio_frame_t;

typedef struct {
    uint8_t type;
    uint8_t data[256];
    uint16_t length;
} m17_packet_frame_t;

typedef struct {
    uint8_t data[256];
    uint16_t length;
} m17_frame_t;

// M17 Packet Types
#define M17_PACKET_TYPE_DATA  0
#define M17_PACKET_TYPE_APRS  1
#define M17_PACKET_TYPE_TEXT  2

#ifdef __cplusplus
extern "C" {
#endif

// Bridge Configuration
typedef struct {
    bool m17_enabled;        // Enable M17 mode
    bool ax25_enabled;       // Enable AX.25 mode
    bool fx25_enabled;       // Enable FX.25 FEC
    bool il2p_enabled;       // Enable IL2P protocol
    bool auto_detect;        // Auto-detect protocol
    uint32_t m17_frequency;  // M17 frequency
    uint32_t ax25_frequency; // AX.25 frequency
    uint8_t m17_can;         // M17 Channel Access Number
    char ax25_callsign[7];   // AX.25 callsign
    uint8_t ax25_ssid;       // AX.25 SSID
    uint8_t fx25_rs_type;    // FX.25 Reed-Solomon type
    uint8_t il2p_debug;      // IL2P debug level
} bridge_config_t;

// Protocol Detection
typedef enum {
    PROTOCOL_UNKNOWN,
    PROTOCOL_M17,
    PROTOCOL_AX25,
    PROTOCOL_FX25,
    PROTOCOL_IL2P,
    PROTOCOL_APRS
} protocol_type_t;

// Bridge State
typedef struct {
    bridge_config_t config;
    protocol_type_t current_protocol;
    bool m17_active;
    bool ax25_active;
    bool fx25_active;
    bool il2p_active;
    uint32_t last_activity;
    uint32_t protocol_timeout;
    fx25_context_t fx25_ctx;
    il2p_context_t il2p_ctx;
} bridge_state_t;

// M17 to AX.25 Conversion
typedef struct {
    char m17_callsign[10];   // M17 callsign
    char ax25_callsign[7];   // AX.25 callsign
    uint8_t ax25_ssid;       // AX.25 SSID
    bool active;             // Conversion active
} m17_ax25_mapping_t;

// Event handler function type
typedef void (*bridge_event_handler_t)(protocol_type_t protocol, const uint8_t* data, uint16_t length);

// Bridge Interface
typedef struct {
    bridge_state_t state;
    kiss_tnc_t kiss_tnc;
    ax25_tnc_t ax25_tnc;
    m17_ax25_mapping_t mappings[16];
    uint8_t num_mappings;
    bridge_event_handler_t event_handler;
    bool event_handler_registered;
    bool debug_enabled;
    int debug_level;
} m17_ax25_bridge_t;

// Bridge Core Functions
int m17_ax25_bridge_init(m17_ax25_bridge_t* bridge);
int m17_ax25_bridge_cleanup(m17_ax25_bridge_t* bridge);
int m17_ax25_bridge_set_config(m17_ax25_bridge_t* bridge, const bridge_config_t* config);
int m17_ax25_bridge_get_config(const m17_ax25_bridge_t* bridge, bridge_config_t* config);

// Protocol Detection
int m17_ax25_bridge_detect_protocol(m17_ax25_bridge_t* bridge, const uint8_t* data, uint16_t length);
protocol_type_t m17_ax25_bridge_get_current_protocol(const m17_ax25_bridge_t* bridge);
int m17_ax25_bridge_set_protocol(m17_ax25_bridge_t* bridge, protocol_type_t protocol);

// M17 to AX.25 Conversion
int m17_ax25_bridge_convert_m17_to_ax25(m17_ax25_bridge_t* bridge, const uint8_t* m17_data, uint16_t m17_length,
                                       uint8_t* ax25_data, uint16_t* ax25_length);
int m17_ax25_bridge_convert_ax25_to_m17(m17_ax25_bridge_t* bridge, const uint8_t* ax25_data, uint16_t ax25_length,
                                        uint8_t* m17_data, uint16_t* m17_length);

// M17 to AX.25 Conversion Functions
int m17_ax25_bridge_convert_m17_lsf_to_aprs(m17_ax25_bridge_t* bridge, const uint8_t* m17_data, uint16_t m17_length,
                                           uint8_t* ax25_data, uint16_t* ax25_length);
int m17_ax25_bridge_convert_m17_packet_to_ax25(m17_ax25_bridge_t* bridge, const uint8_t* m17_data, uint16_t m17_length,
                                              uint8_t* ax25_data, uint16_t* ax25_length);

// M17 Processing Functions
int m17_ax25_bridge_process_m17_tx(m17_ax25_bridge_t* bridge, const uint8_t* data, uint16_t length);
int m17_ax25_bridge_process_ax25_tx(m17_ax25_bridge_t* bridge, const uint8_t* data, uint16_t length);

// M17 Helper Functions
int m17_decode_audio_frame(const uint8_t* data, uint16_t length, m17_audio_frame_t* frame);
int m17_audio_to_pcm(const m17_audio_frame_t* frame, int16_t* pcm_samples, uint16_t sample_count);
int m17_decode_packet_frame(const uint8_t* data, uint16_t length, m17_packet_frame_t* frame);
int m17_validate_packet_data(const uint8_t* data, uint16_t length);
int m17_create_frame(const uint8_t* data, uint16_t length, m17_frame_t* frame);
int m17_encode_frame(const m17_frame_t* frame, uint8_t* data, uint16_t* length);

// Callsign Mapping
int m17_ax25_bridge_add_mapping(m17_ax25_bridge_t* bridge, const char* m17_callsign, 
                                const char* ax25_callsign, uint8_t ax25_ssid);
int m17_ax25_bridge_remove_mapping(m17_ax25_bridge_t* bridge, const char* m17_callsign);
int m17_ax25_bridge_find_mapping(const m17_ax25_bridge_t* bridge, const char* m17_callsign,
                                 char* ax25_callsign, uint8_t* ax25_ssid);

// Data Processing
int m17_ax25_bridge_process_rx_data(m17_ax25_bridge_t* bridge, const uint8_t* data, uint16_t length);
int m17_ax25_bridge_process_tx_data(m17_ax25_bridge_t* bridge, const uint8_t* data, uint16_t length, 
                                   protocol_type_t protocol);

// Frame Processing
int m17_ax25_bridge_process_m17_frame(m17_ax25_bridge_t* bridge, const uint8_t* data, uint16_t length);
int m17_ax25_bridge_process_ax25_frame(m17_ax25_bridge_t* bridge, const uint8_t* data, uint16_t length);

// M17 Frame Processing
int m17_ax25_bridge_process_m17_lsf(m17_ax25_bridge_t* bridge, const uint8_t* data, uint16_t length);
int m17_ax25_bridge_process_m17_stream(m17_ax25_bridge_t* bridge, const uint8_t* data, uint16_t length);
int m17_ax25_bridge_process_m17_packet(m17_ax25_bridge_t* bridge, const uint8_t* data, uint16_t length);
int m17_ax25_bridge_process_m17_bert(m17_ax25_bridge_t* bridge, const uint8_t* data, uint16_t length);

// AX.25 Frame Processing
int m17_ax25_bridge_parse_ax25_frame(m17_ax25_bridge_t* bridge, const uint8_t* data, uint16_t length);
int m17_ax25_bridge_process_ax25_iframe(m17_ax25_bridge_t* bridge, const uint8_t* data, uint16_t length,
                                         const char* src_callsign, const char* dst_callsign);
int m17_ax25_bridge_process_ax25_sframe(m17_ax25_bridge_t* bridge, const uint8_t* data, uint16_t length,
                                         const char* src_callsign, const char* dst_callsign);
int m17_ax25_bridge_process_ax25_uframe(m17_ax25_bridge_t* bridge, const uint8_t* data, uint16_t length,
                                         const char* src_callsign, const char* dst_callsign);
int m17_ax25_bridge_process_aprs_frame(m17_ax25_bridge_t* bridge, const uint8_t* data, uint16_t length,
                                        const char* src_callsign, const char* dst_callsign);

// FX.25 Frame Processing
int m17_ax25_bridge_process_fx25_frame(m17_ax25_bridge_t* bridge, const uint8_t* data, uint16_t length);
int m17_ax25_bridge_encode_fx25_frame(m17_ax25_bridge_t* bridge, const uint8_t* ax25_data, uint16_t ax25_length,
                                       uint8_t* fx25_data, uint16_t* fx25_length);
int m17_ax25_bridge_decode_fx25_frame(m17_ax25_bridge_t* bridge, const uint8_t* fx25_data, uint16_t fx25_length,
                                       uint8_t* ax25_data, uint16_t* ax25_length);

// IL2P Frame Processing
int m17_ax25_bridge_process_il2p_frame(m17_ax25_bridge_t* bridge, const uint8_t* data, uint16_t length);
int m17_ax25_bridge_encode_il2p_frame(m17_ax25_bridge_t* bridge, const uint8_t* data, uint16_t length,
                                      uint8_t* il2p_data, uint16_t* il2p_length);
int m17_ax25_bridge_decode_il2p_frame(m17_ax25_bridge_t* bridge, const uint8_t* il2p_data, uint16_t il2p_length,
                                      uint8_t* data, uint16_t* length);

// M17 Specific Functions
int m17_ax25_bridge_m17_to_aprs(m17_ax25_bridge_t* bridge, const uint8_t* m17_data, uint16_t m17_length,
                                uint8_t* aprs_data, uint16_t* aprs_length);
int m17_ax25_bridge_aprs_to_m17(m17_ax25_bridge_t* bridge, const uint8_t* aprs_data, uint16_t aprs_length,
                                uint8_t* m17_data, uint16_t* m17_length);

// AX.25 Specific Functions
int m17_ax25_bridge_ax25_to_aprs(m17_ax25_bridge_t* bridge, const uint8_t* ax25_data, uint16_t ax25_length,
                                 uint8_t* aprs_data, uint16_t* aprs_length);
int m17_ax25_bridge_aprs_to_ax25(m17_ax25_bridge_t* bridge, const uint8_t* aprs_data, uint16_t aprs_length,
                                 uint8_t* ax25_data, uint16_t* ax25_length);

// APRS Functions
int m17_ax25_bridge_send_aprs_position(m17_ax25_bridge_t* bridge, const char* callsign, 
                                      double latitude, double longitude, int altitude,
                                      const char* comment);
int m17_ax25_bridge_send_aprs_status(m17_ax25_bridge_t* bridge, const char* callsign, 
                                    const char* status);
int m17_ax25_bridge_send_aprs_message(m17_ax25_bridge_t* bridge, const char* from_callsign,
                                      const char* to_callsign, const char* message);

// Utility Functions
int m17_ax25_bridge_validate_callsign(const char* callsign);
int m17_ax25_bridge_normalize_callsign(char* callsign);
int m17_ax25_bridge_compare_callsigns(const char* callsign1, const char* callsign2);

// Statistics
typedef struct {
    uint32_t m17_frames_rx;
    uint32_t m17_frames_tx;
    uint32_t ax25_frames_rx;
    uint32_t ax25_frames_tx;
    uint32_t aprs_frames_rx;
    uint32_t aprs_frames_tx;
    uint32_t protocol_switches;
    uint32_t conversion_errors;
} bridge_statistics_t;

int m17_ax25_bridge_get_statistics(const m17_ax25_bridge_t* bridge, bridge_statistics_t* stats);
int m17_ax25_bridge_reset_statistics(m17_ax25_bridge_t* bridge);

// Event Handlers
int m17_ax25_bridge_register_event_handler(m17_ax25_bridge_t* bridge, bridge_event_handler_t handler);
int m17_ax25_bridge_unregister_event_handler(m17_ax25_bridge_t* bridge);

// Configuration Management
int m17_ax25_bridge_load_config(m17_ax25_bridge_t* bridge, const char* config_file);
int m17_ax25_bridge_save_config(const m17_ax25_bridge_t* bridge, const char* config_file);

// Debug Functions
int m17_ax25_bridge_enable_debug(m17_ax25_bridge_t* bridge, bool enable);
int m17_ax25_bridge_set_debug_level(m17_ax25_bridge_t* bridge, int level);
int m17_ax25_bridge_print_status(const m17_ax25_bridge_t* bridge);

#ifdef __cplusplus
}
#endif
