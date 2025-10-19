//--------------------------------------------------------------------
// M17 C library - crypto/curve25519.c
//
// Curve25519 ECDH implementation for M17
// Based on RFC 7748 and optimized for M17 protocol
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
#include <openssl/obj_mac.h>
#include <openssl/rand.h>
#include <openssl/err.h>

// Curve25519 implementation using OpenSSL
// This provides real cryptographic security

// Curve25519 key generation using OpenSSL
int m17_curve25519_generate_keypair(uint8_t public_key[M17_CURVE25519_PUBLIC_KEY_SIZE], 
                                    uint8_t private_key[M17_CURVE25519_PRIVATE_KEY_SIZE]) {
    if (!public_key || !private_key) {
        return -1;
    }
    
    EVP_PKEY_CTX *pkey_ctx = NULL;
    EVP_PKEY *pkey = NULL;
    int result = -1;
    
    // Create X25519 key generation context
    pkey_ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, NULL);
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
    size_t private_len = M17_CURVE25519_PRIVATE_KEY_SIZE;
    if (EVP_PKEY_get_raw_private_key(pkey, private_key, &private_len) <= 0) {
        goto cleanup;
    }
    
    // Extract public key
    size_t public_len = M17_CURVE25519_PUBLIC_KEY_SIZE;
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
int m17_curve25519_public_key_from_private(const uint8_t private_key[M17_CURVE25519_PRIVATE_KEY_SIZE], 
                                           uint8_t public_key[M17_CURVE25519_PUBLIC_KEY_SIZE]) {
    if (!private_key || !public_key) {
        return -1;
    }
    
    EVP_PKEY *pkey = NULL;
    int result = -1;
    
    // Create EVP_PKEY from raw private key
    pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, NULL, private_key, M17_CURVE25519_PRIVATE_KEY_SIZE);
    if (!pkey) {
        goto cleanup;
    }
    
    // Extract public key
    size_t public_len = M17_CURVE25519_PUBLIC_KEY_SIZE;
    if (EVP_PKEY_get_raw_public_key(pkey, public_key, &public_len) <= 0) {
        goto cleanup;
    }
    
    result = 0;
    
cleanup:
    if (pkey) EVP_PKEY_free(pkey);
    return result;
}

// Curve25519 ECDH key exchange using OpenSSL
int m17_curve25519_ecdh(const uint8_t private_key[M17_CURVE25519_PRIVATE_KEY_SIZE], 
                         const uint8_t peer_public_key[M17_CURVE25519_PUBLIC_KEY_SIZE], 
                         uint8_t shared_secret[M17_CURVE25519_SHARED_SECRET_SIZE]) {
    if (!private_key || !peer_public_key || !shared_secret) {
        return -1;
    }
    
    EVP_PKEY *private_pkey = NULL;
    EVP_PKEY *peer_pkey = NULL;
    EVP_PKEY_CTX *ctx = NULL;
    int result = -1;
    
    // Create private key EVP_PKEY
    private_pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, NULL, private_key, M17_CURVE25519_PRIVATE_KEY_SIZE);
    if (!private_pkey) {
        goto cleanup;
    }
    
    // Create peer public key EVP_PKEY
    peer_pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, NULL, peer_public_key, M17_CURVE25519_PUBLIC_KEY_SIZE);
    if (!peer_pkey) {
        goto cleanup;
    }
    
    // Create ECDH context
    ctx = EVP_PKEY_CTX_new(private_pkey, NULL);
    if (!ctx) {
        goto cleanup;
    }
    
    if (EVP_PKEY_derive_init(ctx) <= 0) {
        goto cleanup;
    }
    
    if (EVP_PKEY_derive_set_peer(ctx, peer_pkey) <= 0) {
        goto cleanup;
    }
    
    // Derive shared secret
    size_t secret_len = M17_CURVE25519_SHARED_SECRET_SIZE;
    if (EVP_PKEY_derive(ctx, shared_secret, &secret_len) <= 0) {
        goto cleanup;
    }
    
    result = 0;
    
cleanup:
    if (ctx) EVP_PKEY_CTX_free(ctx);
    if (peer_pkey) EVP_PKEY_free(peer_pkey);
    if (private_pkey) EVP_PKEY_free(private_pkey);
    return result;
}
