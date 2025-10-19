//--------------------------------------------------------------------
// M17 C library - crypto/aes_gcm.c
//
// AES-GCM authenticated encryption implementation for M17
// Based on NIST SP 800-38D and optimized for M17 protocol
// SECURITY FIX: Uses OpenSSL for proper AES-GCM implementation
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------

#include "m17.h"
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <openssl/err.h>

// AES-GCM constants
#define M17_AES_GCM_BLOCK_SIZE 16
#define M17_AES_GCM_GHASH_SIZE 16

// SECURITY FIX: Use OpenSSL for proper AES-GCM implementation
// This provides real cryptographic security

// SECURITY FIX: Generate cryptographically secure IV using OpenSSL
static int generate_secure_iv(uint8_t iv[M17_AES_GCM_IV_SIZE]) {
    if (!iv) {
        return -1;
    }
    
    // Use OpenSSL's cryptographically secure random number generator
    if (RAND_bytes(iv, M17_AES_GCM_IV_SIZE) != 1) {
        return -1;
    }
    
    return 0;
}

// SECURITY FIX: Proper AES-GCM encryption using OpenSSL
int m17_aes_gcm_encrypt(const uint8_t* plaintext, size_t plaintext_len, 
                        const uint8_t key[M17_AES_GCM_KEY_SIZE], 
                        const uint8_t iv[M17_AES_GCM_IV_SIZE], 
                        uint8_t* ciphertext, uint8_t tag[M17_AES_GCM_TAG_SIZE]) {
    if (!plaintext || !key || !iv || !ciphertext || !tag) {
        return -1;
    }
    
    if (plaintext_len == 0) {
        return -1;
    }
    
    EVP_CIPHER_CTX *ctx = NULL;
    int result = -1;
    int len = 0;
    int final_len = 0;
    
    // Create and initialize the cipher context
    ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        goto cleanup;
    }
    
    // Initialize the encryption operation
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) {
        goto cleanup;
    }
    
    // Set the key and IV
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, M17_AES_GCM_IV_SIZE, NULL) != 1) {
        goto cleanup;
    }
    
    if (EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) != 1) {
        goto cleanup;
    }
    
    // Encrypt the plaintext
    if (EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len) != 1) {
        goto cleanup;
    }
    
    // Finalize the encryption
    if (EVP_EncryptFinal_ex(ctx, ciphertext + len, &final_len) != 1) {
        goto cleanup;
    }
    
    // Get the authentication tag
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, M17_AES_GCM_TAG_SIZE, tag) != 1) {
        goto cleanup;
    }
    
    result = 0;
    
cleanup:
    if (ctx) EVP_CIPHER_CTX_free(ctx);
    return result;
}

// SECURITY FIX: Proper AES-GCM decryption using OpenSSL
int m17_aes_gcm_decrypt(const uint8_t* ciphertext, size_t ciphertext_len, 
                        const uint8_t key[M17_AES_GCM_KEY_SIZE], 
                        const uint8_t iv[M17_AES_GCM_IV_SIZE], 
                        const uint8_t tag[M17_AES_GCM_TAG_SIZE], 
                        uint8_t* plaintext) {
    if (!ciphertext || !key || !iv || !tag || !plaintext) {
        return -1;
    }
    
    if (ciphertext_len == 0) {
        return -1;
    }
    
    EVP_CIPHER_CTX *ctx = NULL;
    int result = -1;
    int len = 0;
    int final_len = 0;
    
    // Create and initialize the cipher context
    ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        goto cleanup;
    }
    
    // Initialize the decryption operation
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) {
        goto cleanup;
    }
    
    // Set the key and IV
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, M17_AES_GCM_IV_SIZE, NULL) != 1) {
        goto cleanup;
    }
    
    if (EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv) != 1) {
        goto cleanup;
    }
    
    // Decrypt the ciphertext
    if (EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len) != 1) {
        goto cleanup;
    }
    
    // Set the authentication tag
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, M17_AES_GCM_TAG_SIZE, (void*)tag) != 1) {
        goto cleanup;
    }
    
    // Finalize the decryption
    if (EVP_DecryptFinal_ex(ctx, plaintext + len, &final_len) != 1) {
        goto cleanup;
    }
    
    result = 0;
    
cleanup:
    if (ctx) EVP_CIPHER_CTX_free(ctx);
    return result;
}
