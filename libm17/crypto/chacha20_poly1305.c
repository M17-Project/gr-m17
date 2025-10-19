//--------------------------------------------------------------------
// M17 C library - crypto/chacha20_poly1305.c
//
// ChaCha20-Poly1305 authenticated encryption implementation for M17
// Based on RFC 8439 and optimized for M17 protocol
// SECURITY FIX: Uses OpenSSL for proper ChaCha20-Poly1305 implementation
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------

#include "m17.h"
#include <string.h>
#include <stdint.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>

// ChaCha20-Poly1305 implementation using OpenSSL
// This provides real cryptographic security

// ChaCha20-Poly1305 encryption using OpenSSL
int m17_chacha20_poly1305_encrypt(const uint8_t* plaintext, size_t plaintext_len,
                                 const uint8_t* key, size_t key_size,
                                 const uint8_t* iv, size_t iv_size,
                                 const uint8_t* aad, size_t aad_len,
                                 uint8_t* ciphertext, size_t ciphertext_size,
                                 uint8_t* tag, size_t tag_size) {
    if (!plaintext || plaintext_len == 0 || !key || !iv || !ciphertext || !tag) {
        return -1;
    }
    
    if (key_size != 32 || iv_size != 12 || tag_size != 16) {
        return -1;
    }
    
    if (ciphertext_size < plaintext_len) {
        return -1;
    }
    
    EVP_CIPHER_CTX *ctx = NULL;
    int result = -1;
    int len = 0;
    int ciphertext_len = 0;
    
    // Create and initialize the cipher context
    ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return -1;
    }
    
    // Initialize the encryption operation with ChaCha20-Poly1305
    if (EVP_EncryptInit_ex(ctx, EVP_chacha20_poly1305(), NULL, NULL, NULL) != 1) {
        goto cleanup;
    }
    
    // Set the key and IV
    if (EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) != 1) {
        goto cleanup;
    }
    
    // Set additional authenticated data (AAD) if provided
    if (aad && aad_len > 0) {
        if (EVP_EncryptUpdate(ctx, NULL, &len, aad, aad_len) != 1) {
            goto cleanup;
        }
    }
    
    // Encrypt the plaintext
    if (EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len) != 1) {
        goto cleanup;
    }
    ciphertext_len = len;
    
    // Finalize the encryption
    if (EVP_EncryptFinal_ex(ctx, ciphertext + len, &len) != 1) {
        goto cleanup;
    }
    ciphertext_len += len;
    
    // Get the authentication tag
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, tag_size, tag) != 1) {
        goto cleanup;
    }
    
    result = ciphertext_len;
    
cleanup:
    if (ctx) {
        EVP_CIPHER_CTX_free(ctx);
    }
    return result;
}

// ChaCha20-Poly1305 decryption using OpenSSL
int m17_chacha20_poly1305_decrypt(const uint8_t* ciphertext, size_t ciphertext_len,
                                 const uint8_t* key, size_t key_size,
                                 const uint8_t* iv, size_t iv_size,
                                 const uint8_t* aad, size_t aad_len,
                                 const uint8_t* tag, size_t tag_size,
                                 uint8_t* plaintext, size_t plaintext_size) {
    if (!ciphertext || ciphertext_len == 0 || !key || !iv || !plaintext || !tag) {
        return -1;
    }
    
    if (key_size != 32 || iv_size != 12 || tag_size != 16) {
        return -1;
    }
    
    if (plaintext_size < ciphertext_len) {
        return -1;
    }
    
    EVP_CIPHER_CTX *ctx = NULL;
    int result = -1;
    int len = 0;
    int plaintext_len = 0;
    
    // Create and initialize the cipher context
    ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return -1;
    }
    
    // Initialize the decryption operation with ChaCha20-Poly1305
    if (EVP_DecryptInit_ex(ctx, EVP_chacha20_poly1305(), NULL, NULL, NULL) != 1) {
        goto cleanup;
    }
    
    // Set the key and IV
    if (EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv) != 1) {
        goto cleanup;
    }
    
    // Set additional authenticated data (AAD) if provided
    if (aad && aad_len > 0) {
        if (EVP_DecryptUpdate(ctx, NULL, &len, aad, aad_len) != 1) {
            goto cleanup;
        }
    }
    
    // Decrypt the ciphertext
    if (EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len) != 1) {
        goto cleanup;
    }
    plaintext_len = len;
    
    // Set the authentication tag
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, tag_size, (void*)tag) != 1) {
        goto cleanup;
    }
    
    // Finalize the decryption (this verifies the tag)
    if (EVP_DecryptFinal_ex(ctx, plaintext + len, &len) != 1) {
        goto cleanup;
    }
    plaintext_len += len;
    
    result = plaintext_len;
    
