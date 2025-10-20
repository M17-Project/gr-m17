//--------------------------------------------------------------------
// Dual-Mode Radio Example Application
//
// Example application demonstrating M17 and AX.25 dual-mode operation
// with GNU Radio SDR blocks and protocol bridge
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include "../libm17/controller/dual_mode_controller.h"

// Global controller instance
dual_mode_controller_t g_controller;
volatile bool g_running = true;

// Signal handler for graceful shutdown
void signal_handler(int sig) {
    printf("\nReceived signal %d, shutting down...\n", sig);
    g_running = false;
}

// Print usage information
void print_usage(const char* program_name) {
    printf("Usage: %s [options]\n", program_name);
    printf("Options:\n");
    printf("  -f <frequency>    Set frequency in Hz (default: 144800000)\n");
    printf("  -m <mode>        Set mode: m17, ax25, dual, bridge (default: dual)\n");
    printf("  -c <callsign>    Set callsign (default: N0CALL)\n");
    printf("  -s <ssid>        Set AX.25 SSID (default: 0)\n");
    printf("  -h               Show this help\n");
    printf("\nModes:\n");
    printf("  m17     - M17 only mode\n");
    printf("  ax25    - AX.25 only mode\n");
    printf("  dual    - Dual mode with auto-detection\n");
    printf("  bridge  - Bridge mode (convert between protocols)\n");
}

// Parse command line arguments
int parse_arguments(int argc, char* argv[], controller_config_t* config) {
    int opt;
    
    // Set defaults
    config->mode = CONTROLLER_MODE_DUAL;
    config->frequency = 144800000;
    config->bandwidth = 25000;
    config->tx_gain = 0;
    config->rx_gain = 0;
    config->full_duplex = false;
    config->auto_protocol_detect = true;
    config->protocol_timeout = 5000;
    /* Safe copy with bounds checking */
    strncpy(config->callsign, "N0CALL", sizeof(config->callsign) - 1);
    config->callsign[sizeof(config->callsign) - 1] = '\0';
    config->can = 0;
    config->ax25_ssid = 0;
    
    while ((opt = getopt(argc, argv, "f:m:c:s:h")) != -1) {
        switch (opt) {
            case 'f':
                config->frequency = atoi(optarg);
                break;
            case 'm':
                if (strcmp(optarg, "m17") == 0) {
                    config->mode = CONTROLLER_MODE_M17_ONLY;
                } else if (strcmp(optarg, "ax25") == 0) {
                    config->mode = CONTROLLER_MODE_AX25_ONLY;
                } else if (strcmp(optarg, "dual") == 0) {
                    config->mode = CONTROLLER_MODE_DUAL;
                } else if (strcmp(optarg, "bridge") == 0) {
                    config->mode = CONTROLLER_MODE_BRIDGE;
                } else {
                    printf("Invalid mode: %s\n", optarg);
                    return -1;
                }
                break;
            case 'c':
                strncpy(config->callsign, optarg, 9);
                config->callsign[9] = '\0';
                break;
            case 's':
                config->ax25_ssid = atoi(optarg);
                break;
            case 'h':
                return -1;
            default:
                return -1;
        }
    }
    
    return 0;
}

// Initialize controller
int init_controller(const controller_config_t* config) {
    printf("Initializing dual-mode controller...\n");
    
    if (dual_mode_controller_init(&g_controller) != 0) {
        printf("Failed to initialize controller\n");
        return -1;
    }
    
    if (dual_mode_controller_set_config(&g_controller, config) != 0) {
        printf("Failed to set controller configuration\n");
        dual_mode_controller_cleanup(&g_controller);
        return -1;
    }
    
    printf("Controller initialized successfully\n");
    return 0;
}

// Main application loop
int main_loop(void) {
    uint8_t rx_buffer[1024];
    uint16_t rx_length;
    controller_state_t state;
    controller_statistics_t stats;
    uint32_t last_stats_time = 0;
    
    printf("Starting main loop...\n");
    printf("Press Ctrl+C to exit\n\n");
    
    // Start RX
    if (dual_mode_controller_start_rx(&g_controller) != 0) {
        printf("Failed to start RX\n");
        return -1;
    }
    
    while (g_running) {
        // Get current state
        if (dual_mode_controller_get_state(&g_controller, &state) == 0) {
            if (state == CONTROLLER_STATE_RX) {
                // Try to receive data
                rx_length = sizeof(rx_buffer);
                if (dual_mode_controller_receive_m17(&g_controller, rx_buffer, &rx_length) == 0 && rx_length > 0) {
                    printf("M17 RX: %d bytes\n", rx_length);
                }
                
                rx_length = sizeof(rx_buffer);
                if (dual_mode_controller_receive_ax25(&g_controller, rx_buffer, &rx_length) == 0 && rx_length > 0) {
                    printf("AX.25 RX: %d bytes\n", rx_length);
                }
                
                rx_length = sizeof(rx_buffer);
                if (dual_mode_controller_receive_aprs(&g_controller, rx_buffer, &rx_length) == 0 && rx_length > 0) {
                    printf("APRS RX: %d bytes\n", rx_length);
                }
            }
        }
        
        // Print statistics every 10 seconds
        uint32_t current_time = time(NULL);
        if (current_time - last_stats_time >= 10) {
            if (dual_mode_controller_get_statistics(&g_controller, &stats) == 0) {
                printf("\n--- Statistics ---\n");
                printf("M17 Frames RX: %u\n", stats.m17_frames_rx);
                printf("M17 Frames TX: %u\n", stats.m17_frames_tx);
                printf("AX.25 Frames RX: %u\n", stats.ax25_frames_rx);
                printf("AX.25 Frames TX: %u\n", stats.ax25_frames_tx);
                printf("Protocol Switches: %u\n", stats.protocol_switches);
                printf("Errors: %u\n", stats.errors);
                printf("Uptime: %u seconds\n", stats.uptime_seconds);
                printf("------------------\n\n");
            }
            last_stats_time = current_time;
        }
        
        // Small delay to prevent busy waiting
        usleep(10000); // 10ms
    }
    
    return 0;
}

