//--------------------------------------------------------------------
// M17 OpenPGP Integration Example
//
// Demonstrates OpenPGP integration with M17 digital radio protocol
// Shows message signing, email signing, and Nitrokey hardware integration
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 21 October 2025
//--------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// Include M17 OpenPGP integration headers
#include "crypto/openpgp_integration.h"
#include "crypto/nitrokey_openpgp.h"

// Example configuration
#define MAX_MESSAGE_SIZE 1024
#define MAX_EMAIL_SIZE 2048
#define MAX_SIGNATURE_SIZE 8192

// Function prototypes
void print_usage(const char* program_name);
void demonstrate_software_openpgp(void);
void demonstrate_nitrokey_openpgp(void);
void demonstrate_message_signing(void);
void demonstrate_email_signing(void);
void demonstrate_signature_verification(void);

int main(int argc, char* argv[]) {
    printf("M17 OpenPGP Integration Example\n");
    printf("===============================\n\n");
    
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char* mode = argv[1];
    
    if (strcmp(mode, "software") == 0) {
        demonstrate_software_openpgp();
    } else if (strcmp(mode, "nitrokey") == 0) {
        demonstrate_nitrokey_openpgp();
    } else if (strcmp(mode, "sign") == 0) {
        demonstrate_message_signing();
    } else if (strcmp(mode, "email") == 0) {
        demonstrate_email_signing();
    } else if (strcmp(mode, "verify") == 0) {
        demonstrate_signature_verification();
    } else {
        printf("Unknown mode: %s\n", mode);
        print_usage(argv[0]);
        return 1;
    }
    
    return 0;
}

void print_usage(const char* program_name) {
    printf("Usage: %s <mode>\n", program_name);
    printf("\nModes:\n");
    printf("  software    - Demonstrate software-based OpenPGP (GnuPG)\n");
    printf("  nitrokey    - Demonstrate hardware-based OpenPGP (Nitrokey)\n");
    printf("  sign        - Sign a message with OpenPGP\n");
    printf("  email       - Sign an email with OpenPGP\n");
    printf("  verify      - Verify an OpenPGP signature\n");
    printf("\nExamples:\n");
    printf("  %s software\n", program_name);
    printf("  %s nitrokey\n", program_name);
    printf("  %s sign \"Hello, M17 World!\"\n", program_name);
}

void demonstrate_software_openpgp(void) {
    printf("=== Software-Based OpenPGP Demonstration ===\n\n");
    
    // Initialize OpenPGP integration
    printf("1. Initializing OpenPGP integration...\n");
    m17_openpgp_status_t status = m17_openpgp_init();
    if (status != M17_OPENPGP_SUCCESS) {
        printf("   ❌ Failed to initialize OpenPGP (status: %d)\n", status);
        return;
    }
    printf("   ✅ OpenPGP integration initialized\n");
    
    // Check GnuPG availability
    printf("\n2. Checking GnuPG availability...\n");
    status = m17_openpgp_check_gpg_availability();
    if (status != M17_OPENPGP_SUCCESS) {
        printf("   ❌ GnuPG not available (status: %d)\n", status);
        printf("   💡 Install GnuPG: sudo apt-get install gnupg2\n");
        return;
    }
    printf("   ✅ GnuPG is available\n");
    
    // List available keys
    printf("\n3. Listing available OpenPGP keys...\n");
    m17_openpgp_key_info_t keys[10];
    size_t key_count = 0;
    status = m17_openpgp_list_keys(keys, 10, &key_count);
    if (status != M17_OPENPGP_SUCCESS) {
        printf("   ❌ Failed to list keys (status: %d)\n", status);
        return;
    }
    
    if (key_count == 0) {
        printf("   ⚠️  No OpenPGP keys found\n");
        printf("   💡 Generate a key: gpg --gen-key\n");
        return;
    }
    
    printf("   ✅ Found %zu OpenPGP keys:\n", key_count);
    for (size_t i = 0; i < key_count; i++) {
        printf("      Key %zu: %s (%s) - %s\n", i + 1, 
               keys[i].key_id, 
               keys[i].is_secret ? "secret" : "public",
               keys[i].user_id);
    }
    
    // Demonstrate message signing
    printf("\n4. Demonstrating message signing...\n");
    const char* test_message = "Hello, M17 OpenPGP World!";
    m17_openpgp_signature_t signature;
    
    // Use first secret key if available
    const char* key_id = NULL;
    for (size_t i = 0; i < key_count; i++) {
        if (keys[i].is_secret) {
            key_id = keys[i].key_id;
            break;
        }
    }
    
    if (!key_id) {
        printf("   ⚠️  No secret keys available for signing\n");
        printf("   💡 Generate a secret key: gpg --gen-key\n");
        return;
    }
    
    status = m17_openpgp_sign_message(test_message, strlen(test_message), 
                                    key_id, M17_OPENPGP_SIG_TEXT, &signature);
    if (status == M17_OPENPGP_SUCCESS) {
        printf("   ✅ Message signed successfully!\n");
        printf("      Key ID: %s\n", signature.key_id);
        printf("      Signature size: %zu bytes\n", signature.signature_size);
        printf("      Signature type: %d\n", signature.sig_type);
    } else {
        printf("   ❌ Failed to sign message (status: %d)\n", status);
    }
    
    // Cleanup
    printf("\n5. Cleaning up...\n");
    m17_openpgp_cleanup();
    printf("   ✅ OpenPGP integration cleaned up\n");
}

