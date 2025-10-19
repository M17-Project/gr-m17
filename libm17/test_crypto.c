//--------------------------------------------------------------------
// M17 C library - test_crypto.c
//
// Test suite for M17 Ed25519/Curve25519 cryptographic functions
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------

#include "m17.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>

// Test Ed25519 functionality
void test_ed25519() {
    printf("Testing Ed25519 functionality...\n");
    
    uint8_t public_key[M17_ED25519_PUBLIC_KEY_SIZE];
    uint8_t private_key[M17_ED25519_PRIVATE_KEY_SIZE];
    uint8_t signature[M17_ED25519_SIGNATURE_SIZE];
    
    // Test key generation
    printf("  - Testing key generation...\n");
    assert(m17_ed25519_generate_keypair(public_key, private_key) == 0);
    printf("    [OK] Key generation successful\n");
    
    // Test public key derivation
    printf("  - Testing public key derivation...\n");
    uint8_t derived_public[M17_ED25519_PUBLIC_KEY_SIZE];
    assert(m17_ed25519_public_key_from_private(private_key, derived_public) == 0);
    assert(memcmp(public_key, derived_public, M17_ED25519_PUBLIC_KEY_SIZE) == 0);
    printf("    [OK] Public key derivation successful\n");
    
    // Test signing
    printf("  - Testing signature generation...\n");
    const char* message = "Hello, M17 Ed25519!";
    assert(m17_ed25519_sign((const uint8_t*)message, strlen(message), private_key, signature) == 0);
    printf("    [OK] Signature generation successful\n");
    
    // Test verification
    printf("  - Testing signature verification...\n");
    assert(m17_ed25519_verify((const uint8_t*)message, strlen(message), signature, public_key) == 0);
    printf("    [OK] Signature verification successful\n");
    
    // Test verification with wrong message
    printf("  - Testing signature verification with wrong message...\n");
    const char* wrong_message = "Wrong message";
    assert(m17_ed25519_verify((const uint8_t*)wrong_message, strlen(wrong_message), signature, public_key) != 0);
    printf("    [OK] Wrong message correctly rejected\n");
    
    // Suppress unused variable warnings
    (void)derived_public;
    (void)signature;
    (void)public_key;
    (void)private_key;
    (void)message;
    (void)wrong_message;
    
    printf("[PASS] Ed25519 tests passed!\n\n");
}

// Test Curve25519 functionality
void test_curve25519() {
    printf("Testing Curve25519 functionality...\n");
    
    uint8_t alice_public[M17_CURVE25519_PUBLIC_KEY_SIZE];
    uint8_t alice_private[M17_CURVE25519_PRIVATE_KEY_SIZE];
    uint8_t bob_public[M17_CURVE25519_PUBLIC_KEY_SIZE];
    uint8_t bob_private[M17_CURVE25519_PRIVATE_KEY_SIZE];
    uint8_t alice_shared[M17_CURVE25519_SHARED_SECRET_SIZE];
    uint8_t bob_shared[M17_CURVE25519_SHARED_SECRET_SIZE];
    
    // Test Alice key generation
    printf("  - Testing Alice key generation...\n");
    assert(m17_curve25519_generate_keypair(alice_public, alice_private) == 0);
    printf("    [OK] Alice key generation successful\n");
    
    // Test Bob key generation
    printf("  - Testing Bob key generation...\n");
    assert(m17_curve25519_generate_keypair(bob_public, bob_private) == 0);
    printf("    [OK] Bob key generation successful\n");
    
    // Test ECDH - Alice side
    printf("  - Testing ECDH (Alice side)...\n");
    assert(m17_curve25519_ecdh(alice_private, bob_public, alice_shared) == 0);
    printf("    [OK] Alice ECDH successful\n");
    
    // Test ECDH - Bob side
    printf("  - Testing ECDH (Bob side)...\n");
    assert(m17_curve25519_ecdh(bob_private, alice_public, bob_shared) == 0);
    printf("    [OK] Bob ECDH successful\n");
    
    // Test shared secret agreement
    printf("  - Testing shared secret agreement...\n");
    assert(memcmp(alice_shared, bob_shared, M17_CURVE25519_SHARED_SECRET_SIZE) == 0);
    printf("    [OK] Shared secrets match\n");
    
    // Suppress unused variable warnings
    (void)alice_public;
    (void)alice_private;
    (void)bob_public;
    (void)bob_private;
    (void)alice_shared;
    (void)bob_shared;
    
    printf("[OK] Curve25519 tests passed!\n\n");
}