cleanup:
    if (ctx) {
        EVP_CIPHER_CTX_free(ctx);
    }
    return result;
}

// Generate cryptographically secure IV for ChaCha20
int m17_chacha20_generate_iv(uint8_t* iv, size_t iv_size) {
    if (!iv || iv_size != 12) {
        return -1;
    }
    
    // Use OpenSSL's cryptographically secure random number generator
    if (RAND_bytes(iv, iv_size) != 1) {
        return -1;
    }
    
    return 0;
}

// Generate cryptographically secure key for ChaCha20
int m17_chacha20_generate_key(uint8_t* key, size_t key_size) {
    if (!key || key_size != 32) {
        return -1;
    }
    
    // Use OpenSSL's cryptographically secure random number generator
    if (RAND_bytes(key, key_size) != 1) {
        return -1;
    }
    
    return 0;
}

// Derive ChaCha20 key from shared secret using HKDF
int m17_chacha20_derive_key(const uint8_t* shared_secret, size_t secret_size,
                           const uint8_t* salt, size_t salt_size,
                           const uint8_t* info, size_t info_len,
                           uint8_t* derived_key, size_t key_size) {
    if (!shared_secret || secret_size == 0 || !derived_key || key_size != 32) {
        return -1;
    }
    
    EVP_PKEY_CTX *pctx = NULL;
    int result = -1;
    
    // Create HKDF context
    pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, NULL);
    if (!pctx) {
        return -1;
    }
    
    if (EVP_PKEY_derive_init(pctx) <= 0) {
        goto cleanup;
    }
    
    // Set the input key material
    if (EVP_PKEY_CTX_set_hkdf_md(pctx, EVP_sha256()) <= 0) {
        goto cleanup;
    }
    
    if (EVP_PKEY_CTX_set1_hkdf_key(pctx, shared_secret, secret_size) <= 0) {
        goto cleanup;
    }
    
    // Set salt if provided
    if (salt && salt_size > 0) {
        if (EVP_PKEY_CTX_set1_hkdf_salt(pctx, salt, salt_size) <= 0) {
            goto cleanup;
        }
    }
    
    // Set info if provided
    if (info && info_len > 0) {
        if (EVP_PKEY_CTX_add1_hkdf_info(pctx, info, info_len) <= 0) {
            goto cleanup;
        }
    }
    
    // Derive the key
    if (EVP_PKEY_derive(pctx, derived_key, &key_size) <= 0) {
        goto cleanup;
    }
    
    result = 0;
    
cleanup:
    if (pctx) {
        EVP_PKEY_CTX_free(pctx);
    }
    return result;
}

// Secure memory clearing for ChaCha20 keys
void m17_chacha20_secure_wipe(uint8_t* data, size_t size) {
    if (data && size > 0) {
        explicit_bzero(data, size);
    }
}

// Validate ChaCha20 key format
int m17_chacha20_validate_key(const uint8_t* key, size_t key_size) {
    if (!key || key_size != 32) {
        return -1;
    }
    
    // Check for weak keys (all zeros, all ones, etc.)
    bool all_zero = true;
    bool all_one = true;
    
    for (size_t i = 0; i < key_size; i++) {
        if (key[i] != 0x00) all_zero = false;
        if (key[i] != 0xFF) all_one = false;
    }
    
    if (all_zero || all_one) {
        return -1;  // Weak key detected
    }
    
    return 0;
}

// Validate ChaCha20 IV format
int m17_chacha20_validate_iv(const uint8_t* iv, size_t iv_size) {
    if (!iv || iv_size != 12) {
        return -1;
    }
    
    // Check for weak IVs (all zeros, all ones, etc.)
    bool all_zero = true;
    bool all_one = true;
    
    for (size_t i = 0; i < iv_size; i++) {
        if (iv[i] != 0x00) all_zero = false;
        if (iv[i] != 0xFF) all_one = false;
    }
    
    if (all_zero || all_one) {
        return -1;  // Weak IV detected
    }
    
    return 0;
}
