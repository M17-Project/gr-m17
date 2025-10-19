//--------------------------------------------------------------------
// M17 C library - crypto/ed25519.c
//
// Ed25519 digital signature implementation for M17
// Based on RFC 8032 and optimized for M17 protocol
// SECURITY FIX: Uses OpenSSL for proper cryptographic implementation
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------

#include "m17.h"
#include <string.h>
#include <stdint.h>
#include <openssl/evp.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/err.h>

// Ed25519 implementation using OpenSSL
// This provides real cryptographic security

// Ed25519 key generation using OpenSSL
int m17_ed25519_generate_keypair(uint8_t public_key[M17_ED25519_PUBLIC_KEY_SIZE], 
                                 uint8_t private_key[M17_ED25519_PRIVATE_KEY_SIZE]) {
    if (!public_key || !private_key) {
        return -1;
    }
    
    EVP_PKEY_CTX *pkey_ctx = NULL;
    EVP_PKEY *pkey = NULL;
    int result = -1;
    
    // Create Ed25519 key generation context
    pkey_ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);
    if (!pkey_ctx) {
        goto cleanup;
    }
    
    if (EVP_PKEY_keygen_init(pkey_ctx) <= 0) {
        goto cleanup;
    }
    
    // Generate the key pair
    if (EVP_PKEY_keygen(pkey_ctx, &pkey) <= 0) {
        goto cleanup;
    }
    
    // Extract private key
    size_t private_len = M17_ED25519_PRIVATE_KEY_SIZE;
    if (EVP_PKEY_get_raw_private_key(pkey, private_key, &private_len) <= 0) {
        goto cleanup;
    }
    
    // Extract public key
    size_t public_len = M17_ED25519_PUBLIC_KEY_SIZE;
    if (EVP_PKEY_get_raw_public_key(pkey, public_key, &public_len) <= 0) {
        goto cleanup;
    }
    
    result = 0;
    
cleanup:
    if (pkey) EVP_PKEY_free(pkey);
    if (pkey_ctx) EVP_PKEY_CTX_free(pkey_ctx);
    return result;
}

// Derive public key from private key using OpenSSL
int m17_ed25519_public_key_from_private(const uint8_t private_key[M17_ED25519_PRIVATE_KEY_SIZE], 
                                        uint8_t public_key[M17_ED25519_PUBLIC_KEY_SIZE]) {
    if (!private_key || !public_key) {
        return -1;
    }
    
    EVP_PKEY *pkey = NULL;
    int result = -1;
    
    // Create EVP_PKEY from raw private key
    pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL, private_key, M17_ED25519_PRIVATE_KEY_SIZE);
    if (!pkey) {
        goto cleanup;
    }
    
    // Extract public key
    size_t public_len = M17_ED25519_PUBLIC_KEY_SIZE;
    if (EVP_PKEY_get_raw_public_key(pkey, public_key, &public_len) <= 0) {
        goto cleanup;
    }
    
    result = 0;
    
cleanup:
    if (pkey) EVP_PKEY_free(pkey);
    return result;
}

// Ed25519 signature generation using OpenSSL
int m17_ed25519_sign(const uint8_t* message, size_t message_len, 
                     const uint8_t private_key[M17_ED25519_PRIVATE_KEY_SIZE], 
                     uint8_t signature[M17_ED25519_SIGNATURE_SIZE]) {
    if (!message || !private_key || !signature) {
        return -1;
    }
    
    if (message_len == 0) {
        return -1;
    }
    
    EVP_PKEY *pkey = NULL;
    EVP_MD_CTX *md_ctx = NULL;
    int result = -1;
    
    // Create EVP_PKEY from raw private key
    pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL, private_key, M17_ED25519_PRIVATE_KEY_SIZE);
    if (!pkey) {
        goto cleanup;
    }
    
    // Create message digest context
    md_ctx = EVP_MD_CTX_new();
    if (!md_ctx) {
        goto cleanup;
    }
    
    // Initialize signing
    if (EVP_DigestSignInit(md_ctx, NULL, NULL, NULL, pkey) <= 0) {
        goto cleanup;
    }
    
    // Sign the message
    size_t sig_len = M17_ED25519_SIGNATURE_SIZE;
    if (EVP_DigestSign(md_ctx, signature, &sig_len, message, message_len) <= 0) {
        goto cleanup;
    }
    
    result = 0;
    
cleanup:
    if (md_ctx) EVP_MD_CTX_free(md_ctx);
    if (pkey) EVP_PKEY_free(pkey);
    return result;
}

// Ed25519 signature verification using OpenSSL
int m17_ed25519_verify(const uint8_t* message, size_t message_len, 
                        const uint8_t signature[M17_ED25519_SIGNATURE_SIZE], 
                        const uint8_t public_key[M17_ED25519_PUBLIC_KEY_SIZE]) {
    if (!message || !signature || !public_key) {
        return -1;
    }
    
    if (message_len == 0) {
        return -1;
    }
    
    EVP_PKEY *pkey = NULL;
    EVP_MD_CTX *md_ctx = NULL;
    int result = -1;
    
    // Create EVP_PKEY from raw public key
    pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL, public_key, M17_ED25519_PUBLIC_KEY_SIZE);
    if (!pkey) {
        goto cleanup;
    }
    
    // Create message digest context
    md_ctx = EVP_MD_CTX_new();
    if (!md_ctx) {
        goto cleanup;
    }
    
    // Initialize verification
    if (EVP_DigestVerifyInit(md_ctx, NULL, NULL, NULL, pkey) <= 0) {
        goto cleanup;
    }
    
    // Verify the signature
    if (EVP_DigestVerify(md_ctx, signature, M17_ED25519_SIGNATURE_SIZE, message, message_len) <= 0) {
        result = -1; // Invalid signature
    } else {
        result = 0; // Valid signature
    }
    
cleanup:
    if (md_ctx) EVP_MD_CTX_free(md_ctx);
    if (pkey) EVP_PKEY_free(pkey);
    return result;
}