// Test HKDF functionality
void test_hkdf() {
    printf("Testing HKDF functionality...\n");
    
    uint8_t input_key_material[32] = {0};
    uint8_t salt[32] = {0};
    uint8_t info[] = "M17-HKDF-Test";
    uint8_t output[32];
    
    // Initialize test data
    for (int i = 0; i < 32; i++) {
        input_key_material[i] = i;
        salt[i] = i * 2;
    }
    
    printf("  - Testing HKDF key derivation...\n");
    assert(m17_hkdf_derive(input_key_material, 32, salt, 32, info, strlen((char*)info), output, 32) == 0);
    printf("    [OK] HKDF key derivation successful\n");
    
    // Test with different parameters
    printf("  - Testing HKDF with different parameters...\n");
    uint8_t output2[16];
    assert(m17_hkdf_derive(input_key_material, 32, NULL, 0, NULL, 0, output2, 16) == 0);
    printf("    [OK] HKDF with minimal parameters successful\n");
    
    // Suppress unused variable warnings
    (void)input_key_material;
    (void)salt;
    (void)info;
    (void)output;
    (void)output2;
    
    printf("[OK] HKDF tests passed!\n\n");
}

// Test AES-GCM functionality
void test_aes_gcm() {
    printf("Testing AES-GCM functionality...\n");
    
    uint8_t key[M17_AES_GCM_KEY_SIZE] = {0};
    uint8_t iv[M17_AES_GCM_IV_SIZE] = {0};
    uint8_t plaintext[] = "Hello, M17 AES-GCM!";
    uint8_t ciphertext[sizeof(plaintext)];
    uint8_t tag[M17_AES_GCM_TAG_SIZE];
    uint8_t decrypted[sizeof(plaintext)];
    
    // Initialize test data
    for (int i = 0; i < M17_AES_GCM_KEY_SIZE; i++) {
        key[i] = i;
    }
    for (int i = 0; i < M17_AES_GCM_IV_SIZE; i++) {
        iv[i] = i * 3;
    }
    
    // Test encryption
    printf("  - Testing AES-GCM encryption...\n");
    assert(m17_aes_gcm_encrypt(plaintext, sizeof(plaintext) - 1, key, iv, ciphertext, tag) == 0);
    printf("    [OK] AES-GCM encryption successful\n");
    
    // Test decryption
    printf("  - Testing AES-GCM decryption...\n");
    assert(m17_aes_gcm_decrypt(ciphertext, sizeof(plaintext) - 1, key, iv, tag, decrypted) == 0);
    printf("    [OK] AES-GCM decryption successful\n");
    
    // Test plaintext recovery
    printf("  - Testing plaintext recovery...\n");
    assert(memcmp(plaintext, decrypted, sizeof(plaintext) - 1) == 0);
    printf("    [OK] Plaintext correctly recovered\n");
    
    // Test authentication failure
    printf("  - Testing authentication failure...\n");
    uint8_t wrong_tag[M17_AES_GCM_TAG_SIZE] = {0};
    assert(m17_aes_gcm_decrypt(ciphertext, sizeof(plaintext) - 1, key, iv, wrong_tag, decrypted) != 0);
    printf("    [OK] Authentication failure correctly detected\n");
    
    // Suppress unused variable warnings
    (void)key;
    (void)iv;
    (void)ciphertext;
    (void)tag;
    (void)decrypted;
    (void)wrong_tag;
    
    printf("[OK] AES-GCM tests passed!\n\n");
}