void demonstrate_nitrokey_openpgp(void) {
    printf("=== Hardware-Based OpenPGP Demonstration (Nitrokey) ===\n\n");
    
    // Initialize Nitrokey OpenPGP integration
    printf("1. Initializing Nitrokey OpenPGP integration...\n");
    m17_nitrokey_openpgp_status_t status = m17_nitrokey_openpgp_init();
    if (status != M17_NITROKEY_OPENPGP_SUCCESS) {
        printf("   ❌ Failed to initialize Nitrokey OpenPGP (status: %d)\n", status);
        if (status == M17_NITROKEY_OPENPGP_ERROR_DEVICE_NOT_FOUND) {
            printf("   💡 Connect a Nitrokey device and install nitropy: pip install pynitrokey\n");
        }
        return;
    }
    printf("   ✅ Nitrokey OpenPGP integration initialized\n");
    
    // Check Nitrokey device
    printf("\n2. Checking Nitrokey device...\n");
    status = m17_nitrokey_openpgp_check_device();
    if (status != M17_NITROKEY_OPENPGP_SUCCESS) {
        printf("   ❌ Nitrokey device not found (status: %d)\n", status);
        printf("   💡 Connect a Nitrokey device and check: nitropy nk3 list\n");
        return;
    }
    printf("   ✅ Nitrokey device is available\n");
    
    // List keys on Nitrokey
    printf("\n3. Listing keys on Nitrokey...\n");
    m17_nitrokey_openpgp_key_t keys[10];
    size_t key_count = 0;
    status = m17_nitrokey_openpgp_list_keys(keys, 10, &key_count);
    if (status != M17_NITROKEY_OPENPGP_SUCCESS) {
        printf("   ❌ Failed to list Nitrokey keys (status: %d)\n", status);
        return;
    }
    
    printf("   ✅ Found %zu keys on Nitrokey:\n", key_count);
    for (size_t i = 0; i < key_count; i++) {
        printf("      Key %zu: %s (%s, %u bits) - %s\n", i + 1, 
               keys[i].key_name,
               keys[i].is_ed25519 ? "Ed25519" : (keys[i].is_rsa ? "RSA" : "Unknown"),
               keys[i].key_size,
               keys[i].user_id);
    }
    
    // Generate a test key if none exist
    if (key_count == 0) {
        printf("\n4. Generating test key on Nitrokey...\n");
        status = m17_nitrokey_openpgp_generate_ed25519_key("m17-test-key", 
                                                         "M17 Test <test@m17project.org>", 
                                                         NULL);
        if (status == M17_NITROKEY_OPENPGP_SUCCESS) {
            printf("   ✅ Generated Ed25519 key on Nitrokey\n");
        } else {
            printf("   ❌ Failed to generate key on Nitrokey (status: %d)\n", status);
            return;
        }
    }
    
    // Demonstrate hardware signing
    printf("\n5. Demonstrating hardware signing...\n");
    const char* test_message = "Secure M17 message signed with Nitrokey!";
    m17_openpgp_signature_t signature;
    
    const char* key_name = (key_count > 0) ? keys[0].key_name : "m17-test-key";
    status = m17_nitrokey_openpgp_sign_message(test_message, strlen(test_message), 
                                             key_name, M17_OPENPGP_SIG_TEXT, &signature);
    if (status == M17_NITROKEY_OPENPGP_SUCCESS) {
        printf("   ✅ Message signed with Nitrokey!\n");
        printf("      Key: %s\n", signature.key_id);
        printf("      Signature size: %zu bytes\n", signature.signature_size);
        printf("      Signature type: %d\n", signature.sig_type);
    } else {
        printf("   ❌ Failed to sign message with Nitrokey (status: %d)\n", status);
    }
    
    // Cleanup
    printf("\n6. Cleaning up...\n");
    m17_nitrokey_openpgp_cleanup();
    printf("   ✅ Nitrokey OpenPGP integration cleaned up\n");
}

