//--------------------------------------------------------------------
// M17 C library - crypto/trustzone.h
//
// TrustZone Secure World interface for M17 cryptographic operations
// Provides hardware-enforced isolation for cryptographic functions
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------

#ifndef M17_TRUSTZONE_H
#define M17_TRUSTZONE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// TrustZone Secure World status codes
typedef enum {
    M17_TZ_SUCCESS = 0,
    M17_TZ_ERROR_INVALID_PARAM = -1,
    M17_TZ_ERROR_SECURE_WORLD_UNAVAILABLE = -2,
    M17_TZ_ERROR_KEY_STORAGE_FULL = -3,
    M17_TZ_ERROR_AUTHENTICATION_FAILED = -4,
    M17_TZ_ERROR_OPERATION_NOT_PERMITTED = -5,
    M17_TZ_ERROR_MEMORY_PROTECTION_VIOLATION = -6
} m17_tz_status_t;

// Secure World key types
typedef enum {
    M17_TZ_KEY_TYPE_ED25519_PRIVATE = 1,
    M17_TZ_KEY_TYPE_ED25519_PUBLIC = 2,
    M17_TZ_KEY_TYPE_CURVE25519_PRIVATE = 3,
    M17_TZ_KEY_TYPE_CURVE25519_PUBLIC = 4,
    M17_TZ_KEY_TYPE_SESSION_ENCRYPTION = 5,
    M17_TZ_KEY_TYPE_SESSION_AUTHENTICATION = 6
} m17_tz_key_type_t;

// Secure World operation types
typedef enum {
    M17_TZ_OP_GENERATE_KEYPAIR = 1,
    M17_TZ_OP_SIGN_DATA = 2,
    M17_TZ_OP_VERIFY_SIGNATURE = 3,
    M17_TZ_OP_ECDH_KEY_EXCHANGE = 4,
    M17_TZ_OP_ENCRYPT_DATA = 5,
    M17_TZ_OP_DECRYPT_DATA = 6,
    M17_TZ_OP_DERIVE_SESSION_KEY = 7,
    M17_TZ_OP_WIPE_KEY = 8
} m17_tz_operation_t;

// Secure World key handle (opaque)
typedef struct {
    uint32_t key_id;
    m17_tz_key_type_t key_type;
    uint32_t permissions;
    bool is_loaded;
} m17_tz_key_handle_t;

// Secure World session context
typedef struct {
    uint32_t session_id;
    uint32_t caller_id;
    uint64_t timestamp;
    uint32_t operation_count;
    bool is_authenticated;
} m17_tz_session_t;

// TrustZone Secure World initialization
m17_tz_status_t m17_tz_init(void);

// Secure World session management
m17_tz_status_t m17_tz_create_session(m17_tz_session_t *session);
m17_tz_status_t m17_tz_authenticate_session(m17_tz_session_t *session, 
                                           const char *credentials);
m17_tz_status_t m17_tz_close_session(m17_tz_session_t *session);

// Secure World key management
m17_tz_status_t m17_tz_generate_keypair(m17_tz_session_t *session,
                                        m17_tz_key_type_t key_type,
                                        m17_tz_key_handle_t *private_handle,
                                        m17_tz_key_handle_t *public_handle);

m17_tz_status_t m17_tz_load_key(m17_tz_session_t *session,
                                m17_tz_key_type_t key_type,
                                const uint8_t *key_data,
                                size_t key_size,
                                m17_tz_key_handle_t *handle);

m17_tz_status_t m17_tz_wipe_key(m17_tz_session_t *session,
                                m17_tz_key_handle_t *handle);

// Secure World cryptographic operations
m17_tz_status_t m17_tz_sign_data(m17_tz_session_t *session,
                                 m17_tz_key_handle_t *private_handle,
                                 const uint8_t *data,
                                 size_t data_size,
                                 uint8_t *signature,
                                 size_t *signature_size);

m17_tz_status_t m17_tz_verify_signature(m17_tz_session_t *session,
                                        m17_tz_key_handle_t *public_handle,
                                        const uint8_t *data,
                                        size_t data_size,
                                        const uint8_t *signature,
                                        size_t signature_size,
                                        bool *is_valid);

m17_tz_status_t m17_tz_ecdh_key_exchange(m17_tz_session_t *session,
                                         m17_tz_key_handle_t *private_handle,
                                         m17_tz_key_handle_t *peer_public_handle,
                                         m17_tz_key_handle_t *shared_secret_handle);

m17_tz_status_t m17_tz_encrypt_data(m17_tz_session_t *session,
                                    m17_tz_key_handle_t *encryption_handle,
                                    const uint8_t *plaintext,
                                    size_t plaintext_size,
                                    uint8_t *ciphertext,
                                    size_t *ciphertext_size,
                                    uint8_t *iv,
                                    size_t *iv_size);

m17_tz_status_t m17_tz_decrypt_data(m17_tz_session_t *session,
                                    m17_tz_key_handle_t *encryption_handle,
                                    const uint8_t *ciphertext,
                                    size_t ciphertext_size,
                                    const uint8_t *iv,
                                    size_t iv_size,
                                    uint8_t *plaintext,
                                    size_t *plaintext_size);

m17_tz_status_t m17_tz_derive_session_key(m17_tz_session_t *session,
                                          m17_tz_key_handle_t *shared_secret_handle,
                                          const char *context,
                                          m17_tz_key_handle_t *session_key_handle);

// Secure World memory protection
m17_tz_status_t m17_tz_secure_memory_alloc(size_t size, void **secure_ptr);
m17_tz_status_t m17_tz_secure_memory_free(void *secure_ptr);
m17_tz_status_t m17_tz_secure_memory_wipe(void *secure_ptr, size_t size);

// Secure World status and monitoring
m17_tz_status_t m17_tz_get_secure_world_status(void);
m17_tz_status_t m17_tz_get_key_count(m17_tz_session_t *session, uint32_t *count);
m17_tz_status_t m17_tz_get_operation_count(m17_tz_session_t *session, uint32_t *count);

// Secure World cleanup
m17_tz_status_t m17_tz_cleanup(void);

#endif // M17_TRUSTZONE_H
