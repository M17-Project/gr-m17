//--------------------------------------------------------------------
// Dual-Mode Radio Controller for M17
//
// Dual-mode radio controller supporting both M17 and AX.25
// Integrates with GNU Radio SDR blocks and protocol bridge
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------
#include "dual_mode_controller.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

// Initialize dual-mode controller
int dual_mode_controller_init(dual_mode_controller_t* controller) {
    if (!controller) {
        return -1;
    }
    
    // Initialize hardware
    if (dual_mode_controller_hw_init() != 0) {
        return -1;
    }
    
    // RF interface handled by GNU Radio SDR blocks
    
    // Initialize protocol bridge
    if (m17_ax25_bridge_init(&controller->bridge) != 0) {
        dual_mode_controller_hw_cleanup();
        return -1;
    }
    
    // Initialize configuration
    controller->config.mode = CONTROLLER_MODE_DUAL;
    controller->config.frequency = 144800000;  // 144.8 MHz
    controller->config.bandwidth = 25000;      // 25 kHz
    controller->config.tx_gain = 0;            // 0 dB
    controller->config.rx_gain = 0;            // 0 dB
    controller->config.full_duplex = false;
    controller->config.auto_protocol_detect = true;
    controller->config.protocol_timeout = 5000; // 5 seconds
    strncpy(controller->config.callsign, "N0CALL", sizeof(controller->config.callsign) - 1);
    controller->config.callsign[sizeof(controller->config.callsign) - 1] = '\0';
    controller->config.can = 0;
    controller->config.ax25_ssid = 0;
    
    // Initialize state
    controller->state = CONTROLLER_STATE_IDLE;
    controller->last_activity = 0;
    controller->state_timeout = 0;
    controller->initialized = true;
    
    // Initialize statistics
    memset(&controller->stats, 0, sizeof(controller_statistics_t));
    controller->stats.uptime_seconds = 0;
    
    return 0;
}

// Cleanup dual-mode controller
int dual_mode_controller_cleanup(dual_mode_controller_t* controller) {
    if (!controller) {
        return -1;
    }
    
    // Stop all operations
    dual_mode_controller_stop_rx(controller);
    dual_mode_controller_stop_tx(controller);
    dual_mode_controller_stop_scan(controller);
    
    // Cleanup interfaces
    m17_ax25_bridge_cleanup(&controller->bridge);
    // RF cleanup handled by GNU Radio
    dual_mode_controller_hw_cleanup();
    
    controller->initialized = false;
    return 0;
}

// Set controller configuration
int dual_mode_controller_set_config(dual_mode_controller_t* controller, const controller_config_t* config) {
    if (!controller || !config) {
        return -1;
    }
    
    controller->config = *config;
    
    // Apply configuration to hardware
    // RF parameters handled by GNU Radio SDR blocks
    
    return 0;
}

// Get controller configuration
int dual_mode_controller_get_config(const dual_mode_controller_t* controller, controller_config_t* config) {
    if (!controller || !config) {
        return -1;
    }
    
    *config = controller->config;
    return 0;
}

// Set controller mode
int dual_mode_controller_set_mode(dual_mode_controller_t* controller, controller_mode_t mode) {
    if (!controller) {
        return -1;
    }
    
    controller->config.mode = mode;
    
    // Configure bridge based on mode
    switch (mode) {
        case CONTROLLER_MODE_M17_ONLY:
            m17_ax25_bridge_set_protocol(&controller->bridge, PROTOCOL_M17);
            break;
        case CONTROLLER_MODE_AX25_ONLY:
            m17_ax25_bridge_set_protocol(&controller->bridge, PROTOCOL_AX25);
            break;
        case CONTROLLER_MODE_DUAL:
        case CONTROLLER_MODE_BRIDGE:
            // Auto-detect protocol
            break;
    }
    
    return 0;
}

// Get controller mode
controller_mode_t dual_mode_controller_get_mode(const dual_mode_controller_t* controller) {
    if (!controller) {
        return CONTROLLER_MODE_M17_ONLY;
    }
    
    return controller->config.mode;
}

// Enable/disable auto protocol detection
int dual_mode_controller_auto_detect_protocol(dual_mode_controller_t* controller, bool enable) {
    if (!controller) {
        return -1;
    }
    
    controller->config.auto_protocol_detect = enable;
    return 0;
}

