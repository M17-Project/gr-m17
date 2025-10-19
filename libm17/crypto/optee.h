//--------------------------------------------------------------------
// M17 C library - crypto/optee.h
//
// OP-TEE (Open Portable TEE) integration for M17 cryptographic operations
// Provides Linux TEE integration for secure cryptographic functions
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------

#ifndef M17_OP_TEE_H
#define M17_OP_TEE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// OP-TEE status codes
typedef enum {
    M17_TEE_SUCCESS = 0,
    M17_TEE_ERROR_INVALID_PARAM = -1,
    M17_TEE_ERROR_TEE_UNAVAILABLE = -2,
    M17_TEE_ERROR_OUT_OF_MEMORY = -3,
    M17_TEE_ERROR_AUTHENTICATION_FAILED = -4,
    M17_TEE_ERROR_OPERATION_NOT_PERMITTED = -5,
    M17_TEE_ERROR_COMMUNICATION_FAILED = -6,
    M17_TEE_ERROR_KEY_NOT_FOUND = -7,
    M17_TEE_ERROR_SIGNATURE_INVALID = -8
} m17_tee_status_t;

// OP-TEE context handle
typedef struct {
    uint32_t context_id;
    bool is_connected;
    uint32_t session_count;
    uint64_t last_activity;
} m17_tee_context_t;

// OP-TEE session handle
typedef struct {
    uint32_t session_id;
    uint32_t context_id;
    bool is_authenticated;
    uint32_t operation_count;
    uint64_t created_timestamp;
} m17_tee_session_t;

// OP-TEE key handle
typedef struct {
    uint32_t key_id;
    uint32_t key_type;
    bool is_loaded;
    uint32_t permissions;
    uint64_t created_timestamp;
} m17_tee_key_handle_t;

// OP-TEE operation types
typedef enum {
    M17_TEE_OP_GENERATE_KEYPAIR = 0x1001,
    M17_TEE_OP_SIGN_DATA = 0x1002,
    M17_TEE_OP_VERIFY_SIGNATURE = 0x1003,
    M17_TEE_OP_ECDH_KEY_EXCHANGE = 0x1004,
    M17_TEE_OP_ENCRYPT_DATA = 0x1005,
    M17_TEE_OP_DECRYPT_DATA = 0x1006,
    M17_TEE_OP_DERIVE_SESSION_KEY = 0x1007,
    M17_TEE_OP_WIPE_KEY = 0x1008
} m17_tee_operation_t;

// OP-TEE key types
typedef enum {
    M17_TEE_KEY_TYPE_ED25519_PRIVATE = 0x2001,
    M17_TEE_KEY_TYPE_ED25519_PUBLIC = 0x2002,
    M17_TEE_KEY_TYPE_CURVE25519_PRIVATE = 0x2003,
    M17_TEE_KEY_TYPE_CURVE25519_PUBLIC = 0x2004,
    M17_TEE_KEY_TYPE_SESSION_ENCRYPTION = 0x2005,
    M17_TEE_KEY_TYPE_SESSION_AUTHENTICATION = 0x2006
} m17_tee_key_type_t;

// OP-TEE initialization and cleanup
m17_tee_status_t m17_tee_initialize(void);
m17_tee_status_t m17_tee_finalize(void);

// OP-TEE context management
m17_tee_status_t m17_tee_open_context(m17_tee_context_t *context);
m17_tee_status_t m17_tee_close_context(m17_tee_context_t *context);

// OP-TEE session management
m17_tee_status_t m17_tee_open_session(m17_tee_context_t *context,
                                     m17_tee_session_t *session,
                                     const char *ta_uuid);
m17_tee_status_t m17_tee_close_session(m17_tee_session_t *session);
m17_tee_status_t m17_tee_authenticate_session(m17_tee_session_t *session,
                                            const char *credentials);

// OP-TEE key management
m17_tee_status_t m17_tee_generate_keypair(m17_tee_session_t *session,
                                          m17_tee_key_type_t key_type,
                                          m17_tee_key_handle_t *private_handle,
                                          m17_tee_key_handle_t *public_handle);

m17_tee_status_t m17_tee_load_key(m17_tee_session_t *session,
                                  m17_tee_key_type_t key_type,
                                  const uint8_t *key_data,
                                  size_t key_size,
                                  m17_tee_key_handle_t *handle);

m17_tee_status_t m17_tee_wipe_key(m17_tee_session_t *session,
                                  m17_tee_key_handle_t *handle);

// OP-TEE cryptographic operations
m17_tee_status_t m17_tee_sign_data(m17_tee_session_t *session,
                                   m17_tee_key_handle_t *private_handle,
                                   const uint8_t *data,
                                   size_t data_size,
                                   uint8_t *signature,
                                   size_t *signature_size);

m17_tee_status_t m17_tee_verify_signature(m17_tee_session_t *session,
                                          m17_tee_key_handle_t *public_handle,
                                          const uint8_t *data,
                                          size_t data_size,
                                          const uint8_t *signature,
                                          size_t signature_size,
                                          bool *is_valid);

m17_tee_status_t m17_tee_ecdh_key_exchange(m17_tee_session_t *session,
                                           m17_tee_key_handle_t *private_handle,
                                           m17_tee_key_handle_t *peer_public_handle,
                                           m17_tee_key_handle_t *shared_secret_handle);

m17_tee_status_t m17_tee_encrypt_data(m17_tee_session_t *session,
                                      m17_tee_key_handle_t *encryption_handle,
                                      const uint8_t *plaintext,
                                      size_t plaintext_size,
                                      uint8_t *ciphertext,
                                      size_t *ciphertext_size,
                                      uint8_t *iv,
                                      size_t *iv_size);

m17_tee_status_t m17_tee_decrypt_data(m17_tee_session_t *session,
                                      m17_tee_key_handle_t *encryption_handle,
                                      const uint8_t *ciphertext,
                                      size_t ciphertext_size,
                                      const uint8_t *iv,
                                      size_t iv_size,
                                      uint8_t *plaintext,
                                      size_t *plaintext_size);

m17_tee_status_t m17_tee_derive_session_key(m17_tee_session_t *session,
                                            m17_tee_key_handle_t *shared_secret_handle,
                                            const char *context,
                                            m17_tee_key_handle_t *session_key_handle);

// OP-TEE secure memory management
m17_tee_status_t m17_tee_secure_memory_alloc(size_t size, void **secure_ptr);
m17_tee_status_t m17_tee_secure_memory_free(void *secure_ptr);
m17_tee_status_t m17_tee_secure_memory_wipe(void *secure_ptr, size_t size);

// OP-TEE status and monitoring
m17_tee_status_t m17_tee_get_status(void);
m17_tee_status_t m17_tee_get_session_info(m17_tee_session_t *session,
                                          uint32_t *operation_count,
                                          uint32_t *key_count);

// OP-TEE secure boot validation
m17_tee_status_t m17_tee_validate_secure_boot(void);
m17_tee_status_t m17_tee_get_secure_boot_status(bool *is_valid);

#endif // M17_OP_TEE_H
