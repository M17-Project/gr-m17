# OpenPGP Integration for M17 Digital Radio

## Overview

The M17 GNU Radio module now includes comprehensive OpenPGP integration, providing secure message and email signing capabilities using both software-based GnuPG and hardware-based Nitrokey devices. This integration enables secure communication and authentication for M17 digital radio operations.

## Features

### Software-Based OpenPGP (GnuPG Integration)
- **Message Signing**: Sign M17 messages with OpenPGP signatures
- **Email Signing**: Sign emails with OpenPGP for secure communication
- **Key Management**: Generate, import, and manage OpenPGP keys
- **Signature Verification**: Verify OpenPGP signatures
- **Detached Signatures**: Create and verify detached signatures for files

### Hardware-Based OpenPGP (Nitrokey Integration)
- **Hardware Key Storage**: Store OpenPGP keys securely on Nitrokey devices
- **Hardware Signing**: Perform cryptographic operations in hardware
- **Key Generation**: Generate Ed25519 and RSA keys directly on Nitrokey
- **Secure Operations**: Private keys never leave the hardware device
- **Multiple Key Support**: Store and manage multiple keys on a single device

## Installation Requirements

### Software Dependencies
```bash
# Install GnuPG
sudo apt-get install gnupg2

# Install Nitrokey tools (optional, for hardware support)
pip install pynitrokey
```

### Hardware Requirements (Optional)
- **Nitrokey 3**: Latest generation with USB-C and NFC support
- **Nitrokey Start**: Open source hardware with FIDO2 support
- **Nitrokey Pro 2**: High-security model with smart card functionality

## API Reference

### OpenPGP Integration Functions

#### Initialization
```c
// Initialize OpenPGP integration
m17_openpgp_status_t m17_openpgp_init(void);

// Check GnuPG availability
m17_openpgp_status_t m17_openpgp_check_gpg_availability(void);
```

#### Key Management
```c
// List available OpenPGP keys
m17_openpgp_status_t m17_openpgp_list_keys(m17_openpgp_key_info_t* keys, 
                                          size_t max_keys, 
                                          size_t* key_count);

// Generate OpenPGP keypair
m17_openpgp_status_t m17_openpgp_generate_keypair(const char* name,
                                                 const char* email,
                                                 const char* comment,
                                                 const char* passphrase,
                                                 uint32_t key_size,
                                                 uint32_t expiration_days);

// Export public key
m17_openpgp_status_t m17_openpgp_export_public_key(const char* key_id,
                                                  char* armored_key,
                                                  size_t armored_key_size);

// Import public key
m17_openpgp_status_t m17_openpgp_import_public_key(const char* armored_key,
                                                  size_t armored_key_size);
```

#### Message Signing
```c
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
```

#### Signature Verification
```c
// Verify OpenPGP signature
m17_openpgp_status_t m17_openpgp_verify_signature(const char* message,
                                                size_t message_len,
                                                const char* signature,
                                                size_t signature_len,
                                                m17_openpgp_verification_t* verification);
```

### Nitrokey OpenPGP Integration Functions

#### Initialization
```c
// Initialize Nitrokey OpenPGP integration
m17_nitrokey_openpgp_status_t m17_nitrokey_openpgp_init(void);

// Check Nitrokey device availability
m17_nitrokey_openpgp_status_t m17_nitrokey_openpgp_check_device(void);
```

#### Hardware Key Management
```c
// List OpenPGP keys on Nitrokey
m17_nitrokey_openpgp_status_t m17_nitrokey_openpgp_list_keys(m17_nitrokey_openpgp_key_t* keys,
                                                           size_t max_keys,
                                                           size_t* key_count);

// Generate Ed25519 key on Nitrokey
m17_nitrokey_openpgp_status_t m17_nitrokey_openpgp_generate_ed25519_key(const char* key_name,
                                                                       const char* user_id,
                                                                       const char* passphrase);

// Generate RSA key on Nitrokey
m17_nitrokey_openpgp_status_t m17_nitrokey_openpgp_generate_rsa_key(const char* key_name,
                                                                   const char* user_id,
                                                                   const char* passphrase,
                                                                   uint32_t key_size);
```

#### Hardware Signing
```c
// Sign message using Nitrokey
m17_nitrokey_openpgp_status_t m17_nitrokey_openpgp_sign_message(const char* message,
                                                              size_t message_len,
                                                              const char* key_name,
                                                              m17_openpgp_sig_type_t sig_type,
                                                              m17_openpgp_signature_t* signature);

// Sign email using Nitrokey
m17_nitrokey_openpgp_status_t m17_nitrokey_openpgp_sign_email(const char* email_content,
                                                            size_t email_len,
                                                            const char* key_name,
                                                            m17_openpgp_sig_type_t sig_type,
                                                            m17_openpgp_signature_t* signature);
```

## Usage Examples

### Basic Message Signing

```c
#include "openpgp_integration.h"

int main() {
    // Initialize OpenPGP integration
    m17_openpgp_status_t status = m17_openpgp_init();
    if (status != M17_OPENPGP_SUCCESS) {
        printf("Failed to initialize OpenPGP\n");
        return 1;
    }
    
    // List available keys
    m17_openpgp_key_info_t keys[10];
    size_t key_count = 0;
    status = m17_openpgp_list_keys(keys, 10, &key_count);
    if (status != M17_OPENPGP_SUCCESS) {
        printf("Failed to list keys\n");
        return 1;
    }
    
    // Sign a message
    const char* message = "Hello, M17 World!";
    m17_openpgp_signature_t signature;
    status = m17_openpgp_sign_message(message, strlen(message), 
                                   keys[0].key_id, M17_OPENPGP_SIG_TEXT, &signature);
    if (status == M17_OPENPGP_SUCCESS) {
        printf("Message signed successfully!\n");
        printf("Signature: %s\n", signature.signature_armored);
    }
    
    // Cleanup
    m17_openpgp_cleanup();
    return 0;
}
```