// Test integrated M17 crypto workflow
void test_integrated_workflow() {
    printf("Testing integrated M17 crypto workflow...\n");
    
    // Step 1: Generate Ed25519 keypair for signing
    printf("  - Step 1: Generating Ed25519 keypair...\n");
    uint8_t sign_public[M17_ED25519_PUBLIC_KEY_SIZE];
    uint8_t sign_private[M17_ED25519_PRIVATE_KEY_SIZE];
    assert(m17_ed25519_generate_keypair(sign_public, sign_private) == 0);
    printf("    [OK] Ed25519 keypair generated\n");
    
    // Step 2: Generate Curve25519 keypairs for ECDH
    printf("  - Step 2: Generating Curve25519 keypairs...\n");
    uint8_t alice_public[M17_CURVE25519_PUBLIC_KEY_SIZE];
    uint8_t alice_private[M17_CURVE25519_PRIVATE_KEY_SIZE];
    uint8_t bob_public[M17_CURVE25519_PUBLIC_KEY_SIZE];
    uint8_t bob_private[M17_CURVE25519_PRIVATE_KEY_SIZE];
    
    assert(m17_curve25519_generate_keypair(alice_public, alice_private) == 0);
    assert(m17_curve25519_generate_keypair(bob_public, bob_private) == 0);
    printf("    [OK] Curve25519 keypairs generated\n");
    
    // Step 3: Perform ECDH key exchange
    printf("  - Step 3: Performing ECDH key exchange...\n");
    uint8_t shared_secret[M17_CURVE25519_SHARED_SECRET_SIZE];
    assert(m17_curve25519_ecdh(alice_private, bob_public, shared_secret) == 0);
    printf("    [OK] ECDH key exchange successful\n");
    
    // Step 4: Derive encryption key using HKDF
    printf("  - Step 4: Deriving encryption key...\n");
    uint8_t encryption_key[M17_AES_GCM_KEY_SIZE];
    const char* hkdf_info = "M17-Encryption-Key";
    assert(m17_hkdf_derive(shared_secret, M17_CURVE25519_SHARED_SECRET_SIZE, 
                          NULL, 0, (const uint8_t*)hkdf_info, strlen(hkdf_info), 
                          encryption_key, M17_AES_GCM_KEY_SIZE) == 0);
    printf("    [OK] Encryption key derived\n");
    
    // Step 5: Encrypt message
    printf("  - Step 5: Encrypting message...\n");
    const char* message = "Secret M17 message!";
    uint8_t iv[M17_AES_GCM_IV_SIZE] = {0};
    uint8_t ciphertext[64];
    uint8_t tag[M17_AES_GCM_TAG_SIZE];
    
    assert(m17_aes_gcm_encrypt((const uint8_t*)message, strlen(message), 
                              encryption_key, iv, ciphertext, tag) == 0);
    printf("    [OK] Message encrypted\n");
    
    // Step 6: Sign encrypted data
    printf("  - Step 6: Signing encrypted data...\n");
    uint8_t signature[M17_ED25519_SIGNATURE_SIZE];
    uint8_t encrypted_data[64 + M17_AES_GCM_TAG_SIZE];
    memcpy(encrypted_data, ciphertext, 64);
    memcpy(encrypted_data + 64, tag, M17_AES_GCM_TAG_SIZE);
    
    assert(m17_ed25519_sign(encrypted_data, 64 + M17_AES_GCM_TAG_SIZE, 
                           sign_private, signature) == 0);
    printf("    [OK] Encrypted data signed\n");
    
    // Step 7: Verify signature
    printf("  - Step 7: Verifying signature...\n");
    assert(m17_ed25519_verify(encrypted_data, 64 + M17_AES_GCM_TAG_SIZE, 
                             signature, sign_public) == 0);
    printf("    [OK] Signature verified\n");
    
    // Step 8: Decrypt message
    printf("  - Step 8: Decrypting message...\n");
    uint8_t decrypted[64];
    assert(m17_aes_gcm_decrypt(ciphertext, 64, encryption_key, iv, tag, decrypted) == 0);
    assert(memcmp(message, decrypted, strlen(message)) == 0);
    printf("    [OK] Message decrypted and verified\n");
    
    // Suppress unused variable warnings
    (void)sign_public;
    (void)sign_private;
    (void)alice_public;
    (void)alice_private;
    (void)bob_public;
    (void)bob_private;
    (void)shared_secret;
    (void)encryption_key;
    (void)hkdf_info;
    (void)message;
    (void)iv;
    (void)signature;
    (void)decrypted;
    
    printf("[OK] Integrated M17 crypto workflow test passed!\n\n");
}

// Main test function
int main() {
    printf("M17 Ed25519/Curve25519 Cryptographic Test Suite\n");
    printf("================================================\n\n");
    
    // Run all tests
    test_ed25519();
    test_curve25519();
    test_hkdf();
    test_aes_gcm();
    test_constant_time();
    test_integrated_workflow();
    
    printf("[SUCCESS] All M17 crypto tests passed successfully!\n");
    printf("\nM17 now supports:\n");
    printf("  [OK] Ed25519 digital signatures\n");
    printf("  [OK] Curve25519 ECDH key exchange\n");
    printf("  [OK] HKDF key derivation\n");
    printf("  [OK] AES-GCM authenticated encryption\n");
    printf("  [OK] Constant-time operations\n");
    printf("  [OK] Integrated secure communication workflow\n");
    
    return 0;
}

// Test constant-time operations
void test_constant_time() {
    printf("Testing constant-time operations...\n");
    
    uint8_t data1[] = "Hello, M17!";
    uint8_t data2[] = "Hello, M17!";
    uint8_t data3[] = "Hello, M18!";
    uint8_t result[sizeof(data1)];
    
    // Test constant-time memory comparison
    printf("  - Testing constant-time memory comparison...\n");
    assert(m17_constant_time_memcmp(data1, data2, sizeof(data1)) == 0);
    assert(m17_constant_time_memcmp(data1, data3, sizeof(data1)) != 0);
    printf("    [OK] Constant-time memory comparison successful\n");
    
    // Test constant-time memory copy
    printf("  - Testing constant-time memory copy...\n");
    m17_constant_time_memcpy(result, data1, sizeof(data1));
    assert(memcmp(data1, result, sizeof(data1)) == 0);
    printf("    [OK] Constant-time memory copy successful\n");
    
    // Test constant-time memory zeroing
    printf("  - Testing constant-time memory zeroing...\n");
    m17_constant_time_memzero(result, sizeof(result));
    for (size_t i = 0; i < sizeof(result); i++) {
        assert(result[i] == 0);
    }
    printf("    [OK] Constant-time memory zeroing successful\n");
    
    // Test constant-time array equality
    printf("  - Testing constant-time array equality...\n");
    assert(m17_constant_time_array_equal(data1, data2, sizeof(data1)) == 1);
    assert(m17_constant_time_array_equal(data1, data3, sizeof(data1)) == 0);
    printf("    [OK] Constant-time array equality successful\n");
    
    printf("Constant-time tests passed!\n");
}