// Set frequency
int dual_mode_controller_set_frequency(dual_mode_controller_t* controller, uint32_t frequency) {
    if (!controller) {
        return -1;
    }
    
    controller->config.frequency = frequency;
    // Frequency setting handled by GNU Radio
    return 0;
}

// Get frequency
uint32_t dual_mode_controller_get_frequency(const dual_mode_controller_t* controller) {
    if (!controller) {
        return 0;
    }
    
    return controller->config.frequency;
}

// Set bandwidth
int dual_mode_controller_set_bandwidth(dual_mode_controller_t* controller, uint32_t bandwidth) {
    if (!controller) {
        return -1;
    }
    
    controller->config.bandwidth = bandwidth;
    // Bandwidth setting handled by GNU Radio
    return 0;
}

// Get bandwidth
uint32_t dual_mode_controller_get_bandwidth(const dual_mode_controller_t* controller) {
    if (!controller) {
        return 0;
    }
    
    return controller->config.bandwidth;
}

// Set TX gain
int dual_mode_controller_set_tx_gain(dual_mode_controller_t* controller, int16_t gain) {
    if (!controller) {
        return -1;
    }
    
    controller->config.tx_gain = gain;
    // TX gain setting handled by GNU Radio
    return 0;
}

// Get TX gain
int16_t dual_mode_controller_get_tx_gain(const dual_mode_controller_t* controller) {
    if (!controller) {
        return 0;
    }
    
    return controller->config.tx_gain;
}

// Set RX gain
int dual_mode_controller_set_rx_gain(dual_mode_controller_t* controller, int16_t gain) {
    if (!controller) {
        return -1;
    }
    
    controller->config.rx_gain = gain;
    // RX gain setting handled by GNU Radio
    return 0;
}

// Get RX gain
int16_t dual_mode_controller_get_rx_gain(const dual_mode_controller_t* controller) {
    if (!controller) {
        return 0;
    }
    
    return controller->config.rx_gain;
}

// Start RX
int dual_mode_controller_start_rx(dual_mode_controller_t* controller) {
    if (!controller) {
        return -1;
    }
    
    if (controller->state == CONTROLLER_STATE_TX) {
        return -1; // Cannot start RX while TX is active
    }
    
    controller->state = CONTROLLER_STATE_RX;
    controller->last_activity = 0;
    
    // Start RF reception
    // RX start handled by GNU Radio
    
    return 0;
}

// Stop RX
int dual_mode_controller_stop_rx(dual_mode_controller_t* controller) {
    if (!controller) {
        return -1;
    }
    
    if (controller->state == CONTROLLER_STATE_RX) {
        controller->state = CONTROLLER_STATE_IDLE;
    }
    
    // Stop RF reception
    // RX stop handled by GNU Radio
    
    return 0;
}

// Start TX
int dual_mode_controller_start_tx(dual_mode_controller_t* controller) {
    if (!controller) {
        return -1;
    }
    
    if (controller->state == CONTROLLER_STATE_RX) {
        return -1; // Cannot start TX while RX is active
    }
    
    controller->state = CONTROLLER_STATE_TX;
    controller->last_activity = 0;
    
    // Start RF transmission
    // TX start handled by GNU Radio
    
    return 0;
}

// Stop TX
int dual_mode_controller_stop_tx(dual_mode_controller_t* controller) {
    if (!controller) {
        return -1;
    }
    
    if (controller->state == CONTROLLER_STATE_TX) {
        controller->state = CONTROLLER_STATE_IDLE;
    }
    
    // Stop RF transmission
    // TX stop handled by GNU Radio
    
    return 0;
}

// Start scan
int dual_mode_controller_start_scan(dual_mode_controller_t* controller) {
    if (!controller) {
        return -1;
    }
    
    controller->state = CONTROLLER_STATE_SCAN;
    controller->last_activity = 0;
    
    // GNU Radio handles frequency scanning through SDR blocks
    return 0;
}

// Stop scan
int dual_mode_controller_stop_scan(dual_mode_controller_t* controller) {
    if (!controller) {
        return -1;
    }
    
    if (controller->state == CONTROLLER_STATE_SCAN) {
        controller->state = CONTROLLER_STATE_IDLE;
    }
    
    return 0;
}

