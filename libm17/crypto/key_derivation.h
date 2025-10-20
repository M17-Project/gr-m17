//--------------------------------------------------------------------
// M17 C library - crypto/key_derivation.h
//
// Secure key derivation using HKDF with context information
// Implements proper session key derivation from ECDH shared secrets
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------

#ifndef M17_KEY_DERIVATION_H
#define M17_KEY_DERIVATION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Context structure for key derivation
typedef struct {
    uint8_t session_id[16];        // Unique session identifier
    uint64_t timestamp;            // Session timestamp
    uint8_t sender_callsign[9];   // Sender callsign
    uint8_t receiver_callsign[9]; // Receiver callsign
    uint16_t frame_number;        // Current frame number
    uint8_t key_type;             // Type of key to derive
} m17_key_context_t;

// Key types for derivation
#define M17_KEY_TYPE_ENCRYPTION    1
#define M17_KEY_TYPE_AUTHENTICATION 2
#define M17_KEY_TYPE_INTEGRITY     3
#define M17_KEY_TYPE_SESSION      4

// Create key derivation context
int m17_create_key_context(m17_key_context_t *ctx, 
                          const char *sender_callsign,
                          const char *receiver_callsign,
                          uint16_t frame_number,
                          uint8_t key_type);

// Derive session key from ECDH shared secret
int m17_derive_session_key(const uint8_t *shared_secret, size_t shared_secret_len,
                          const m17_key_context_t *context,
                          uint8_t *derived_key, size_t key_len);

// Derive multiple keys for a session
int m17_derive_session_keys(const uint8_t *shared_secret, size_t shared_secret_len,
                           const m17_key_context_t *context,
                           uint8_t *encryption_key, size_t enc_key_len,
                           uint8_t *authentication_key, size_t auth_key_len,
                           uint8_t *integrity_key, size_t int_key_len);

// Verify key derivation context
bool m17_verify_key_context(const m17_key_context_t *context);

// Secure key comparison (constant-time)
bool m17_secure_key_compare(const uint8_t *key1, const uint8_t *key2, size_t len);

// Secure key wiping
void m17_secure_key_wipe(uint8_t *key, size_t len);

#ifdef __cplusplus
}
#endif

#endif // M17_KEY_DERIVATION_H