### Email Signing

```c
#include "openpgp_integration.h"

int main() {
    m17_openpgp_init();
    
    // Email content
    const char* email = "From: sender@example.com\n"
                       "To: recipient@example.com\n"
                       "Subject: Secure M17 Communication\n\n"
                       "This is a secure message signed with OpenPGP.";
    
    // Sign email
    m17_openpgp_signature_t signature;
    m17_openpgp_status_t status = m17_openpgp_sign_email(email, strlen(email), 
                                                        "your-key-id", 
                                                        M17_OPENPGP_SIG_TEXT, 
                                                        &signature);
    if (status == M17_OPENPGP_SUCCESS) {
        printf("Email signed successfully!\n");
        printf("Signature: %s\n", signature.signature_armored);
    }
    
    m17_openpgp_cleanup();
    return 0;
}
```

### Nitrokey Hardware Signing

```c
#include "nitrokey_openpgp.h"

int main() {
    // Initialize Nitrokey OpenPGP integration
    m17_nitrokey_openpgp_status_t status = m17_nitrokey_openpgp_init();
    if (status != M17_NITROKEY_OPENPGP_SUCCESS) {
        printf("Nitrokey not available\n");
        return 1;
    }
    
    // Generate Ed25519 key on Nitrokey
    status = m17_nitrokey_openpgp_generate_ed25519_key("m17-radio-key", 
                                                     "M17 Radio <radio@example.com>", 
                                                     NULL);
    if (status != M17_NITROKEY_OPENPGP_SUCCESS) {
        printf("Failed to generate key on Nitrokey\n");
        return 1;
    }
    
    // Sign message using Nitrokey
    const char* message = "Secure M17 message";
    m17_openpgp_signature_t signature;
    status = m17_nitrokey_openpgp_sign_message(message, strlen(message), 
                                             "m17-radio-key", 
                                             M17_OPENPGP_SIG_TEXT, 
                                             &signature);
    if (status == M17_NITROKEY_OPENPGP_SUCCESS) {
        printf("Message signed with Nitrokey!\n");
        printf("Signature: %s\n", signature.signature_armored);
    }
    
    m17_nitrokey_openpgp_cleanup();
    return 0;
}
```

## Security Considerations

### Key Management
- **Private Key Protection**: When using Nitrokey, private keys never leave the hardware device
- **Key Backup**: Always backup your OpenPGP keys in a secure location
- **Key Rotation**: Regularly rotate your OpenPGP keys for enhanced security
- **Passphrase Security**: Use strong passphrases for key protection

### Signature Types
- **Binary Signatures**: For binary data and files
- **Text Signatures**: For text messages and emails (canonical format)
- **Detached Signatures**: For file integrity verification

### Best Practices
- **Verify Signatures**: Always verify signatures before trusting messages
- **Key Validation**: Validate public keys through trusted channels
- **Secure Storage**: Store private keys securely (preferably on hardware)
- **Regular Updates**: Keep GnuPG and Nitrokey firmware updated

## Troubleshooting

### Common Issues

#### GnuPG Not Found
```
Error: M17_OPENPGP_ERROR_GPG_NOT_FOUND
```
**Solution**: Install GnuPG2:
```bash
sudo apt-get install gnupg2
```

#### No Keys Available
```
Error: No keys found for signing
```
**Solution**: Generate or import OpenPGP keys:
```bash
gpg --gen-key
```

#### Nitrokey Not Detected
```
Error: M17_NITROKEY_OPENPGP_ERROR_DEVICE_NOT_FOUND
```
**Solution**: 
1. Ensure Nitrokey is connected
2. Install nitropy: `pip install pynitrokey`
3. Check device: `nitropy nk3 list`

#### Permission Denied
```
Error: Permission denied for GnuPG operations
```
**Solution**: Fix GnuPG directory permissions:
```bash
chmod 700 ~/.gnupg
chmod 600 ~/.gnupg/*
```

## Testing

Run the OpenPGP integration test suite:

```bash
cd /home/haaken/github-projects/gr-m17/libm17/crypto
gcc -o test_openpgp test_openpgp_integration.c openpgp_integration.c nitrokey_openpgp.c -lm17
./test_openpgp
```

## Legal Disclaimer

**IMPORTANT: Encryption of radio amateur signals is illegal in many countries.**

**User Responsibility:**
- Users are entirely responsible for compliance with local laws and regulations
- Check your local regulations before using OpenPGP encryption features
- Some countries prohibit encryption of amateur radio communications
- Penalties may apply for non-compliance with local laws

**Recommendation:**
Always consult with your local amateur radio regulatory authority before using OpenPGP encryption features on amateur radio frequencies.

## Support

For technical support and questions about OpenPGP integration:
- **M17 Project**: https://m17project.org
- **Documentation**: See `docs/` directory for detailed technical documentation
- **Issues**: Report issues on the project repository
- **Community**: Join the M17 community for discussions and support
