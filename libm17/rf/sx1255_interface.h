//--------------------------------------------------------------------
// SX1255 RF Frontend Interface for M17
//
// SX1255 IQ modulator/demodulator interface
// Supports both M17 and AFSK modulation
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

// SX1255 Configuration Constants
#define SX1255_MAX_BANDWIDTH    500000  // 500 kHz maximum bandwidth
#define SX1255_SAMPLE_RATE      48000   // 48 kHz sample rate
#define SX1255_IQ_BUFFER_SIZE   1024    // IQ buffer size

// AFSK Configuration
#define AFSK_1200_MARK_FREQ     1200    // 1200 Hz mark frequency
#define AFSK_1200_SPACE_FREQ    2200    // 2200 Hz space frequency
#define AFSK_300_MARK_FREQ      1080    // 1080 Hz mark frequency (300 bps)
#define AFSK_300_SPACE_FREQ    1180    // 1180 Hz space frequency (300 bps)

// Modulation Types
typedef enum {
    SX1255_MOD_M17,         // M17 4FSK modulation
    SX1255_MOD_AFSK_1200,   // AFSK 1200 bps (Bell 202)
    SX1255_MOD_AFSK_300,    // AFSK 300 bps
    SX1255_MOD_PSK_2400,    // PSK 2400 bps
    SX1255_MOD_PSK_4800,    // PSK 4800 bps
    SX1255_MOD_GMSK_9600    // GMSK 9600 bps
} sx1255_modulation_t;

// SX1255 Configuration
typedef struct {
    uint32_t frequency;          // Center frequency (Hz)
    uint32_t bandwidth;          // Bandwidth (Hz)
    uint32_t sample_rate;        // Sample rate (Hz)
    sx1255_modulation_t modulation; // Modulation type
    int16_t tx_gain;             // TX gain (dB)
    int16_t rx_gain;             // RX gain (dB)
    bool full_duplex;            // Full duplex mode
} sx1255_config_t;

// IQ Sample Structure
typedef struct {
    int16_t i;                  // In-phase component
    int16_t q;                  // Quadrature component
} sx1255_iq_sample_t;

// SX1255 Interface
typedef struct {
    sx1255_config_t config;
    sx1255_iq_sample_t tx_buffer[SX1255_IQ_BUFFER_SIZE];
    sx1255_iq_sample_t rx_buffer[SX1255_IQ_BUFFER_SIZE];
    uint16_t tx_buffer_pos;
    uint16_t rx_buffer_pos;
    bool initialized;
} sx1255_interface_t;

// SX1255 Core Functions
int sx1255_init(sx1255_interface_t* rf);
int sx1255_cleanup(sx1255_interface_t* rf);
int sx1255_set_config(sx1255_interface_t* rf, const sx1255_config_t* config);
int sx1255_get_config(const sx1255_interface_t* rf, sx1255_config_t* config);

// Frequency Control
int sx1255_set_frequency(sx1255_interface_t* rf, uint32_t frequency);
int sx1255_get_frequency(const sx1255_interface_t* rf, uint32_t* frequency);
int sx1255_set_bandwidth(sx1255_interface_t* rf, uint32_t bandwidth);
int sx1255_get_bandwidth(const sx1255_interface_t* rf, uint32_t* bandwidth);

// Gain Control
int sx1255_set_tx_gain(sx1255_interface_t* rf, int16_t gain);
int sx1255_get_tx_gain(const sx1255_interface_t* rf, int16_t* gain);
int sx1255_set_rx_gain(sx1255_interface_t* rf, int16_t gain);
int sx1255_get_rx_gain(const sx1255_interface_t* rf, int16_t* gain);

// Modulation Control
int sx1255_set_modulation(sx1255_interface_t* rf, sx1255_modulation_t modulation);
int sx1255_get_modulation(const sx1255_interface_t* rf, sx1255_modulation_t* modulation);

// IQ Data Interface
int sx1255_tx_iq_samples(sx1255_interface_t* rf, const sx1255_iq_sample_t* samples, uint16_t count);
int sx1255_rx_iq_samples(sx1255_interface_t* rf, sx1255_iq_sample_t* samples, uint16_t* count);
int sx1255_tx_ready(const sx1255_interface_t* rf);
int sx1255_rx_ready(const sx1255_interface_t* rf);

// AFSK Modulation Functions
int sx1255_afsk_modulate(sx1255_interface_t* rf, const uint8_t* data, uint16_t length, 
                        sx1255_iq_sample_t* iq_out, uint16_t* iq_count);
int sx1255_afsk_demodulate(sx1255_interface_t* rf, const sx1255_iq_sample_t* iq_in, uint16_t iq_count,
                           uint8_t* data, uint16_t* length);

// AFSK Tone Generation
int sx1255_afsk_generate_tone(sx1255_interface_t* rf, uint16_t frequency, uint16_t duration_ms,
                              sx1255_iq_sample_t* iq_out, uint16_t* iq_count);
int sx1255_afsk_generate_silence(sx1255_interface_t* rf, uint16_t duration_ms,
                                 sx1255_iq_sample_t* iq_out, uint16_t* iq_count);

// M17 Modulation Functions
int sx1255_m17_modulate(sx1255_interface_t* rf, const uint8_t* symbols, uint16_t symbol_count,
                       sx1255_iq_sample_t* iq_out, uint16_t* iq_count);
int sx1255_m17_demodulate(sx1255_interface_t* rf, const sx1255_iq_sample_t* iq_in, uint16_t iq_count,
                          uint8_t* symbols, uint16_t* symbol_count);

// Hardware Interface Functions
int sx1255_hw_init(void);
int sx1255_hw_cleanup(void);
int sx1255_hw_reset(void);
int sx1255_hw_set_frequency(uint32_t frequency);
int sx1255_hw_set_bandwidth(uint32_t bandwidth);
int sx1255_hw_set_gain(int16_t tx_gain, int16_t rx_gain);
int sx1255_hw_start_tx(void);
int sx1255_hw_stop_tx(void);
int sx1255_hw_start_rx(void);
int sx1255_hw_stop_rx(void);

// DMA Interface Functions
int sx1255_dma_tx_start(const sx1255_iq_sample_t* buffer, uint16_t count);
int sx1255_dma_tx_stop(void);
int sx1255_dma_rx_start(sx1255_iq_sample_t* buffer, uint16_t count);
int sx1255_dma_rx_stop(void);
int sx1255_dma_tx_complete(void);
int sx1255_dma_rx_complete(void);

// Interrupt Functions
void sx1255_irq_handler(void);
int sx1255_register_irq_handler(void (*handler)(void));

// Utility Functions
int sx1255_calculate_iq_from_audio(sx1255_interface_t* rf, const int16_t* audio, uint16_t audio_count,
                                   sx1255_iq_sample_t* iq_out, uint16_t* iq_count);
int sx1255_calculate_audio_from_iq(sx1255_interface_t* rf, const sx1255_iq_sample_t* iq_in, uint16_t iq_count,
                                   int16_t* audio, uint16_t* audio_count);

// Filter Functions
int sx1255_apply_tx_filter(sx1255_interface_t* rf, sx1255_iq_sample_t* iq_samples, uint16_t count);
int sx1255_apply_rx_filter(sx1255_interface_t* rf, sx1255_iq_sample_t* iq_samples, uint16_t count);

// Calibration Functions
int sx1255_calibrate_tx(sx1255_interface_t* rf);
int sx1255_calibrate_rx(sx1255_interface_t* rf);
int sx1255_calibrate_iq_balance(sx1255_interface_t* rf);

#ifdef __cplusplus
}
#endif
