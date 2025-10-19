//--------------------------------------------------------------------
// M17 C library - crypto/validation.c
//
// Input validation and bounds checking for cryptographic operations
// Implements comprehensive security validation
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------

#include "m17.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// Validate key length
bool m17_validate_key_length(size_t key_length, int key_type) {
    switch (key_type) {
        case M17_AES_128:
            return key_length == 16;
        case M17_AES_192:
            return key_length == 24;
        case M17_AES_256:
            return key_length == 32;
        case M17_ED25519_PRIVATE:
            return key_length == M17_ED25519_PRIVATE_KEY_SIZE;
        case M17_ED25519_PUBLIC:
            return key_length == M17_ED25519_PUBLIC_KEY_SIZE;
        case M17_CURVE25519_PRIVATE:
            return key_length == M17_CURVE25519_PRIVATE_KEY_SIZE;
        case M17_CURVE25519_PUBLIC:
            return key_length == M17_CURVE25519_PUBLIC_KEY_SIZE;
        default:
            return false;
    }
}

// Validate IV length
bool m17_validate_iv_length(size_t iv_length, int cipher_type) {
    switch (cipher_type) {
        case M17_AES_CTR:
            return iv_length == 16; // 128-bit IV for CTR mode
        case M17_AES_GCM:
            return iv_length == M17_AES_GCM_IV_SIZE;
        default:
            return false;
    }
}

// Validate buffer bounds
bool m17_validate_buffer_bounds(const void *ptr, size_t size, size_t offset, size_t length) {
    if (ptr == NULL) {
        return false;
    }
    
    if (offset >= size) {
        return false;
    }
    
    if (length == 0) {
        return true;
    }
    
    if (offset + length > size) {
        return false;
    }
    
    return true;
}

// Validate key material (check for weak keys)
bool m17_validate_key_material(const uint8_t *key, size_t key_length) {
    if (key == NULL || key_length == 0) {
        return false;
    }
    
    // Check for all zeros (weak key)
    bool all_zeros = true;
    for (size_t i = 0; i < key_length; i++) {
        if (key[i] != 0x00) {
            all_zeros = false;
            break;
        }
    }
    if (all_zeros) {
        return false;
    }
    
    // Check for all ones (weak key)
    bool all_ones = true;
    for (size_t i = 0; i < key_length; i++) {
        if (key[i] != 0xFF) {
            all_ones = false;
            break;
        }
    }
    if (all_ones) {
        return false;
    }
    
    // Check for repeated patterns (weak key)
    if (key_length >= 2) {
        bool repeated_pattern = true;
        for (size_t i = 1; i < key_length; i++) {
            if (key[i] != key[0]) {
                repeated_pattern = false;
                break;
            }
        }
        if (repeated_pattern) {
            return false;
        }
    }
    
    // Check for sequential patterns (weak key)
    if (key_length >= 2) {
        bool sequential = true;
        for (size_t i = 1; i < key_length; i++) {
            if (key[i] != (key[i-1] + 1) % 256) {
                sequential = false;
                break;
            }
        }
        if (sequential) {
            return false;
        }
    }
    
    return true;
}

// Validate IV uniqueness (basic check)
bool m17_validate_iv_uniqueness(const uint8_t *iv, size_t iv_length, const uint8_t *previous_iv, size_t prev_iv_length) {
    if (iv == NULL || iv_length == 0) {
        return false;
    }
    
    // If no previous IV, this is the first one (valid)
    if (previous_iv == NULL || prev_iv_length == 0) {
        return true;
    }
    
    // Check if IVs are different
    if (iv_length != prev_iv_length) {
        return true; // Different lengths are considered unique
    }
    
    for (size_t i = 0; i < iv_length; i++) {
        if (iv[i] != previous_iv[i]) {
            return true; // Found a difference
        }
    }
    
    return false; // IVs are identical (not unique)
}

// Validate frame number for IV generation
bool m17_validate_frame_number(uint16_t frame_number) {
    // Frame number should be within valid range
    // M17 uses 16-bit frame numbers, so 0-65535 is valid
    return frame_number <= 0xFFFF;
}