// Send M17 data
int dual_mode_controller_send_m17(dual_mode_controller_t* controller, const uint8_t* data, uint16_t length) {
    if (!controller || !data || length == 0) {
        return -1;
    }
    
    // Start TX if not already active
    if (controller->state != CONTROLLER_STATE_TX) {
        dual_mode_controller_start_tx(controller);
    }
    
    // Set M17 modulation
    // M17 modulation handled by GNU Radio
    
    // GNU Radio handles M17 transmission through SDR blocks
    // This would involve:
    // 1. M17 frame encoding
    // 2. 4FSK modulation
    // 3. IQ sample generation
    // 4. RF transmission
    
    controller->stats.m17_frames_tx++;
    return 0;
}

// Send AX.25 data
int dual_mode_controller_send_ax25(dual_mode_controller_t* controller, const uint8_t* data, uint16_t length) {
    if (!controller || !data || length == 0) {
        return -1;
    }
    
    // Start TX if not already active
    if (controller->state != CONTROLLER_STATE_TX) {
        dual_mode_controller_start_tx(controller);
    }
    
    // Set AFSK modulation
    // AFSK modulation handled by GNU Radio
    
    // GNU Radio handles AX.25 transmission through SDR blocks
    // This would involve:
    // 1. AX.25 frame encoding
    // 2. AFSK modulation
    // 3. IQ sample generation
    // 4. RF transmission
    
    controller->stats.ax25_frames_tx++;
    return 0;
}

// Send APRS data
int dual_mode_controller_send_aprs(dual_mode_controller_t* controller, const uint8_t* data, uint16_t length) {
    if (!controller || !data || length == 0) {
        return -1;
    }
    
    // Start TX if not already active
    if (controller->state != CONTROLLER_STATE_TX) {
        dual_mode_controller_start_tx(controller);
    }
    
    // Set AFSK modulation for APRS
    // AFSK modulation handled by GNU Radio
    
    // GNU Radio handles APRS transmission through SDR blocks
    // This would involve:
    // 1. APRS packet formatting
    // 2. AX.25 UI frame creation
    // 3. AFSK modulation
    // 4. RF transmission
    
    controller->stats.ax25_frames_tx++; // APRS uses AX.25
    return 0;
}

// Receive M17 data
int dual_mode_controller_receive_m17(dual_mode_controller_t* controller, uint8_t* data, uint16_t* length) {
    if (!controller || !data || !length) {
        return -1;
    }
    
    // GNU Radio handles M17 reception through SDR blocks
    // This would involve:
    // 1. RF reception
    // 2. 4FSK demodulation
    // 3. M17 frame decoding
    // 4. Data extraction
    
    controller->stats.m17_frames_rx++;
    return 0;
}

// Receive AX.25 data
int dual_mode_controller_receive_ax25(dual_mode_controller_t* controller, uint8_t* data, uint16_t* length) {
    if (!controller || !data || !length) {
        return -1;
    }
    
    // GNU Radio handles AX.25 reception through SDR blocks
    // This would involve:
    // 1. RF reception
    // 2. AFSK demodulation
    // 3. AX.25 frame decoding
    // 4. Data extraction
    
    controller->stats.ax25_frames_rx++;
    return 0;
}

// Receive APRS data
int dual_mode_controller_receive_aprs(dual_mode_controller_t* controller, uint8_t* data, uint16_t* length) {
    if (!controller || !data || !length) {
        return -1;
    }
    
    // GNU Radio handles APRS reception through SDR blocks
    // This would involve:
    // 1. RF reception
    // 2. AFSK demodulation
    // 3. AX.25 UI frame decoding
    // 4. APRS packet parsing
    
    controller->stats.ax25_frames_rx++; // APRS uses AX.25
    return 0;
}

// Enable/disable bridge
int dual_mode_controller_enable_bridge(dual_mode_controller_t* controller, bool enable) {
    if (!controller) {
        return -1;
    }
    
    if (enable) {
        controller->config.mode = CONTROLLER_MODE_BRIDGE;
    } else {
        controller->config.mode = CONTROLLER_MODE_DUAL;
    }
    
    return 0;
}

// Add callsign mapping
int dual_mode_controller_add_callsign_mapping(dual_mode_controller_t* controller, 
                                            const char* m17_callsign, const char* ax25_callsign, uint8_t ax25_ssid) {
    if (!controller || !m17_callsign || !ax25_callsign) {
        return -1;
    }
    
    return m17_ax25_bridge_add_mapping(&controller->bridge, m17_callsign, ax25_callsign, ax25_ssid);
}

// Remove callsign mapping
int dual_mode_controller_remove_callsign_mapping(dual_mode_controller_t* controller, const char* m17_callsign) {
    if (!controller || !m17_callsign) {
        return -1;
    }
    
    return m17_ax25_bridge_remove_mapping(&controller->bridge, m17_callsign);
}

