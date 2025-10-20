//--------------------------------------------------------------------
// M17 C library - crypto/secure_memory.h
//
// Secure memory handling utilities for cryptographic operations
// Implements secure memory wiping and protection mechanisms
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------

#ifndef M17_SECURE_MEMORY_H
#define M17_SECURE_MEMORY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Secure memory wiping function
// Wipes memory with multiple patterns to prevent recovery
void m17_secure_wipe(void *ptr, size_t len);

// Secure memory allocation with automatic wiping on free
void* m17_secure_alloc(size_t size);

// Secure memory reallocation
void* m17_secure_realloc(void *ptr, size_t old_size, size_t new_size);

// Secure memory free with wiping
void m17_secure_free(void *ptr, size_t size);

// Lock memory to prevent swapping to disk
int m17_secure_lock_memory(void *ptr, size_t size);

// Unlock memory
int m17_secure_unlock_memory(void *ptr, size_t size);

// Secure key storage structure (opaque)
typedef struct m17_secure_key_t m17_secure_key_t;

// Create secure key storage
m17_secure_key_t* m17_secure_key_create(size_t size);

// Lock key in memory
int m17_secure_key_lock(m17_secure_key_t *key);

// Unlock key from memory
int m17_secure_key_unlock(m17_secure_key_t *key);

// Destroy secure key storage
void m17_secure_key_destroy(m17_secure_key_t *key);

// Get key data (read-only access)
const uint8_t* m17_secure_key_get_data(const m17_secure_key_t *key);

// Get key size
size_t m17_secure_key_get_size(const m17_secure_key_t *key);

// Copy key data securely
int m17_secure_key_copy(m17_secure_key_t *dest, const uint8_t *src, size_t size);

#ifdef __cplusplus
}
#endif

#endif // M17_SECURE_MEMORY_H










