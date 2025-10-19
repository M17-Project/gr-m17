//--------------------------------------------------------------------
// M17 C library - test_chacha20_poly1305.c
//
// Test suite for ChaCha20-Poly1305 authenticated encryption
// Tests encryption, decryption, key generation, and validation
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------

#include "m17.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

// Test ChaCha20-Poly1305 encryption and decryption
int test_chacha20_poly1305_basic() {
    printf("Testing ChaCha20-Poly1305 basic encryption/decryption...\n");
    
    const char* plaintext = "Hello, M17! This is a test message for ChaCha20-Poly1305 encryption.";
    size_t plaintext_len = strlen(plaintext);
    
    uint8_t key[32];
    uint8_t iv[12];
    uint8_t aad[] = "M17-ChaCha20-Test";
    size_t aad_len = strlen((char*)aad);
    
    uint8_t ciphertext[256];
    uint8_t tag[16];
    uint8_t decrypted[256];
    
    // Generate test key and IV
    if (m17_chacha20_generate_key(key, sizeof(key)) != 0) {
        printf("ERROR: Failed to generate ChaCha20 key\n");
        return -1;
    }
    
    if (m17_chacha20_generate_iv(iv, sizeof(iv)) != 0) {
        printf("ERROR: Failed to generate ChaCha20 IV\n");
        return -1;
    }
    
    // Encrypt
    int ciphertext_len = m17_chacha20_poly1305_encrypt(
        (uint8_t*)plaintext, plaintext_len,
        key, sizeof(key),
        iv, sizeof(iv),
        aad, aad_len,
        ciphertext, sizeof(ciphertext),
        tag, sizeof(tag)
    );
    
    if (ciphertext_len < 0) {
        printf("ERROR: ChaCha20-Poly1305 encryption failed\n");
        return -1;
    }
    
    printf("SUCCESS: Encrypted %zu bytes to %d bytes\n", plaintext_len, ciphertext_len);
    
    // Decrypt
    int decrypted_len = m17_chacha20_poly1305_decrypt(
        ciphertext, ciphertext_len,
        key, sizeof(key),
        iv, sizeof(iv),
        aad, aad_len,
        tag, sizeof(tag),
        decrypted, sizeof(decrypted)
    );
    
    if (decrypted_len < 0) {
        printf("ERROR: ChaCha20-Poly1305 decryption failed\n");
        return -1;
    }
    
    printf("SUCCESS: Decrypted %d bytes\n", decrypted_len);
    
    // Verify plaintext matches
    if (decrypted_len != (int)plaintext_len || memcmp(plaintext, decrypted, plaintext_len) != 0) {
        printf("ERROR: Decrypted text does not match original\n");
        return -1;
    }
    
    printf("SUCCESS: Plaintext matches after decryption\n");
    return 0;
}