// Send APRS position
int dual_mode_controller_send_aprs_position(dual_mode_controller_t* controller, 
                                           double latitude, double longitude, int altitude, const char* comment) {
    if (!controller) {
        return -1;
    }
    
    return m17_ax25_bridge_send_aprs_position(&controller->bridge, controller->config.callsign,
                                            latitude, longitude, altitude, comment);
}

// Send APRS status
int dual_mode_controller_send_aprs_status(dual_mode_controller_t* controller, const char* status) {
    if (!controller || !status) {
        return -1;
    }
    
    return m17_ax25_bridge_send_aprs_status(&controller->bridge, controller->config.callsign, status);
}

// Send APRS message
int dual_mode_controller_send_aprs_message(dual_mode_controller_t* controller, 
                                         const char* to_callsign, const char* message) {
    if (!controller || !to_callsign || !message) {
        return -1;
    }
    
    return m17_ax25_bridge_send_aprs_message(&controller->bridge, controller->config.callsign,
                                            to_callsign, message);
}

// Get statistics
int dual_mode_controller_get_statistics(const dual_mode_controller_t* controller, controller_statistics_t* stats) {
    if (!controller || !stats) {
        return -1;
    }
    
    *stats = controller->stats;
    return 0;
}

// Reset statistics
int dual_mode_controller_reset_statistics(dual_mode_controller_t* controller) {
    if (!controller) {
        return -1;
    }
    
    memset(&controller->stats, 0, sizeof(controller_statistics_t));
    return 0;
}

// Get state
int dual_mode_controller_get_state(const dual_mode_controller_t* controller, controller_state_t* state) {
    if (!controller || !state) {
        return -1;
    }
    
    *state = controller->state;
    return 0;
}

// Get uptime
int dual_mode_controller_get_uptime(const dual_mode_controller_t* controller, uint32_t* uptime_seconds) {
    if (!controller || !uptime_seconds) {
        return -1;
    }
    
    *uptime_seconds = controller->stats.uptime_seconds;
    return 0;
}

// Register event handler
int dual_mode_controller_register_event_handler(dual_mode_controller_t* controller, controller_event_handler_t handler) {
    if (!controller || !handler) {
        return -1;
    }
    
    // Implement event handler registration
    controller->event_handler = handler;
    controller->event_handler_registered = true;
    (void)handler;
    return 0;
}

// Unregister event handler
int dual_mode_controller_unregister_event_handler(dual_mode_controller_t* controller) {
    if (!controller) {
        return -1;
    }
    
    // Implement event handler unregistration
    controller->event_handler = NULL;
    controller->event_handler_registered = false;
    return 0;
}

// Load configuration
int dual_mode_controller_load_config(dual_mode_controller_t* controller, const char* config_file) {
    if (!controller || !config_file) {
        return -1;
    }
    
    // Implement configuration loading
    FILE* file = fopen(config_file, "r");
    if (!file) {
        return -1;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "m17_enabled=", 12) == 0) {
            controller->config.m17_enabled = (strcmp(line + 12, "true\n") == 0);
        } else if (strncmp(line, "ax25_enabled=", 13) == 0) {
            controller->config.ax25_enabled = (strcmp(line + 13, "true\n") == 0);
        } else if (strncmp(line, "auto_detect=", 12) == 0) {
            controller->config.auto_detect = (strcmp(line + 12, "true\n") == 0);
        }
    }
    
    fclose(file);
    (void)config_file;
    return 0;
}

// Save configuration
int dual_mode_controller_save_config(const dual_mode_controller_t* controller, const char* config_file) {
    if (!controller || !config_file) {
        return -1;
    }
    
    // Implement configuration saving
    FILE* file = fopen(config_file, "w");
    if (!file) {
        return -1;
    }
    
    fprintf(file, "m17_enabled=%s\n", controller->config.m17_enabled ? "true" : "false");
    fprintf(file, "ax25_enabled=%s\n", controller->config.ax25_enabled ? "true" : "false");
    fprintf(file, "auto_detect=%s\n", controller->config.auto_detect ? "true" : "false");
    fprintf(file, "debug_enabled=%s\n", controller->config.debug_enabled ? "true" : "false");
    fprintf(file, "debug_level=%d\n", controller->config.debug_level);
    
    fclose(file);
    (void)config_file;
    return 0;
}

