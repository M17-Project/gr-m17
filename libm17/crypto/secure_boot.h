//--------------------------------------------------------------------
// M17 C library - crypto/secure_boot.h
//
// Secure boot chain validation for M17 cryptographic operations
// Ensures hardware-enforced security from boot to runtime
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------

#ifndef M17_SECURE_BOOT_H
#define M17_SECURE_BOOT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Secure boot status codes
typedef enum {
    M17_SB_SUCCESS = 0,
    M17_SB_ERROR_INVALID_PARAM = -1,
    M17_SB_ERROR_BOOT_CHAIN_INVALID = -2,
    M17_SB_ERROR_SIGNATURE_INVALID = -3,
    M17_SB_ERROR_HARDWARE_UNAVAILABLE = -4,
    M17_SB_ERROR_VERIFICATION_FAILED = -5,
    M17_SB_ERROR_ATTESTATION_FAILED = -6
} m17_sb_status_t;

// Secure boot validation levels
typedef enum {
    M17_SB_LEVEL_NONE = 0,
    M17_SB_LEVEL_BASIC = 1,
    M17_SB_LEVEL_ENHANCED = 2,
    M17_SB_LEVEL_MAXIMUM = 3
} m17_sb_level_t;

// Secure boot component types
typedef enum {
    M17_SB_COMPONENT_BOOTLOADER = 1,
    M17_SB_COMPONENT_KERNEL = 2,
    M17_SB_COMPONENT_TEE = 3,
    M17_SB_COMPONENT_M17_LIBRARY = 4,
    M17_SB_COMPONENT_CRYPTO_MODULE = 5
} m17_sb_component_t;

// Secure boot attestation structure
typedef struct {
    uint32_t component_id;
    uint8_t component_hash[32];
    uint8_t signature[64];
    uint64_t timestamp;
    uint32_t version;
    bool is_verified;
} m17_sb_attestation_t;

// Secure boot chain validation
m17_sb_status_t m17_sb_validate_boot_chain(void);
m17_sb_status_t m17_sb_get_boot_status(m17_sb_level_t *level, bool *is_valid);
m17_sb_status_t m17_sb_validate_component(m17_sb_component_t component);

// Secure boot attestation
m17_sb_status_t m17_sb_create_attestation(m17_sb_component_t component,
                                          m17_sb_attestation_t *attestation);
m17_sb_status_t m17_sb_verify_attestation(const m17_sb_attestation_t *attestation);
m17_sb_status_t m17_sb_get_attestation_report(uint8_t *report, size_t *report_size);

// Secure boot key management
m17_sb_status_t m17_sb_validate_root_key(void);
m17_sb_status_t m17_sb_get_root_key_hash(uint8_t *hash, size_t *hash_size);
m17_sb_status_t m17_sb_validate_chain_of_trust(void);

// Secure boot monitoring
m17_sb_status_t m17_sb_get_boot_measurements(uint8_t *measurements, size_t *measurement_size);
m17_sb_status_t m17_sb_monitor_integrity(bool *is_intact);
m17_sb_status_t m17_sb_detect_tampering(bool *tampering_detected);

// Secure boot hardware features
m17_sb_status_t m17_sb_check_secure_boot_hardware(bool *is_available);
m17_sb_status_t m17_sb_get_hardware_features(uint32_t *features);
m17_sb_status_t m17_sb_validate_hardware_roots_of_trust(void);

#endif // M17_SECURE_BOOT_H