// Test ChaCha20-Poly1305 with different message sizes
int test_chacha20_poly1305_sizes() {
    printf("Testing ChaCha20-Poly1305 with different message sizes...\n");
    
    uint8_t key[32];
    uint8_t iv[12];
    uint8_t aad[] = "M17-Size-Test";
    size_t aad_len = strlen((char*)aad);
    
    // Test different message sizes
    size_t test_sizes[] = {1, 16, 32, 64, 128, 256, 512, 1024};
    int num_sizes = sizeof(test_sizes) / sizeof(test_sizes[0]);
    
    for (int i = 0; i < num_sizes; i++) {
        size_t msg_len = test_sizes[i];
        uint8_t* plaintext = malloc(msg_len);
        uint8_t* ciphertext = malloc(msg_len + 16);  // Extra space for padding
        uint8_t* decrypted = malloc(msg_len);
        uint8_t tag[16];
        
        if (!plaintext || !ciphertext || !decrypted) {
            printf("ERROR: Memory allocation failed for size %zu\n", msg_len);
            free(plaintext);
            free(ciphertext);
            free(decrypted);
            return -1;
        }
        
        // Fill with test data
        for (size_t j = 0; j < msg_len; j++) {
            plaintext[j] = (uint8_t)(j % 256);
        }
        
        // Generate fresh key and IV for each test
        if (m17_chacha20_generate_key(key, sizeof(key)) != 0) {
            printf("ERROR: Failed to generate key for size %zu\n", msg_len);
            free(plaintext);
            free(ciphertext);
            free(decrypted);
            return -1;
        }
        
        if (m17_chacha20_generate_iv(iv, sizeof(iv)) != 0) {
            printf("ERROR: Failed to generate IV for size %zu\n", msg_len);
            free(plaintext);
            free(ciphertext);
            free(decrypted);
            return -1;
        }
        
        // Encrypt
        int ciphertext_len = m17_chacha20_poly1305_encrypt(
            plaintext, msg_len,
            key, sizeof(key),
            iv, sizeof(iv),
            aad, aad_len,
            ciphertext, msg_len + 16,
            tag, sizeof(tag)
        );
        
        if (ciphertext_len < 0) {
            printf("ERROR: Encryption failed for size %zu\n", msg_len);
            free(plaintext);
            free(ciphertext);
            free(decrypted);
            return -1;
        }
        
        // Decrypt
        int decrypted_len = m17_chacha20_poly1305_decrypt(
            ciphertext, ciphertext_len,
            key, sizeof(key),
            iv, sizeof(iv),
            aad, aad_len,
            tag, sizeof(tag),
            decrypted, msg_len
        );
        
        if (decrypted_len < 0 || decrypted_len != (int)msg_len) {
            printf("ERROR: Decryption failed for size %zu\n", msg_len);
            free(plaintext);
            free(ciphertext);
            free(decrypted);
            return -1;
        }
        
        // Verify
        if (memcmp(plaintext, decrypted, msg_len) != 0) {
            printf("ERROR: Data mismatch for size %zu\n", msg_len);
            free(plaintext);
            free(ciphertext);
            free(decrypted);
            return -1;
        }
        
        printf("SUCCESS: Size %zu bytes\n", msg_len);
        
        free(plaintext);
        free(ciphertext);
        free(decrypted);
    }
    
    return 0;
}

// Test ChaCha20-Poly1305 authentication failure
int test_chacha20_poly1305_auth_failure() {
    printf("Testing ChaCha20-Poly1305 authentication failure...\n");
    
    const char* plaintext = "Test message for authentication failure test";
    size_t plaintext_len = strlen(plaintext);
    
    uint8_t key[32];
    uint8_t iv[12];
    uint8_t aad[] = "M17-Auth-Test";
    size_t aad_len = strlen((char*)aad);
    
    uint8_t ciphertext[256];
    uint8_t tag[16];
    uint8_t decrypted[256];
    
    // Generate test key and IV
    if (m17_chacha20_generate_key(key, sizeof(key)) != 0) {
        printf("ERROR: Failed to generate ChaCha20 key\n");
        return -1;
    }
    
    if (m17_chacha20_generate_iv(iv, sizeof(iv)) != 0) {
        printf("ERROR: Failed to generate ChaCha20 IV\n");
        return -1;
    }
    
    // Encrypt
    int ciphertext_len = m17_chacha20_poly1305_encrypt(
        (uint8_t*)plaintext, plaintext_len,
        key, sizeof(key),
        iv, sizeof(iv),
        aad, aad_len,
        ciphertext, sizeof(ciphertext),
        tag, sizeof(tag)
    );
    
    if (ciphertext_len < 0) {
        printf("ERROR: ChaCha20-Poly1305 encryption failed\n");
        return -1;
    }
    
    // Corrupt the tag
    tag[0] ^= 0xFF;
    
    // Try to decrypt with corrupted tag
    int decrypted_len = m17_chacha20_poly1305_decrypt(
        ciphertext, ciphertext_len,
        key, sizeof(key),
        iv, sizeof(iv),
        aad, aad_len,
        tag, sizeof(tag),
        decrypted, sizeof(decrypted)
    );
    
    if (decrypted_len >= 0) {
        printf("ERROR: Decryption should have failed with corrupted tag\n");
        return -1;
    }
    
    printf("SUCCESS: Authentication failure correctly detected\n");
    return 0;
}

