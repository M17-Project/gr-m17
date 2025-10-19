//--------------------------------------------------------------------
// M17 C library - examples/chacha20_poly1305_example.c
//
// Example usage of ChaCha20-Poly1305 authenticated encryption
// Demonstrates encryption, decryption, and key management
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------

#include "m17.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main() {
    printf("M17 ChaCha20-Poly1305 Example\n");
    printf("=============================\n\n");
    
    // Example message to encrypt
    const char* message = "Hello from M17! This message is encrypted with ChaCha20-Poly1305.";
    size_t message_len = strlen(message);
    
    printf("Original message: %s\n", message);
    printf("Message length: %zu bytes\n\n", message_len);
    
    // Generate encryption key and IV
    uint8_t key[32];
    uint8_t iv[12];
    
    printf("Generating ChaCha20 key and IV...\n");
    if (m17_chacha20_generate_key(key, sizeof(key)) != 0) {
        printf("ERROR: Failed to generate key\n");
        return 1;
    }
    
    if (m17_chacha20_generate_iv(iv, sizeof(iv)) != 0) {
        printf("ERROR: Failed to generate IV\n");
        return 1;
    }
    
    printf("SUCCESS: Generated 32-byte key and 12-byte IV\n\n");
    
    // Additional authenticated data (AAD)
    const char* aad = "M17-ChaCha20-Example";
    size_t aad_len = strlen(aad);
    
    printf("AAD: %s (length: %zu bytes)\n\n", aad, aad_len);
    
    // Allocate buffers for encryption
    size_t ciphertext_size = message_len + 16;  // Extra space for padding
    uint8_t* ciphertext = malloc(ciphertext_size);
    uint8_t tag[16];
    
    if (!ciphertext) {
        printf("ERROR: Memory allocation failed\n");
        return 1;
    }
    
    // Encrypt the message
    printf("Encrypting message with ChaCha20-Poly1305...\n");
    int ciphertext_len = m17_chacha20_poly1305_encrypt(
        (uint8_t*)message, message_len,
        key, sizeof(key),
        iv, sizeof(iv),
        (uint8_t*)aad, aad_len,
        ciphertext, ciphertext_size,
        tag, sizeof(tag)
    );
    
    if (ciphertext_len < 0) {
        printf("ERROR: Encryption failed\n");
        free(ciphertext);
        return 1;
    }
    
    printf("SUCCESS: Encrypted %zu bytes to %d bytes\n", message_len, ciphertext_len);
    printf("Authentication tag: ");
    for (int i = 0; i < 16; i++) {
        printf("%02x", tag[i]);
    }
    printf("\n\n");
    
    // Allocate buffer for decryption
    uint8_t* decrypted = malloc(message_len + 1);
    if (!decrypted) {
        printf("ERROR: Memory allocation failed\n");
        free(ciphertext);
        return 1;
    }
    
    // Decrypt the message
    printf("Decrypting message with ChaCha20-Poly1305...\n");
    int decrypted_len = m17_chacha20_poly1305_decrypt(
        ciphertext, ciphertext_len,
        key, sizeof(key),
        iv, sizeof(iv),
        (uint8_t*)aad, aad_len,
        tag, sizeof(tag),
        decrypted, message_len
    );
    
    if (decrypted_len < 0) {
        printf("ERROR: Decryption failed\n");
        free(ciphertext);
        free(decrypted);
        return 1;
    }
    
    printf("SUCCESS: Decrypted %d bytes\n", decrypted_len);
    
    // Null-terminate for printing
    decrypted[decrypted_len] = '\0';
    
    printf("Decrypted message: %s\n\n", decrypted);
    
    // Verify the decrypted message matches the original
    if (decrypted_len == (int)message_len && memcmp(message, decrypted, message_len) == 0) {
        printf("SUCCESS: Decrypted message matches original!\n");
    } else {
        printf("ERROR: Decrypted message does not match original\n");
        free(ciphertext);
        free(decrypted);
        return 1;
    }
    
    // Demonstrate authentication failure
    printf("\nTesting authentication failure...\n");
    
    // Corrupt the authentication tag
    tag[0] ^= 0xFF;
    
    // Try to decrypt with corrupted tag
    int failed_len = m17_chacha20_poly1305_decrypt(
        ciphertext, ciphertext_len,
        key, sizeof(key),
        iv, sizeof(iv),
        (uint8_t*)aad, aad_len,
        tag, sizeof(tag),
        decrypted, message_len
    );
    
    if (failed_len < 0) {
        printf("SUCCESS: Authentication failure correctly detected\n");
    } else {
        printf("ERROR: Authentication failure should have been detected\n");
    }
    
    // Demonstrate key derivation
    printf("\nDemonstrating key derivation...\n");
    
    uint8_t shared_secret[32];
    uint8_t salt[16] = "M17-Salt-Example";
    uint8_t info[] = "M17-ChaCha20-Derived";
    uint8_t derived_key[32];
    
    // Generate a shared secret (in real usage, this would come from ECDH)
    if (m17_chacha20_generate_key(shared_secret, sizeof(shared_secret)) != 0) {
        printf("ERROR: Failed to generate shared secret\n");
        free(ciphertext);
        free(decrypted);
        return 1;
    }
    
    // Derive a key from the shared secret
    if (m17_chacha20_derive_key(shared_secret, sizeof(shared_secret),
                               salt, sizeof(salt),
                               info, sizeof(info) - 1,
                               derived_key, sizeof(derived_key)) != 0) {
        printf("ERROR: Failed to derive key\n");
        free(ciphertext);
        free(decrypted);
        return 1;
    }
    
    printf("SUCCESS: Derived 32-byte key from shared secret\n");
    
    // Clean up
    free(ciphertext);
    free(decrypted);
    
    // Securely wipe sensitive data
    m17_chacha20_secure_wipe(key, sizeof(key));
    m17_chacha20_secure_wipe(iv, sizeof(iv));
    m17_chacha20_secure_wipe(shared_secret, sizeof(shared_secret));
    m17_chacha20_secure_wipe(derived_key, sizeof(derived_key));
    
    printf("\nExample completed successfully!\n");
    return 0;
}
