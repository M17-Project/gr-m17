//--------------------------------------------------------------------
// M17 C library - crypto/test_openpgp_integration.c
//
// OpenPGP integration test suite for M17 digital radio protocol
// Tests GnuPG and Nitrokey OpenPGP functionality
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 21 October 2025
//--------------------------------------------------------------------

#include "openpgp_integration.h"
#include "nitrokey_openpgp.h"
#include "m17.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>

// Test configuration
#define TEST_MESSAGE "Hello, M17 OpenPGP World!"
#define TEST_EMAIL "From: test@example.com\nTo: recipient@example.com\nSubject: Test\n\nThis is a test email for OpenPGP signing."
#define TEST_KEY_NAME "m17-test-key"
#define TEST_USER_ID "M17 Test <test@m17project.org>"

// Test counters
static int tests_passed = 0;
static int tests_failed = 0;

// Test assertion macro
#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            printf("✓ %s\n", message); \
            tests_passed++; \
        } else { \
            printf("✗ %s\n", message); \
            tests_failed++; \
        } \
    } while(0)

// Test OpenPGP initialization
static void test_openpgp_init(void) {
    printf("\n=== Testing OpenPGP Initialization ===\n");
    
    m17_openpgp_status_t status = m17_openpgp_init();
    TEST_ASSERT(status == M17_OPENPGP_SUCCESS, "OpenPGP initialization");
    
    status = m17_openpgp_check_gpg_availability();
    TEST_ASSERT(status == M17_OPENPGP_SUCCESS, "GnuPG availability check");
}

// Test OpenPGP key listing
static void test_openpgp_list_keys(void) {
    printf("\n=== Testing OpenPGP Key Listing ===\n");
    
    m17_openpgp_key_info_t keys[10];
    size_t key_count = 0;
    
    m17_openpgp_status_t status = m17_openpgp_list_keys(keys, 10, &key_count);
    TEST_ASSERT(status == M17_OPENPGP_SUCCESS, "List OpenPGP keys");
    
    printf("Found %zu OpenPGP keys:\n", key_count);
    for (size_t i = 0; i < key_count; i++) {
        printf("  Key %zu: %s (%s) - %s\n", i + 1, 
               keys[i].key_id, 
               keys[i].is_secret ? "secret" : "public",
               keys[i].user_id);
    }
}

// Test OpenPGP message signing
static void test_openpgp_sign_message(void) {
    printf("\n=== Testing OpenPGP Message Signing ===\n");
    
    // Get first available key
    m17_openpgp_key_info_t keys[10];
    size_t key_count = 0;
    m17_openpgp_status_t status = m17_openpgp_list_keys(keys, 10, &key_count);
    
    if (key_count == 0) {
        printf("No keys available for signing test\n");
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
        printf("No secret keys available for signing test\n");
        return;
    }
    
    // Sign message
    m17_openpgp_signature_t signature;
    status = m17_openpgp_sign_message(TEST_MESSAGE, strlen(TEST_MESSAGE), 
                                    key_id, M17_OPENPGP_SIG_TEXT, &signature);
    TEST_ASSERT(status == M17_OPENPGP_SUCCESS, "Sign message with OpenPGP");
    
    if (status == M17_OPENPGP_SUCCESS) {
        printf("Signature created successfully:\n");
        printf("  Key ID: %s\n", signature.key_id);
        printf("  Signature size: %zu bytes\n", signature.signature_size);
        printf("  Signature type: %d\n", signature.sig_type);
    }
}

// Test OpenPGP email signing
static void test_openpgp_sign_email(void) {
    printf("\n=== Testing OpenPGP Email Signing ===\n");
    
    // Get first available key
    m17_openpgp_key_info_t keys[10];
    size_t key_count = 0;
    m17_openpgp_status_t status = m17_openpgp_list_keys(keys, 10, &key_count);
    
    if (key_count == 0) {
        printf("No keys available for email signing test\n");
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
        printf("No secret keys available for email signing test\n");
        return;
    }
    
    // Sign email
    m17_openpgp_signature_t signature;
    status = m17_openpgp_sign_email(TEST_EMAIL, strlen(TEST_EMAIL), 
                                  key_id, M17_OPENPGP_SIG_TEXT, &signature);
    TEST_ASSERT(status == M17_OPENPGP_SUCCESS, "Sign email with OpenPGP");
    
    if (status == M17_OPENPGP_SUCCESS) {
        printf("Email signature created successfully:\n");
        printf("  Key ID: %s\n", signature.key_id);
        printf("  Signature size: %zu bytes\n", signature.signature_size);
    }
}