// Demo functions
int demo_m17_transmission(void) {
    printf("\n--- M17 Transmission Demo ---\n");
    
    const char* test_message = "Hello from M17!";
    uint8_t m17_data[256];
    uint16_t m17_length = strlen(test_message);
    
    // Convert string to M17 data (simplified)
    memcpy(m17_data, test_message, m17_length);
    
    // Start TX
    if (dual_mode_controller_start_tx(&g_controller) != 0) {
        printf("Failed to start TX\n");
        return -1;
    }
    
    // Send M17 data
    if (dual_mode_controller_send_m17(&g_controller, m17_data, m17_length) != 0) {
        printf("Failed to send M17 data\n");
        dual_mode_controller_stop_tx(&g_controller);
        return -1;
    }
    
    printf("M17 data sent: %s\n", test_message);
    
    // Stop TX
    dual_mode_controller_stop_tx(&g_controller);
    
    return 0;
}

int demo_ax25_transmission(void) {
    printf("\n--- AX.25 Transmission Demo ---\n");
    
    const char* test_message = "Hello from AX.25!";
    uint8_t ax25_data[256];
    uint16_t ax25_length = strlen(test_message);
    
    // Convert string to AX.25 data (simplified)
    memcpy(ax25_data, test_message, ax25_length);
    
    // Start TX
    if (dual_mode_controller_start_tx(&g_controller) != 0) {
        printf("Failed to start TX\n");
        return -1;
    }
    
    // Send AX.25 data
    if (dual_mode_controller_send_ax25(&g_controller, ax25_data, ax25_length) != 0) {
        printf("Failed to send AX.25 data\n");
        dual_mode_controller_stop_tx(&g_controller);
        return -1;
    }
    
    printf("AX.25 data sent: %s\n", test_message);
    
    // Stop TX
    dual_mode_controller_stop_tx(&g_controller);
    
    return 0;
}

int demo_aprs_transmission(void) {
    printf("\n--- APRS Transmission Demo ---\n");
    
    // Send APRS position
    if (dual_mode_controller_send_aprs_position(&g_controller, 52.2297, 21.0122, 100, "M17-AX.25 Bridge") != 0) {
        printf("Failed to send APRS position\n");
        return -1;
    }
    
    printf("APRS position sent: 52.2297N, 21.0122E, 100m\n");
    
    // Send APRS status
    if (dual_mode_controller_send_aprs_status(&g_controller, "M17-AX.25 Bridge Online") != 0) {
        printf("Failed to send APRS status\n");
        return -1;
    }
    
    printf("APRS status sent: M17-AX.25 Bridge Online\n");
    
    return 0;
}

int demo_protocol_bridge(void) {
    printf("\n--- Protocol Bridge Demo ---\n");
    
    // Add callsign mapping
    if (dual_mode_controller_add_callsign_mapping(&g_controller, "SP5WWP", "SP5WWP", 0) != 0) {
        printf("Failed to add callsign mapping\n");
        return -1;
    }
    
    printf("Callsign mapping added: SP5WWP <-> SP5WWP-0\n");
    
    // Enable bridge mode
    if (dual_mode_controller_enable_bridge(&g_controller, true) != 0) {
        printf("Failed to enable bridge mode\n");
        return -1;
    }
    
    printf("Bridge mode enabled\n");
    
    return 0;
}

// Main function
int main(int argc, char* argv[]) {
    controller_config_t config;
    
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("M17-AX.25 Dual-Mode Radio Controller\n");
    printf("====================================\n\n");
    
    // Parse command line arguments
    if (parse_arguments(argc, argv, &config) != 0) {
        print_usage(argv[0]);
        return 1;
    }
    
    // Print configuration
    printf("Configuration:\n");
    printf("  Frequency: %u Hz\n", config.frequency);
    printf("  Mode: %d\n", config.mode);
    printf("  Callsign: %s\n", config.callsign);
    printf("  AX.25 SSID: %d\n", config.ax25_ssid);
    printf("\n");
    
    // Initialize controller
    if (init_controller(&config) != 0) {
        return 1;
    }
    
    // Print controller status
    dual_mode_controller_print_status(&g_controller);
    printf("\n");
    
    // Run demos
    demo_protocol_bridge();
    demo_m17_transmission();
    demo_ax25_transmission();
    demo_aprs_transmission();
    
    // Start main loop
    if (main_loop() != 0) {
        printf("Main loop failed\n");
        dual_mode_controller_cleanup(&g_controller);
        return 1;
    }
    
    // Cleanup
    printf("Cleaning up...\n");
    dual_mode_controller_cleanup(&g_controller);
    
    printf("Exiting...\n");
    return 0;
}