// Validate callsign format
bool m17_validate_callsign_format(const char *callsign) {
    if (callsign == NULL) {
        return false;
    }
    
    size_t length = strlen(callsign);
    if (length == 0 || length > 9) { // M17 callsigns are 1-9 characters
        return false;
    }
    
    // Check for valid characters (alphanumeric and some special chars)
    for (size_t i = 0; i < length; i++) {
        char c = callsign[i];
        if (!((c >= 'A' && c <= 'Z') || 
              (c >= 'a' && c <= 'z') || 
              (c >= '0' && c <= '9') || 
              c == '-' || c == '/')) {
            return false;
        }
    }
    
    return true;
}

// Validate encryption type
bool m17_validate_encryption_type(int encr_type) {
    switch (encr_type) {
        case M17_TYPE_ENCR_NONE:
        case M17_TYPE_ENCR_SCRAM:
        case M17_TYPE_ENCR_AES:
        case M17_TYPE_ENCR_ED25519:
        case M17_TYPE_ENCR_CURVE25519:
            return true;
        default:
            return false;
    }
}

// Validate signature length
bool m17_validate_signature_length(size_t signature_length, int signature_type) {
    switch (signature_type) {
        case M17_ED25519_SIGNATURE:
            return signature_length == M17_ED25519_SIGNATURE_SIZE;
        case M17_ECDSA_SIGNATURE:
            return signature_length == 64; // 256-bit ECDSA signature
        default:
            return false;
    }
}

// Validate data length for encryption
bool m17_validate_encryption_data_length(size_t data_length, int cipher_type) {
    if (data_length == 0) {
        return false;
    }
    
    switch (cipher_type) {
        case M17_AES_CTR:
        case M17_AES_GCM:
            // AES can handle any length (with padding if needed)
            return data_length <= 65536; // Reasonable upper limit
        default:
            return false;
    }
}

// Comprehensive security validation for encryption operation
bool m17_validate_encryption_operation(const uint8_t *key, size_t key_length, 
                                      const uint8_t *iv, size_t iv_length,
                                      const uint8_t *data, size_t data_length,
                                      int cipher_type) {
    // Validate key
    if (!m17_validate_key_length(key_length, cipher_type)) {
        return false;
    }
    
    if (!m17_validate_key_material(key, key_length)) {
        return false;
    }
    
    // Validate IV
    if (!m17_validate_iv_length(iv_length, cipher_type)) {
        return false;
    }
    
    // Validate data
    if (!m17_validate_encryption_data_length(data_length, cipher_type)) {
        return false;
    }
    
    // Validate pointers
    if (key == NULL || iv == NULL || data == NULL) {
        return false;
    }
    
    return true;
}

// Validate decryption operation
bool m17_validate_decryption_operation(const uint8_t *key, size_t key_length,
                                      const uint8_t *iv, size_t iv_length,
                                      const uint8_t *ciphertext, size_t ciphertext_length,
                                      int cipher_type) {
    // Use same validation as encryption
    return m17_validate_encryption_operation(key, key_length, iv, iv_length, 
                                           ciphertext, ciphertext_length, cipher_type);
}

// Validate signature operation
bool m17_validate_signature_operation(const uint8_t *private_key, size_t private_key_length,
                                     const uint8_t *data, size_t data_length,
                                     int signature_type) {
    if (private_key == NULL || data == NULL) {
        return false;
    }
    
    if (data_length == 0) {
        return false;
    }
    
    if (!m17_validate_key_length(private_key_length, signature_type)) {
        return false;
    }
    
    if (!m17_validate_key_material(private_key, private_key_length)) {
        return false;
    }
    
    return true;
}

// Validate verification operation
bool m17_validate_verification_operation(const uint8_t *public_key, size_t public_key_length,
                                        const uint8_t *signature, size_t signature_length,
                                        const uint8_t *data, size_t data_length,
                                        int signature_type) {
    if (public_key == NULL || signature == NULL || data == NULL) {
        return false;
    }
    
    if (data_length == 0) {
        return false;
    }
    
    if (!m17_validate_key_length(public_key_length, signature_type)) {
        return false;
    }
    
    if (!m17_validate_signature_length(signature_length, signature_type)) {
        return false;
    }
    
    return true;
}