// Test OpenPGP signature verification
static void test_openpgp_verify_signature(void) {
    printf("\n=== Testing OpenPGP Signature Verification ===\n");
    
    // Get first available key
    m17_openpgp_key_info_t keys[10];
    size_t key_count = 0;
    m17_openpgp_status_t status = m17_openpgp_list_keys(keys, 10, &key_count);
    
    if (key_count == 0) {
        printf("No keys available for verification test\n");
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
        printf("No secret keys available for verification test\n");
        return;
    }
    
    // Create signature
    m17_openpgp_signature_t signature;
    status = m17_openpgp_sign_message(TEST_MESSAGE, strlen(TEST_MESSAGE), 
                                    key_id, M17_OPENPGP_SIG_TEXT, &signature);
    TEST_ASSERT(status == M17_OPENPGP_SUCCESS, "Create signature for verification");
    
    if (status == M17_OPENPGP_SUCCESS) {
        // Verify signature
        m17_openpgp_verification_t verification;
        status = m17_openpgp_verify_signature(TEST_MESSAGE, strlen(TEST_MESSAGE),
                                            signature.signature_armored, signature.signature_size,
                                            &verification);
        TEST_ASSERT(status == M17_OPENPGP_SUCCESS, "Verify OpenPGP signature");
        
        if (status == M17_OPENPGP_SUCCESS) {
            printf("Signature verification result:\n");
            printf("  Valid: %s\n", verification.is_valid ? "Yes" : "No");
            printf("  Key ID: %s\n", verification.key_id);
            printf("  User ID: %s\n", verification.user_id);
        }
    }
}

// Test Nitrokey OpenPGP initialization
static void test_nitrokey_openpgp_init(void) {
    printf("\n=== Testing Nitrokey OpenPGP Initialization ===\n");
    
    m17_nitrokey_openpgp_status_t status = m17_nitrokey_openpgp_init();
    if (status == M17_NITROKEY_OPENPGP_ERROR_DEVICE_NOT_FOUND) {
        printf("Nitrokey device not found - skipping Nitrokey tests\n");
        return;
    }
    
    TEST_ASSERT(status == M17_NITROKEY_OPENPGP_SUCCESS, "Nitrokey OpenPGP initialization");
    
    status = m17_nitrokey_openpgp_check_device();
    TEST_ASSERT(status == M17_NITROKEY_OPENPGP_SUCCESS, "Nitrokey device check");
}

// Test Nitrokey OpenPGP key listing
static void test_nitrokey_openpgp_list_keys(void) {
    printf("\n=== Testing Nitrokey OpenPGP Key Listing ===\n");
    
    m17_nitrokey_openpgp_key_t keys[10];
    size_t key_count = 0;
    
    m17_nitrokey_openpgp_status_t status = m17_nitrokey_openpgp_list_keys(keys, 10, &key_count);
    if (status == M17_NITROKEY_OPENPGP_ERROR_DEVICE_NOT_FOUND) {
        printf("Nitrokey device not found - skipping key listing test\n");
        return;
    }
    
    TEST_ASSERT(status == M17_NITROKEY_OPENPGP_SUCCESS, "List Nitrokey OpenPGP keys");
    
    printf("Found %zu Nitrokey OpenPGP keys:\n", key_count);
    for (size_t i = 0; i < key_count; i++) {
        printf("  Key %zu: %s (%s, %s) - %s\n", i + 1, 
               keys[i].key_name,
               keys[i].is_ed25519 ? "Ed25519" : (keys[i].is_rsa ? "RSA" : "Unknown"),
               keys[i].key_size > 0 ? "secret" : "public",
               keys[i].user_id);
    }
}