void demonstrate_message_signing(void) {
    printf("=== Message Signing Demonstration ===\n\n");
    
    // Initialize OpenPGP
    m17_openpgp_status_t status = m17_openpgp_init();
    if (status != M17_OPENPGP_SUCCESS) {
        printf("❌ Failed to initialize OpenPGP\n");
        return;
    }
    
    // Get message from user
    char message[MAX_MESSAGE_SIZE];
    printf("Enter message to sign: ");
    if (fgets(message, sizeof(message), stdin) == NULL) {
        printf("❌ Failed to read message\n");
        m17_openpgp_cleanup();
        return;
    }
    
    // Remove newline
    message[strcspn(message, "\n")] = '\0';
    
    // List available keys
    m17_openpgp_key_info_t keys[10];
    size_t key_count = 0;
    status = m17_openpgp_list_keys(keys, 10, &key_count);
    if (status != M17_OPENPGP_SUCCESS || key_count == 0) {
        printf("❌ No OpenPGP keys available\n");
        m17_openpgp_cleanup();
        return;
    }
    
    // Use first secret key
    const char* key_id = NULL;
    for (size_t i = 0; i < key_count; i++) {
        if (keys[i].is_secret) {
            key_id = keys[i].key_id;
            break;
        }
    }
    
    if (!key_id) {
        printf("❌ No secret keys available for signing\n");
        m17_openpgp_cleanup();
        return;
    }
    
    // Sign message
    printf("\nSigning message with key: %s\n", key_id);
    m17_openpgp_signature_t signature;
    status = m17_openpgp_sign_message(message, strlen(message), 
                                    key_id, M17_OPENPGP_SIG_TEXT, &signature);
    if (status == M17_OPENPGP_SUCCESS) {
        printf("✅ Message signed successfully!\n");
        printf("\nSignature:\n%s\n", signature.signature_armored);
    } else {
        printf("❌ Failed to sign message (status: %d)\n", status);
    }
    
    m17_openpgp_cleanup();
}

