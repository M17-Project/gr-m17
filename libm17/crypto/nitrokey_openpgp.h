//--------------------------------------------------------------------
// M17 C library - crypto/nitrokey_openpgp.h
//
// Nitrokey OpenPGP integration for M17 digital radio protocol
// Provides hardware-based OpenPGP operations using Nitrokey devices
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 21 October 2025
//--------------------------------------------------------------------

#ifndef M17_NITROKEY_OPENPGP_H
#define M17_NITROKEY_OPENPGP_H

#include "openpgp_integration.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Nitrokey OpenPGP status codes
typedef enum {
    M17_NITROKEY_OPENPGP_SUCCESS = 0,
    M17_NITROKEY_OPENPGP_ERROR_INVALID_PARAM = -1,
    M17_NITROKEY_OPENPGP_ERROR_DEVICE_NOT_FOUND = -2,
    M17_NITROKEY_OPENPGP_ERROR_KEY_NOT_FOUND = -3,
    M17_NITROKEY_OPENPGP_ERROR_OPERATION_FAILED = -4,
    M17_NITROKEY_OPENPGP_ERROR_AUTHENTICATION_FAILED = -5,
    M17_NITROKEY_OPENPGP_ERROR_NITROPY_NOT_AVAILABLE = -6
} m17_nitrokey_openpgp_status_t;

// Nitrokey OpenPGP key information
typedef struct {
    char key_name[64];          // Nitrokey key name
    char key_id[17];           // OpenPGP key ID
    char fingerprint[41];      // OpenPGP fingerprint
    char user_id[256];         // User ID string
    bool is_ed25519;           // True if Ed25519 key
    bool is_rsa;               // True if RSA key
    uint32_t key_size;         // Key size in bits
    uint32_t creation_time;    // Key creation timestamp
} m17_nitrokey_openpgp_key_t;

// Initialize Nitrokey OpenPGP integration
m17_nitrokey_openpgp_status_t m17_nitrokey_openpgp_init(void);

// Check if Nitrokey is available and accessible
m17_nitrokey_openpgp_status_t m17_nitrokey_openpgp_check_device(void);

// List OpenPGP keys stored on Nitrokey
m17_nitrokey_openpgp_status_t m17_nitrokey_openpgp_list_keys(m17_nitrokey_openpgp_key_t* keys,
                                                           size_t max_keys,
                                                           size_t* key_count);

// Generate Ed25519 OpenPGP key on Nitrokey
m17_nitrokey_openpgp_status_t m17_nitrokey_openpgp_generate_ed25519_key(const char* key_name,
                                                                       const char* user_id,
                                                                       const char* passphrase);

// Generate RSA OpenPGP key on Nitrokey
m17_nitrokey_openpgp_status_t m17_nitrokey_openpgp_generate_rsa_key(const char* key_name,
                                                                   const char* user_id,
                                                                   const char* passphrase,
                                                                   uint32_t key_size);

// Export OpenPGP public key from Nitrokey
m17_nitrokey_openpgp_status_t m17_nitrokey_openpgp_export_public_key(const char* key_name,
                                                                    char* armored_key,
                                                                    size_t armored_key_size);

// Import OpenPGP public key to Nitrokey
m17_nitrokey_openpgp_status_t m17_nitrokey_openpgp_import_public_key(const char* key_name,
                                                                    const char* armored_key,
                                                                    size_t armored_key_size);

// Sign message using Nitrokey OpenPGP key
m17_nitrokey_openpgp_status_t m17_nitrokey_openpgp_sign_message(const char* message,
                                                              size_t message_len,
                                                              const char* key_name,
                                                              m17_openpgp_sig_type_t sig_type,
                                                              m17_openpgp_signature_t* signature);

// Sign email using Nitrokey OpenPGP key
m17_nitrokey_openpgp_status_t m17_nitrokey_openpgp_sign_email(const char* email_content,
                                                            size_t email_len,
                                                            const char* key_name,
                                                            m17_openpgp_sig_type_t sig_type,
                                                            m17_openpgp_signature_t* signature);

// Create detached signature using Nitrokey
m17_nitrokey_openpgp_status_t m17_nitrokey_openpgp_create_detached_signature(const char* file_path,
                                                                           const char* key_name,
                                                                           const char* output_path);

// Verify signature using Nitrokey public key
m17_nitrokey_openpgp_status_t m17_nitrokey_openpgp_verify_signature(const char* message,
                                                                   size_t message_len,
                                                                   const char* signature,
                                                                   size_t signature_len,
                                                                   const char* key_name,
                                                                   m17_openpgp_verification_t* verification);

// Set default Nitrokey OpenPGP key for operations
m17_nitrokey_openpgp_status_t m17_nitrokey_openpgp_set_default_key(const char* key_name);

// Get default Nitrokey OpenPGP key
m17_nitrokey_openpgp_status_t m17_nitrokey_openpgp_get_default_key(char* key_name, size_t key_name_size);

// Delete OpenPGP key from Nitrokey
m17_nitrokey_openpgp_status_t m17_nitrokey_openpgp_delete_key(const char* key_name);

// Get Nitrokey device information
m17_nitrokey_openpgp_status_t m17_nitrokey_openpgp_get_device_info(char* device_info, size_t info_size);

// Cleanup Nitrokey OpenPGP integration
void m17_nitrokey_openpgp_cleanup(void);

#endif // M17_NITROKEY_OPENPGP_H