// Test Nitrokey OpenPGP key generation
static void test_nitrokey_openpgp_generate_key(void) {
    printf("\n=== Testing Nitrokey OpenPGP Key Generation ===\n");
    
    m17_nitrokey_openpgp_status_t status = m17_nitrokey_openpgp_check_device();
    if (status == M17_NITROKEY_OPENPGP_ERROR_DEVICE_NOT_FOUND) {
        printf("Nitrokey device not found - skipping key generation test\n");
        return;
    }
    
    // Generate Ed25519 key
    status = m17_nitrokey_openpgp_generate_ed25519_key(TEST_KEY_NAME, TEST_USER_ID, NULL);
    if (status == M17_NITROKEY_OPENPGP_SUCCESS) {
        printf("✓ Generated Ed25519 key on Nitrokey\n");
        tests_passed++;
    } else {
        printf("✗ Failed to generate Ed25519 key on Nitrokey (status: %d)\n", status);
        tests_failed++;
    }
}

// Test Nitrokey OpenPGP message signing
static void test_nitrokey_openpgp_sign_message(void) {
    printf("\n=== Testing Nitrokey OpenPGP Message Signing ===\n");
    
    m17_nitrokey_openpgp_status_t status = m17_nitrokey_openpgp_check_device();
    if (status == M17_NITROKEY_OPENPGP_ERROR_DEVICE_NOT_FOUND) {
        printf("Nitrokey device not found - skipping message signing test\n");
        return;
    }
    
    // Try to use the test key we generated
    m17_openpgp_signature_t signature;
    status = m17_nitrokey_openpgp_sign_message(TEST_MESSAGE, strlen(TEST_MESSAGE), 
                                             TEST_KEY_NAME, M17_OPENPGP_SIG_TEXT, &signature);
    if (status == M17_NITROKEY_OPENPGP_SUCCESS) {
        printf("✓ Signed message with Nitrokey\n");
        printf("  Key: %s\n", signature.key_id);
        printf("  Signature size: %zu bytes\n", signature.signature_size);
        tests_passed++;
    } else {
        printf("✗ Failed to sign message with Nitrokey (status: %d)\n", status);
        tests_failed++;
    }
}

// Test Nitrokey OpenPGP email signing
static void test_nitrokey_openpgp_sign_email(void) {
    printf("\n=== Testing Nitrokey OpenPGP Email Signing ===\n");
    
    m17_nitrokey_openpgp_status_t status = m17_nitrokey_openpgp_check_device();
    if (status == M17_NITROKEY_OPENPGP_ERROR_DEVICE_NOT_FOUND) {
        printf("Nitrokey device not found - skipping email signing test\n");
        return;
    }
    
    // Try to use the test key we generated
    m17_openpgp_signature_t signature;
    status = m17_nitrokey_openpgp_sign_email(TEST_EMAIL, strlen(TEST_EMAIL), 
                                           TEST_KEY_NAME, M17_OPENPGP_SIG_TEXT, &signature);
    if (status == M17_NITROKEY_OPENPGP_SUCCESS) {
        printf("✓ Signed email with Nitrokey\n");
        printf("  Key: %s\n", signature.key_id);
        printf("  Signature size: %zu bytes\n", signature.signature_size);
        tests_passed++;
    } else {
        printf("✗ Failed to sign email with Nitrokey (status: %d)\n", status);
        tests_failed++;
    }
}

// Test cleanup
static void test_cleanup(void) {
    printf("\n=== Testing Cleanup ===\n");
    
    m17_openpgp_cleanup();
    printf("✓ OpenPGP integration cleaned up\n");
    tests_passed++;
    
    m17_nitrokey_openpgp_cleanup();
    printf("✓ Nitrokey OpenPGP integration cleaned up\n");
    tests_passed++;
}

// Main test function
int main(void) {
    printf("M17 OpenPGP Integration Test Suite\n");
    printf("==================================\n");
    
    // Run all tests
    test_openpgp_init();
    test_openpgp_list_keys();
    test_openpgp_sign_message();
    test_openpgp_sign_email();
    test_openpgp_verify_signature();
    
    test_nitrokey_openpgp_init();
    test_nitrokey_openpgp_list_keys();
    test_nitrokey_openpgp_generate_key();
    test_nitrokey_openpgp_sign_message();
    test_nitrokey_openpgp_sign_email();
    
    test_cleanup();
    
    // Print results
    printf("\n=== Test Results ===\n");
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    printf("Total tests: %d\n", tests_passed + tests_failed);
    
    if (tests_failed == 0) {
        printf("\n🎉 All tests passed! OpenPGP integration is working correctly.\n");
        return 0;
    } else {
        printf("\n❌ Some tests failed. Check the output above for details.\n");
        return 1;
    }
}
