//--------------------------------------------------------------------
// M17 C library - crypto/openpgp_integration.h
//
// OpenPGP integration for M17 digital radio protocol
// Provides GnuPG/OpenPGP integration for message and email signing
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 21 October 2025
//--------------------------------------------------------------------

#ifndef M17_OPENPGP_INTEGRATION_H
#define M17_OPENPGP_INTEGRATION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// OpenPGP integration status codes
typedef enum {
    M17_OPENPGP_SUCCESS = 0,
    M17_OPENPGP_ERROR_INVALID_PARAM = -1,
    M17_OPENPGP_ERROR_GPG_NOT_FOUND = -2,
    M17_OPENPGP_ERROR_KEY_NOT_FOUND = -3,
    M17_OPENPGP_ERROR_SIGNATURE_FAILED = -4,
    M17_OPENPGP_ERROR_VERIFICATION_FAILED = -5,
    M17_OPENPGP_ERROR_NITROKEY_NOT_AVAILABLE = -6,
    M17_OPENPGP_ERROR_OPERATION_FAILED = -7
} m17_openpgp_status_t;

// OpenPGP signature types
typedef enum {
    M17_OPENPGP_SIG_BINARY = 0x00,      // Binary signature
    M17_OPENPGP_SIG_TEXT = 0x01,       // Text signature (canonical)
    M17_OPENPGP_SIG_STANDALONE = 0x02,  // Standalone signature
    M17_OPENPGP_SIG_CERT_GENERIC = 0x10, // Generic certification
    M17_OPENPGP_SIG_CERT_PERSONA = 0x11, // Persona certification
    M17_OPENPGP_SIG_CERT_CASUAL = 0x12,  // Casual certification
    M17_OPENPGP_SIG_CERT_POSITIVE = 0x13 // Positive certification
} m17_openpgp_sig_type_t;

// OpenPGP key information
typedef struct {
    char key_id[17];           // 16-character key ID + null terminator
    char fingerprint[41];      // 40-character fingerprint + null terminator
    char user_id[256];         // User ID string
    bool is_secret;            // True if secret key available
    bool is_nitrokey;          // True if key is on Nitrokey
    uint32_t creation_time;    // Key creation timestamp
    uint32_t expiration_time;  // Key expiration timestamp (0 = no expiration)
} m17_openpgp_key_info_t;

// OpenPGP signature result
typedef struct {
    char signature_armored[8192];  // ASCII-armored signature
    size_t signature_size;         // Size of signature data
    char key_id[17];               // Key ID used for signing
    uint32_t creation_time;        // Signature creation time
    m17_openpgp_sig_type_t sig_type; // Signature type
} m17_openpgp_signature_t;

// OpenPGP verification result
typedef struct {
    bool is_valid;              // True if signature is valid
    char key_id[17];            // Key ID of signing key
    char fingerprint[41];        // Fingerprint of signing key
    char user_id[256];          // User ID of signing key
    uint32_t creation_time;     // Signature creation time
    m17_openpgp_sig_type_t sig_type; // Signature type
    char error_message[256];    // Error message if verification failed
} m17_openpgp_verification_t;

// OpenPGP integration initialization
m17_openpgp_status_t m17_openpgp_init(void);

// Check if GnuPG is available and properly configured
m17_openpgp_status_t m17_openpgp_check_gpg_availability(void);

// List available OpenPGP keys
m17_openpgp_status_t m17_openpgp_list_keys(m17_openpgp_key_info_t* keys, 
                                          size_t max_keys, 
                                          size_t* key_count);

// Import key from Nitrokey to GnuPG keyring
m17_openpgp_status_t m17_openpgp_import_nitrokey_key(const char* nitrokey_key_name,
                                                    const char* gpg_key_id);

// Sign message with OpenPGP
m17_openpgp_status_t m17_openpgp_sign_message(const char* message,
                                             size_t message_len,
                                             const char* key_id,
                                             m17_openpgp_sig_type_t sig_type,
                                             m17_openpgp_signature_t* signature);

// Sign email with OpenPGP
m17_openpgp_status_t m17_openpgp_sign_email(const char* email_content,
                                           size_t email_len,
                                           const char* key_id,
                                           m17_openpgp_sig_type_t sig_type,
                                           m17_openpgp_signature_t* signature);

// Verify OpenPGP signature
m17_openpgp_status_t m17_openpgp_verify_signature(const char* message,
                                                size_t message_len,
                                                const char* signature,
                                                size_t signature_len,
                                                m17_openpgp_verification_t* verification);

// Create detached signature for file
m17_openpgp_status_t m17_openpgp_create_detached_signature(const char* file_path,
                                                         const char* key_id,
                                                         const char* output_path);

// Verify detached signature
m17_openpgp_status_t m17_openpgp_verify_detached_signature(const char* file_path,
                                                        const char* signature_path,
                                                        m17_openpgp_verification_t* verification);

// Generate OpenPGP keypair
m17_openpgp_status_t m17_openpgp_generate_keypair(const char* name,
                                                 const char* email,
                                                 const char* comment,
                                                 const char* passphrase,
                                                 uint32_t key_size,
                                                 uint32_t expiration_days);

// Export public key in ASCII-armored format
m17_openpgp_status_t m17_openpgp_export_public_key(const char* key_id,
                                                  char* armored_key,
                                                  size_t armored_key_size);

// Import public key from ASCII-armored format
m17_openpgp_status_t m17_openpgp_import_public_key(const char* armored_key,
                                                  size_t armored_key_size);

// Set default signing key
m17_openpgp_status_t m17_openpgp_set_default_key(const char* key_id);

// Get default signing key
m17_openpgp_status_t m17_openpgp_get_default_key(char* key_id, size_t key_id_size);

// Cleanup OpenPGP integration
void m17_openpgp_cleanup(void);

#endif // M17_OPENPGP_INTEGRATION_H
