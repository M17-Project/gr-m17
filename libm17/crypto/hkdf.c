//--------------------------------------------------------------------
// M17 C library - crypto/hkdf.c
//
// HKDF (HMAC-based Key Derivation Function) implementation for M17
// Based on RFC 5869 and optimized for M17 protocol
// SECURITY FIX: Uses OpenSSL for proper HKDF implementation
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------

#include "m17.h"
#include <string.h>
#include <stdint.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/kdf.h>
#include <openssl/err.h>

// HKDF constants
#define M17_HKDF_HASH_SIZE 32  // SHA-256 output size
#define M17_HKDF_BLOCK_SIZE 64  // SHA-256 block size

// SECURITY FIX: Use OpenSSL for proper HKDF implementation
// This provides real cryptographic security

// HKDF-Extract function using OpenSSL
static int hkdf_extract(const uint8_t *salt, size_t salt_len, 
                       const uint8_t *ikm, size_t ikm_len, 
                       uint8_t prk[M17_HKDF_HASH_SIZE]) {
    if (!ikm || ikm_len == 0 || !prk) {
        return -1;
    }
    
    // If no salt provided, use zero salt
    uint8_t zero_salt[M17_HKDF_BLOCK_SIZE] = {0};
    const uint8_t *actual_salt = salt;
    size_t actual_salt_len = salt_len;
    
    if (!salt || salt_len == 0) {
        actual_salt = zero_salt;
        actual_salt_len = M17_HKDF_BLOCK_SIZE;
    }
    
    // Use OpenSSL HMAC-SHA256
    unsigned int hmac_len = M17_HKDF_HASH_SIZE;
    if (HMAC(EVP_sha256(), actual_salt, actual_salt_len, ikm, ikm_len, prk, &hmac_len) == NULL) {
        return -1;
    }
    
    return 0;
}

// HKDF-Expand function using OpenSSL
static int hkdf_expand(const uint8_t *prk, size_t prk_len, 
                      const uint8_t *info, size_t info_len, 
                      uint8_t *okm, size_t okm_len) {
    if (!prk || !okm || okm_len == 0) {
        return -1;
    }
    
    if (okm_len > 255 * M17_HKDF_HASH_SIZE) {
        return -1; // Output length too long
    }
    
    uint8_t counter = 1;
    size_t offset = 0;
    
    while (offset < okm_len) {
        // T(i) = HMAC(PRK, T(i-1) | info | counter)
        uint8_t input[M17_HKDF_BLOCK_SIZE + M17_HKDF_INFO_SIZE + 1];
        size_t input_len = 0;
        
        // Add T(i-1) if not first iteration
        if (offset > 0) {
            memcpy(input + input_len, okm + offset - M17_HKDF_HASH_SIZE, M17_HKDF_HASH_SIZE);
            input_len += M17_HKDF_HASH_SIZE;
        }
        
        // Add info
        if (info && info_len > 0) {
            memcpy(input + input_len, info, info_len);
            input_len += info_len;
        }
        
        // Add counter
        input[input_len] = counter;
        input_len++;
        
        // HMAC-SHA256(PRK, input) using OpenSSL
        uint8_t hash[M17_HKDF_HASH_SIZE];
        unsigned int hmac_len = M17_HKDF_HASH_SIZE;
        if (HMAC(EVP_sha256(), prk, prk_len, input, input_len, hash, &hmac_len) == NULL) {
            return -1;
        }
        
        // Copy to output
        size_t copy_len = M17_HKDF_HASH_SIZE;
        if (offset + copy_len > okm_len) {
            copy_len = okm_len - offset;
        }
        memcpy(okm + offset, hash, copy_len);
        offset += copy_len;
        counter++;
    }
    
    return 0;
}

// Main HKDF function
int m17_hkdf_derive(const uint8_t* input_key_material, size_t ikm_len, 
                    const uint8_t* salt, size_t salt_len, 
                    const uint8_t* info, size_t info_len, 
                    uint8_t* output, size_t output_len) {
    if (!input_key_material || !output || output_len == 0) {
        return -1;
    }
    
    if (output_len > M17_HKDF_MAX_OUTPUT_SIZE) {
        return -1; // Output too long
    }
    
    // Step 1: Extract
    uint8_t prk[M17_HKDF_HASH_SIZE];
    if (hkdf_extract(salt, salt_len, input_key_material, ikm_len, prk) != 0) {
        return -1;
    }
    
    // Step 2: Expand
    if (hkdf_expand(prk, M17_HKDF_HASH_SIZE, info, info_len, output, output_len) != 0) {
        return -1;
    }
    
    return 0;
}