void demonstrate_email_signing(void) {
    printf("=== Email Signing Demonstration ===\n\n");
    
    // Initialize OpenPGP
    m17_openpgp_status_t status = m17_openpgp_init();
    if (status != M17_OPENPGP_SUCCESS) {
        printf("❌ Failed to initialize OpenPGP\n");
        return;
    }
    
    // Create sample email
    char email[MAX_EMAIL_SIZE];
    snprintf(email, sizeof(email),
             "From: m17@example.com\n"
             "To: recipient@example.com\n"
             "Subject: Secure M17 Communication\n"
             "Date: %s\n\n"
             "This is a secure message signed with OpenPGP for M17 digital radio communication.\n"
             "The signature ensures message integrity and authenticity.\n",
             ctime(&(time_t){time(NULL)}));
    
    printf("Sample email:\n%s\n", email);
    
    // List available keys
    m17_openpgp_key_info_t keys[10];
    size_t key_count = 0;
    status = m17_openpgp_list_keys(keys, 10, &key_count);
    if (status != M17_OPENPGP_SUCCESS || key_count == 0) {
        printf("❌ No OpenPGP keys available\n");
        m17_openpgp_cleanup();
        return;
    }
    
    // Use first secret key
    const char* key_id = NULL;
    for (size_t i = 0; i < key_count; i++) {
        if (keys[i].is_secret) {
            key_id = keys[i].key_id;
            break;
        }
    }
    
    if (!key_id) {
        printf("❌ No secret keys available for signing\n");
        m17_openpgp_cleanup();
        return;
    }
    
    // Sign email
    printf("Signing email with key: %s\n", key_id);
    m17_openpgp_signature_t signature;
    status = m17_openpgp_sign_email(email, strlen(email), 
                                  key_id, M17_OPENPGP_SIG_TEXT, &signature);
    if (status == M17_OPENPGP_SUCCESS) {
        printf("✅ Email signed successfully!\n");
        printf("\nSignature:\n%s\n", signature.signature_armored);
    } else {
        printf("❌ Failed to sign email (status: %d)\n", status);
    }
    
    m17_openpgp_cleanup();
}

void demonstrate_signature_verification(void) {
    printf("=== Signature Verification Demonstration ===\n\n");
    
    printf("This demonstration shows how to verify OpenPGP signatures.\n");
    printf("In a real scenario, you would:\n");
    printf("1. Receive a message and its signature\n");
    printf("2. Import the sender's public key\n");
    printf("3. Verify the signature\n");
    printf("4. Trust the message if signature is valid\n\n");
    
    printf("For this demo, we'll sign a message and then verify it:\n\n");
    
    // Initialize OpenPGP
    m17_openpgp_status_t status = m17_openpgp_init();
    if (status != M17_OPENPGP_SUCCESS) {
        printf("❌ Failed to initialize OpenPGP\n");
        return;
    }
    
    // Create test message
    const char* test_message = "This is a test message for signature verification.";
    printf("Test message: %s\n\n", test_message);
    
    // List available keys
    m17_openpgp_key_info_t keys[10];
    size_t key_count = 0;
    status = m17_openpgp_list_keys(keys, 10, &key_count);
    if (status != M17_OPENPGP_SUCCESS || key_count == 0) {
        printf("❌ No OpenPGP keys available\n");
        m17_openpgp_cleanup();
        return;
    }
    
    // Use first secret key
    const char* key_id = NULL;
    for (size_t i = 0; i < key_count; i++) {
        if (keys[i].is_secret) {
            key_id = keys[i].key_id;
            break;
        }
    }
    
    if (!key_id) {
        printf("❌ No secret keys available for signing\n");
        m17_openpgp_cleanup();
        return;
    }
    
    // Sign message
    printf("1. Signing message with key: %s\n", key_id);
    m17_openpgp_signature_t signature;
    status = m17_openpgp_sign_message(test_message, strlen(test_message), 
                                    key_id, M17_OPENPGP_SIG_TEXT, &signature);
    if (status != M17_OPENPGP_SUCCESS) {
        printf("❌ Failed to sign message (status: %d)\n", status);
        m17_openpgp_cleanup();
        return;
    }
    printf("✅ Message signed successfully\n");
    
    // Verify signature
    printf("\n2. Verifying signature...\n");
    m17_openpgp_verification_t verification;
    status = m17_openpgp_verify_signature(test_message, strlen(test_message),
                                        signature.signature_armored, signature.signature_size,
                                        &verification);
    if (status == M17_OPENPGP_SUCCESS) {
        printf("✅ Signature verification completed\n");
        printf("   Valid: %s\n", verification.is_valid ? "Yes" : "No");
        printf("   Key ID: %s\n", verification.key_id);
        printf("   User ID: %s\n", verification.user_id);
        if (!verification.is_valid) {
            printf("   Error: %s\n", verification.error_message);
        }
    } else {
        printf("❌ Failed to verify signature (status: %d)\n", status);
    }
    
    m17_openpgp_cleanup();
}
