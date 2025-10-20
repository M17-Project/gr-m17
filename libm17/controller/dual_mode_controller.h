//--------------------------------------------------------------------
// Dual-Mode Radio Controller for M17
//
// Dual-mode radio controller supporting both M17 and AX.25
// Integrates with GNU Radio SDR blocks and protocol bridge
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
// RF interface handled by GNU Radio SDR blocks
#include "../bridge/m17_ax25_bridge.h"

#ifdef __cplusplus
extern "C" {
#endif

// Controller Modes
typedef enum {
    CONTROLLER_MODE_M17_ONLY,      // M17 only mode
    CONTROLLER_MODE_AX25_ONLY,     // AX.25 only mode
    CONTROLLER_MODE_DUAL,          // Dual mode (auto-switch)
    CONTROLLER_MODE_BRIDGE         // Bridge mode (convert between protocols)
} controller_mode_t;

// Controller State
typedef enum {
    CONTROLLER_STATE_IDLE,
    CONTROLLER_STATE_RX,
    CONTROLLER_STATE_TX,
    CONTROLLER_STATE_SCAN,
    CONTROLLER_STATE_ERROR
} controller_state_t;

// Controller Configuration
typedef struct {
    controller_mode_t mode;
    uint32_t frequency;
    uint32_t bandwidth;
    int16_t tx_gain;
    int16_t rx_gain;
    bool full_duplex;
    bool auto_protocol_detect;
    uint32_t protocol_timeout;
    char callsign[10];
    uint8_t can;                    // M17 Channel Access Number
    uint8_t ax25_ssid;              // AX.25 SSID
    bool m17_enabled;
    bool ax25_enabled;
    bool auto_detect;
    bool debug_enabled;
    int debug_level;
} controller_config_t;

// Controller Statistics
typedef struct {
    uint32_t m17_frames_rx;
    uint32_t m17_frames_tx;
    uint32_t ax25_frames_rx;
    uint32_t ax25_frames_tx;
    uint32_t protocol_switches;
    uint32_t errors;
    uint32_t uptime_seconds;
} controller_statistics_t;

// Controller Interface
typedef struct {
    controller_config_t config;
    controller_state_t state;
    controller_statistics_t stats;
    
    // Hardware interfaces
    // RF interface handled by GNU Radio SDR blocks
    m17_ax25_bridge_t bridge;
    
    // State management
    uint32_t last_activity;
    uint32_t state_timeout;
    bool initialized;
    
    // Event handling
    void* event_handler;
    bool event_handler_registered;
} dual_mode_controller_t;

// Controller Core Functions
int dual_mode_controller_init(dual_mode_controller_t* controller);
int dual_mode_controller_cleanup(dual_mode_controller_t* controller);
int dual_mode_controller_set_config(dual_mode_controller_t* controller, const controller_config_t* config);
int dual_mode_controller_get_config(const dual_mode_controller_t* controller, controller_config_t* config);

// Mode Control
int dual_mode_controller_set_mode(dual_mode_controller_t* controller, controller_mode_t mode);
controller_mode_t dual_mode_controller_get_mode(const dual_mode_controller_t* controller);
int dual_mode_controller_auto_detect_protocol(dual_mode_controller_t* controller, bool enable);

// Frequency Control
int dual_mode_controller_set_frequency(dual_mode_controller_t* controller, uint32_t frequency);
uint32_t dual_mode_controller_get_frequency(const dual_mode_controller_t* controller);
int dual_mode_controller_set_bandwidth(dual_mode_controller_t* controller, uint32_t bandwidth);
uint32_t dual_mode_controller_get_bandwidth(const dual_mode_controller_t* controller);

// Gain Control
int dual_mode_controller_set_tx_gain(dual_mode_controller_t* controller, int16_t gain);
int16_t dual_mode_controller_get_tx_gain(const dual_mode_controller_t* controller);
int dual_mode_controller_set_rx_gain(dual_mode_controller_t* controller, int16_t gain);
int16_t dual_mode_controller_get_rx_gain(const dual_mode_controller_t* controller);

// State Control
int dual_mode_controller_start_rx(dual_mode_controller_t* controller);
int dual_mode_controller_stop_rx(dual_mode_controller_t* controller);
int dual_mode_controller_start_tx(dual_mode_controller_t* controller);
int dual_mode_controller_stop_tx(dual_mode_controller_t* controller);
int dual_mode_controller_start_scan(dual_mode_controller_t* controller);
int dual_mode_controller_stop_scan(dual_mode_controller_t* controller);

// Data Transmission
int dual_mode_controller_send_m17(dual_mode_controller_t* controller, const uint8_t* data, uint16_t length);
int dual_mode_controller_send_ax25(dual_mode_controller_t* controller, const uint8_t* data, uint16_t length);
int dual_mode_controller_send_aprs(dual_mode_controller_t* controller, const uint8_t* data, uint16_t length);

// Data Reception
int dual_mode_controller_receive_m17(dual_mode_controller_t* controller, uint8_t* data, uint16_t* length);
int dual_mode_controller_receive_ax25(dual_mode_controller_t* controller, uint8_t* data, uint16_t* length);
int dual_mode_controller_receive_aprs(dual_mode_controller_t* controller, uint8_t* data, uint16_t* length);

// Protocol Bridge Functions
int dual_mode_controller_enable_bridge(dual_mode_controller_t* controller, bool enable);
int dual_mode_controller_add_callsign_mapping(dual_mode_controller_t* controller, 
                                            const char* m17_callsign, const char* ax25_callsign, uint8_t ax25_ssid);
int dual_mode_controller_remove_callsign_mapping(dual_mode_controller_t* controller, const char* m17_callsign);

// APRS Functions
int dual_mode_controller_send_aprs_position(dual_mode_controller_t* controller, 
                                           double latitude, double longitude, int altitude, const char* comment);
int dual_mode_controller_send_aprs_status(dual_mode_controller_t* controller, const char* status);
int dual_mode_controller_send_aprs_message(dual_mode_controller_t* controller, 
                                          const char* to_callsign, const char* message);

// M17 Functions
int dual_mode_controller_send_m17_voice(dual_mode_controller_t* controller, const int16_t* audio, uint16_t samples);
int dual_mode_controller_send_m17_data(dual_mode_controller_t* controller, const uint8_t* data, uint16_t length);
int dual_mode_controller_send_m17_position(dual_mode_controller_t* controller, 
                                          double latitude, double longitude, int altitude);

// AX.25 Functions
int dual_mode_controller_connect_ax25(dual_mode_controller_t* controller, const char* callsign);
int dual_mode_controller_disconnect_ax25(dual_mode_controller_t* controller, const char* callsign);
int dual_mode_controller_send_ax25_data(dual_mode_controller_t* controller, const char* callsign, 
                                       const uint8_t* data, uint16_t length);

// Statistics and Monitoring
int dual_mode_controller_get_statistics(const dual_mode_controller_t* controller, controller_statistics_t* stats);
int dual_mode_controller_reset_statistics(dual_mode_controller_t* controller);
int dual_mode_controller_get_state(const dual_mode_controller_t* controller, controller_state_t* state);
int dual_mode_controller_get_uptime(const dual_mode_controller_t* controller, uint32_t* uptime_seconds);

// Event Handling
typedef void (*controller_event_handler_t)(controller_state_t state, const uint8_t* data, uint16_t length);

int dual_mode_controller_register_event_handler(dual_mode_controller_t* controller, controller_event_handler_t handler);
int dual_mode_controller_unregister_event_handler(dual_mode_controller_t* controller);

// Configuration Management
int dual_mode_controller_load_config(dual_mode_controller_t* controller, const char* config_file);
int dual_mode_controller_save_config(const dual_mode_controller_t* controller, const char* config_file);

// Calibration
int dual_mode_controller_calibrate_tx(dual_mode_controller_t* controller);
int dual_mode_controller_calibrate_rx(dual_mode_controller_t* controller);
int dual_mode_controller_calibrate_iq_balance(dual_mode_controller_t* controller);

// Debug and Diagnostics
int dual_mode_controller_enable_debug(dual_mode_controller_t* controller, bool enable);
int dual_mode_controller_set_debug_level(dual_mode_controller_t* controller, int level);
int dual_mode_controller_print_status(const dual_mode_controller_t* controller);
int dual_mode_controller_print_statistics(const dual_mode_controller_t* controller);

// Hardware Interface
int dual_mode_controller_hw_init(void);
int dual_mode_controller_hw_cleanup(void);
int dual_mode_controller_hw_reset(void);

// Power Management
int dual_mode_controller_set_power_mode(dual_mode_controller_t* controller, int power_mode);
int dual_mode_controller_get_power_mode(const dual_mode_controller_t* controller, int* power_mode);
int dual_mode_controller_set_sleep_mode(dual_mode_controller_t* controller, bool enable);
int dual_mode_controller_get_sleep_mode(const dual_mode_controller_t* controller, bool* enable);

#ifdef __cplusplus
}
#endif