// Test ChaCha20 key validation
int test_chacha20_validation() {
    printf("Testing ChaCha20 key and IV validation...\n");
    
    uint8_t weak_key[32] = {0};  // All zeros
    uint8_t weak_iv[12] = {0};   // All zeros
    
    uint8_t strong_key[32];
    uint8_t strong_iv[12];
    
    // Test weak key detection
    if (m17_chacha20_validate_key(weak_key, sizeof(weak_key)) == 0) {
        printf("ERROR: Weak key should have been rejected\n");
        return -1;
    }
    
    // Test weak IV detection
    if (m17_chacha20_validate_iv(weak_iv, sizeof(weak_iv)) == 0) {
        printf("ERROR: Weak IV should have been rejected\n");
        return -1;
    }
    
    // Generate strong key and IV
    if (m17_chacha20_generate_key(strong_key, sizeof(strong_key)) != 0) {
        printf("ERROR: Failed to generate strong key\n");
        return -1;
    }
    
    if (m17_chacha20_generate_iv(strong_iv, sizeof(strong_iv)) != 0) {
        printf("ERROR: Failed to generate strong IV\n");
        return -1;
    }
    
    // Test strong key validation
    if (m17_chacha20_validate_key(strong_key, sizeof(strong_key)) != 0) {
        printf("ERROR: Strong key should have been accepted\n");
        return -1;
    }
    
    // Test strong IV validation
    if (m17_chacha20_validate_iv(strong_iv, sizeof(strong_iv)) != 0) {
        printf("ERROR: Strong IV should have been accepted\n");
        return -1;
    }
    
    printf("SUCCESS: Key and IV validation working correctly\n");
    return 0;
}

// Test ChaCha20 key derivation
int test_chacha20_key_derivation() {
    printf("Testing ChaCha20 key derivation...\n");
    
    uint8_t shared_secret[32];
    uint8_t salt[16] = "M17-Salt-Test";
    uint8_t info[] = "M17-ChaCha20-Key";
    uint8_t derived_key[32];
    
    // Generate test shared secret
    if (m17_chacha20_generate_key(shared_secret, sizeof(shared_secret)) != 0) {
        printf("ERROR: Failed to generate shared secret\n");
        return -1;
    }
    
    // Derive key
    if (m17_chacha20_derive_key(shared_secret, sizeof(shared_secret),
                               salt, sizeof(salt),
                               info, sizeof(info) - 1,
                               derived_key, sizeof(derived_key)) != 0) {
        printf("ERROR: Failed to derive key\n");
        return -1;
    }
    
    // Validate derived key
    if (m17_chacha20_validate_key(derived_key, sizeof(derived_key)) != 0) {
        printf("ERROR: Derived key is weak\n");
        return -1;
    }
    
    printf("SUCCESS: Key derivation working correctly\n");
    return 0;
}

int main() {
    printf("M17 ChaCha20-Poly1305 Test Suite\n");
    printf("================================\n\n");
    
    int tests_passed = 0;
    int tests_total = 5;
    
    if (test_chacha20_poly1305_basic() == 0) {
        tests_passed++;
        printf("PASSED: Basic encryption/decryption test\n\n");
    } else {
        printf("FAILED: Basic encryption/decryption test\n\n");
    }
    
    if (test_chacha20_poly1305_sizes() == 0) {
        tests_passed++;
        printf("PASSED: Different message sizes test\n\n");
    } else {
        printf("FAILED: Different message sizes test\n\n");
    }
    
    if (test_chacha20_poly1305_auth_failure() == 0) {
        tests_passed++;
        printf("PASSED: Authentication failure test\n\n");
    } else {
        printf("FAILED: Authentication failure test\n\n");
    }
    
    if (test_chacha20_validation() == 0) {
        tests_passed++;
        printf("PASSED: Key and IV validation test\n\n");
    } else {
        printf("FAILED: Key and IV validation test\n\n");
    }
    
    if (test_chacha20_key_derivation() == 0) {
        tests_passed++;
        printf("PASSED: Key derivation test\n\n");
    } else {
        printf("FAILED: Key derivation test\n\n");
    }
    
    printf("Test Results: %d/%d tests passed\n", tests_passed, tests_total);
    
    if (tests_passed == tests_total) {
        printf("ALL TESTS PASSED!\n");
        return 0;
    } else {
        printf("SOME TESTS FAILED!\n");
        return 1;
    }
}
