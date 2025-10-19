//--------------------------------------------------------------------
// M17 C library - crypto/chacha20_poly1305.h
//
// ChaCha20-Poly1305 authenticated encryption header for M17
// Based on RFC 8439 and optimized for M17 protocol
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------

#ifndef M17_CHACHA20_POLY1305_H
#define M17_CHACHA20_POLY1305_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ChaCha20-Poly1305 encryption
int m17_chacha20_poly1305_encrypt(const uint8_t* plaintext, size_t plaintext_len,
                                 const uint8_t* key, size_t key_size,
                                 const uint8_t* iv, size_t iv_size,
                                 const uint8_t* aad, size_t aad_len,
                                 uint8_t* ciphertext, size_t ciphertext_size,
                                 uint8_t* tag, size_t tag_size);

// ChaCha20-Poly1305 decryption
int m17_chacha20_poly1305_decrypt(const uint8_t* ciphertext, size_t ciphertext_len,
                                 const uint8_t* key, size_t key_size,
                                 const uint8_t* iv, size_t iv_size,
                                 const uint8_t* aad, size_t aad_len,
                                 const uint8_t* tag, size_t tag_size,
                                 uint8_t* plaintext, size_t plaintext_size);

// Generate cryptographically secure IV
int m17_chacha20_generate_iv(uint8_t* iv, size_t iv_size);

// Generate cryptographically secure key
int m17_chacha20_generate_key(uint8_t* key, size_t key_size);

// Derive key from shared secret using HKDF
int m17_chacha20_derive_key(const uint8_t* shared_secret, size_t secret_size,
                           const uint8_t* salt, size_t salt_size,
                           const uint8_t* info, size_t info_len,
                           uint8_t* derived_key, size_t key_size);

// Secure memory clearing
void m17_chacha20_secure_wipe(uint8_t* data, size_t size);

// Validation functions
int m17_chacha20_validate_key(const uint8_t* key, size_t key_size);
int m17_chacha20_validate_iv(const uint8_t* iv, size_t iv_size);

#ifdef __cplusplus
}
#endif

#endif // M17_CHACHA20_POLY1305_H
