#include <iostream>
#include <cstring>
#include <cstdint>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/aes.h>

// Test real cryptographic operations with fuzzed inputs

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    // Test AES encryption/decryption
    if (size >= 48) {  // 32-byte key + 16-byte IV
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (ctx) {
            const uint8_t* key = data;
            const uint8_t* iv = data + 32;
            uint8_t plaintext[16] = {0};
            uint8_t ciphertext[32];
            int len;
            
            // Test encryption (may crash with invalid inputs)
            EVP_EncryptInit_ex(ctx, EVP_aes_256_ctr(), NULL, key, iv);
            EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, 16);
            EVP_EncryptFinal_ex(ctx, ciphertext + len, &len);
            
            EVP_CIPHER_CTX_free(ctx);
        }
    }
    
    // Test SHA-256 hashing
    if (size > 0) {
        EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
        if (md_ctx) {
            uint8_t hash[32];
            unsigned int hash_len;
            
            EVP_DigestInit_ex(md_ctx, EVP_sha256(), NULL);
            EVP_DigestUpdate(md_ctx, data, size);
            EVP_DigestFinal_ex(md_ctx, hash, &hash_len);
            
            EVP_MD_CTX_free(md_ctx);
        }
    }
    
    // Test signature operations (simulated)
    if (size >= 64) {
        uint8_t signature[64];
        memcpy(signature, data, 64);
        
        // Test signature parsing
        bool all_zeros = true;
        for (int i = 0; i < 64; i++) {
            if (signature[i] != 0) {
                all_zeros = false;
                break;
            }
        }
    }
    
    // Test scrambler operations
    if (size >= 3) {
        uint32_t seed = (data[0] << 16) | (data[1] << 8) | data[2];
        
        // Simulate scrambler LFSR
        uint8_t pn[128];
        uint32_t lfsr = seed;
        for (int i = 0; i < 128; i++) {
            uint32_t bit = (lfsr >> 23) ^ (lfsr >> 22) ^ (lfsr >> 21) ^ (lfsr >> 16);
            bit &= 1;
            lfsr = (lfsr << 1) | bit;
            lfsr &= 0xFFFFFF;
            pn[i] = bit;
        }
    }
    
    return 0;
}
