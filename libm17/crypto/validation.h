//--------------------------------------------------------------------
// M17 C library - crypto/validation.h
//
// Input validation and bounds checking for cryptographic operations
// Implements comprehensive security validation
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------

#ifndef M17_VALIDATION_H
#define M17_VALIDATION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Key type constants for validation
#define M17_AES_128           1
#define M17_AES_192           2
#define M17_AES_256           3
#define M17_ED25519_PRIVATE   4
#define M17_ED25519_PUBLIC    5
#define M17_CURVE25519_PRIVATE 6
#define M17_CURVE25519_PUBLIC 7

// Cipher type constants for validation
#define M17_AES_CTR           1
#define M17_AES_GCM           2

// Signature type constants for validation
#define M17_ED25519_SIGNATURE 1
#define M17_ECDSA_SIGNATURE   2

// Validate key length
bool m17_validate_key_length(size_t key_length, int key_type);

// Validate IV length
bool m17_validate_iv_length(size_t iv_length, int cipher_type);

// Validate buffer bounds
bool m17_validate_buffer_bounds(const void *ptr, size_t size, size_t offset, size_t length);

// Validate key material (check for weak keys)
bool m17_validate_key_material(const uint8_t *key, size_t key_length);

// Validate IV uniqueness (basic check)
bool m17_validate_iv_uniqueness(const uint8_t *iv, size_t iv_length, 
                                const uint8_t *previous_iv, size_t prev_iv_length);

// Validate frame number for IV generation
bool m17_validate_frame_number(uint16_t frame_number);

// Validate callsign format
bool m17_validate_callsign_format(const char *callsign);

// Validate encryption type
bool m17_validate_encryption_type(int encr_type);

// Validate signature length
bool m17_validate_signature_length(size_t signature_length, int signature_type);

// Validate data length for encryption
bool m17_validate_encryption_data_length(size_t data_length, int cipher_type);

// Comprehensive security validation for encryption operation
bool m17_validate_encryption_operation(const uint8_t *key, size_t key_length, 
                                      const uint8_t *iv, size_t iv_length,
                                      const uint8_t *data, size_t data_length,
                                      int cipher_type);

// Validate decryption operation
bool m17_validate_decryption_operation(const uint8_t *key, size_t key_length,
                                      const uint8_t *iv, size_t iv_length,
                                      const uint8_t *ciphertext, size_t ciphertext_length,
                                      int cipher_type);

// Validate signature operation
bool m17_validate_signature_operation(const uint8_t *private_key, size_t private_key_length,
                                     const uint8_t *data, size_t data_length,
                                     int signature_type);

// Validate verification operation
bool m17_validate_verification_operation(const uint8_t *public_key, size_t public_key_length,
                                        const uint8_t *signature, size_t signature_length,
                                        const uint8_t *data, size_t data_length,
                                        int signature_type);

#ifdef __cplusplus
}
#endif

#endif // M17_VALIDATION_H