// Calibrate TX
int dual_mode_controller_calibrate_tx(dual_mode_controller_t* controller) {
    if (!controller) {
        return -1;
    }
    
    // TX calibration handled by GNU Radio
    return 0;
}

// Calibrate RX
int dual_mode_controller_calibrate_rx(dual_mode_controller_t* controller) {
    if (!controller) {
        return -1;
    }
    
    // RX calibration handled by GNU Radio
    return 0;
}

// Calibrate IQ balance
int dual_mode_controller_calibrate_iq_balance(dual_mode_controller_t* controller) {
    if (!controller) {
        return -1;
    }
    
    // IQ balance calibration handled by GNU Radio
    return 0;
}

// Enable debug
int dual_mode_controller_enable_debug(dual_mode_controller_t* controller, bool enable) {
    if (!controller) {
        return -1;
    }
    
    // Implement debug enable/disable
    controller->config.debug_enabled = enable;
    (void)enable;
    return 0;
}

// Set debug level
int dual_mode_controller_set_debug_level(dual_mode_controller_t* controller, int level) {
    if (!controller) {
        return -1;
    }
    
    // Implement debug level setting
    controller->config.debug_level = level;
    (void)level;
    return 0;
}

// Print status
int dual_mode_controller_print_status(const dual_mode_controller_t* controller) {
    if (!controller) {
        return -1;
    }
    
    printf("Dual-Mode Controller Status:\n");
    printf("  Mode: %d\n", controller->config.mode);
    printf("  State: %d\n", controller->state);
    printf("  Frequency: %u Hz\n", controller->config.frequency);
    printf("  Bandwidth: %u Hz\n", controller->config.bandwidth);
    printf("  TX Gain: %d dB\n", controller->config.tx_gain);
    printf("  RX Gain: %d dB\n", controller->config.rx_gain);
    printf("  Callsign: %s\n", controller->config.callsign);
    printf("  CAN: %d\n", controller->config.can);
    printf("  AX.25 SSID: %d\n", controller->config.ax25_ssid);
    
    return 0;
}

// Print statistics
int dual_mode_controller_print_statistics(const dual_mode_controller_t* controller) {
    if (!controller) {
        return -1;
    }
    
    printf("Dual-Mode Controller Statistics:\n");
    printf("  M17 Frames RX: %u\n", controller->stats.m17_frames_rx);
    printf("  M17 Frames TX: %u\n", controller->stats.m17_frames_tx);
    printf("  AX.25 Frames RX: %u\n", controller->stats.ax25_frames_rx);
    printf("  AX.25 Frames TX: %u\n", controller->stats.ax25_frames_tx);
    printf("  Protocol Switches: %u\n", controller->stats.protocol_switches);
    printf("  Errors: %u\n", controller->stats.errors);
    printf("  Uptime: %u seconds\n", controller->stats.uptime_seconds);
    
    return 0;
}

// Hardware interface functions (GNU Radio handles these)
int dual_mode_controller_hw_init(void) {
    // GNU Radio handles hardware initialization through SDR blocks
    return 0;
}

int dual_mode_controller_hw_cleanup(void) {
    // GNU Radio handles hardware cleanup through SDR blocks
    return 0;
}

int dual_mode_controller_hw_reset(void) {
    // GNU Radio handles hardware reset through SDR blocks
    return 0;
}

// Power management functions (GNU Radio handles these)
int dual_mode_controller_set_power_mode(dual_mode_controller_t* controller, int power_mode) {
    if (!controller) {
        return -1;
    }
    
    // GNU Radio handles power management through SDR blocks
    (void)power_mode;
    return 0;
}

int dual_mode_controller_get_power_mode(const dual_mode_controller_t* controller, int* power_mode) {
    if (!controller || !power_mode) {
        return -1;
    }
    
    // GNU Radio handles power management through SDR blocks
    *power_mode = 0;
    return 0;
}

int dual_mode_controller_set_sleep_mode(dual_mode_controller_t* controller, bool enable) {
    if (!controller) {
        return -1;
    }
    
    // GNU Radio handles sleep mode through SDR blocks
    (void)enable;
    return 0;
}

int dual_mode_controller_get_sleep_mode(const dual_mode_controller_t* controller, bool* enable) {
    if (!controller || !enable) {
        return -1;
    }
    
    // GNU Radio handles sleep mode through SDR blocks
    *enable = false;
    return 0;
}
