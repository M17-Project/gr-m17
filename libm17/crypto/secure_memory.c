//--------------------------------------------------------------------
// M17 C library - crypto/secure_memory.c
//
// Secure memory handling utilities for cryptographic operations
// Implements secure memory wiping and protection mechanisms
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------

#include "m17.h"
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

// Secure memory wiping function
void m17_secure_wipe(void *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return;
    }
    
    // Use volatile pointer to prevent compiler optimization
    volatile uint8_t *volatile_ptr = (volatile uint8_t *)ptr;
    
    // Multiple passes with different patterns to ensure complete wiping
    for (size_t i = 0; i < len; i++) {
        volatile_ptr[i] = 0x00;
    }
    
    for (size_t i = 0; i < len; i++) {
        volatile_ptr[i] = 0xFF;
    }
    
    for (size_t i = 0; i < len; i++) {
        volatile_ptr[i] = 0xAA;
    }
    
    for (size_t i = 0; i < len; i++) {
        volatile_ptr[i] = 0x55;
    }
    
    // Final zero pass
    for (size_t i = 0; i < len; i++) {
        volatile_ptr[i] = 0x00;
    }
    
    // Force memory barrier to ensure writes are completed
    __asm__ __volatile__("" ::: "memory");
}

// Secure memory allocation with automatic wiping on free
void* m17_secure_alloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    
    void *ptr = malloc(size);
    if (ptr == NULL) {
        return NULL;
    }
    
    // SECURITY FIX: Use secure memory clearing for sensitive data
    explicit_bzero(ptr, size);
    
    return ptr;
}

// Secure memory reallocation
void* m17_secure_realloc(void *ptr, size_t old_size, size_t new_size) {
    if (new_size == 0) {
        if (ptr != NULL && old_size > 0) {
            m17_secure_wipe(ptr, old_size);
            free(ptr);
        }
        return NULL;
    }
    
    void *new_ptr = realloc(ptr, new_size);
    if (new_ptr == NULL) {
        // If realloc failed, wipe the old memory before returning NULL
        if (ptr != NULL && old_size > 0) {
            m17_secure_wipe(ptr, old_size);
        }
        return NULL;
    }
    
    // If the size increased, zero out the new portion
    if (new_size > old_size) {
        // SECURITY FIX: Use secure memory clearing for sensitive data
        explicit_bzero((uint8_t*)new_ptr + old_size, new_size - old_size);
    }
    
    return new_ptr;
}

// Secure memory free with wiping
void m17_secure_free(void *ptr, size_t size) {
    if (ptr != NULL && size > 0) {
        m17_secure_wipe(ptr, size);
    }
    free(ptr);
}

// Lock memory to prevent swapping to disk
int m17_secure_lock_memory(void *ptr, size_t size) {
    if (ptr == NULL || size == 0) {
        return -1;
    }
    
    // Align to page boundary
    uintptr_t addr = (uintptr_t)ptr;
    uintptr_t page_size = getpagesize();
    uintptr_t aligned_addr = addr & ~(page_size - 1);
    size_t aligned_size = ((addr + size) - aligned_addr + page_size - 1) & ~(page_size - 1);
    
    return mlock((void*)aligned_addr, aligned_size);
}

// Unlock memory
int m17_secure_unlock_memory(void *ptr, size_t size) {
    if (ptr == NULL || size == 0) {
        return -1;
    }
    
    // Align to page boundary
    uintptr_t addr = (uintptr_t)ptr;
    uintptr_t page_size = getpagesize();
    uintptr_t aligned_addr = addr & ~(page_size - 1);
    size_t aligned_size = ((addr + size) - aligned_addr + page_size - 1) & ~(page_size - 1);
    
    return munlock((void*)aligned_addr, aligned_size);
}

// Secure key storage structure
struct m17_secure_key_t {
    uint8_t *key_data;
    size_t key_size;
    int locked;
};

// Create secure key storage
m17_secure_key_t* m17_secure_key_create(size_t size) {
    if (size == 0) {
        return NULL;
    }
    
    m17_secure_key_t *key = malloc(sizeof(m17_secure_key_t));
    if (key == NULL) {
        return NULL;
    }
    
    key->key_data = m17_secure_alloc(size);
    if (key->key_data == NULL) {
        free(key);
        return NULL;
    }
    
    key->key_size = size;
    key->locked = 0;
    
    return key;
}

// Lock key in memory
int m17_secure_key_lock(m17_secure_key_t *key) {
    if (key == NULL || key->key_data == NULL) {
        return -1;
    }
    
    if (key->locked) {
        return 0; // Already locked
    }
    
    int result = m17_secure_lock_memory(key->key_data, key->key_size);
    if (result == 0) {
        key->locked = 1;
    }
    
    return result;
}

// Unlock key from memory
int m17_secure_key_unlock(m17_secure_key_t *key) {
    if (key == NULL || key->key_data == NULL) {
        return -1;
    }
    
    if (!key->locked) {
        return 0; // Already unlocked
    }
    
    int result = m17_secure_unlock_memory(key->key_data, key->key_size);
    if (result == 0) {
        key->locked = 0;
    }
    
    return result;
}

// Destroy secure key storage
void m17_secure_key_destroy(m17_secure_key_t *key) {
    if (key == NULL) {
        return;
    }
    
    if (key->key_data != NULL) {
        m17_secure_key_unlock(key);
        m17_secure_free(key->key_data, key->key_size);
    }
    
    m17_secure_wipe(key, sizeof(m17_secure_key_t));
    free(key);
}

// Get key data (read-only access)
const uint8_t* m17_secure_key_get_data(const m17_secure_key_t *key) {
    if (key == NULL || key->key_data == NULL) {
        return NULL;
    }
    
    return key->key_data;
}

// Get key size
size_t m17_secure_key_get_size(const m17_secure_key_t *key) {
    if (key == NULL) {
        return 0;
    }
    
    return key->key_size;
}

// Copy key data securely
int m17_secure_key_copy(m17_secure_key_t *dest, const uint8_t *src, size_t size) {
    if (dest == NULL || src == NULL || size == 0) {
        return -1;
    }
    
    if (size > dest->key_size) {
        return -1; // Size mismatch
    }
    
    // Use secure copy to prevent timing attacks
    for (size_t i = 0; i < size; i++) {
        dest->key_data[i] = src[i];
    }
    
    // Zero out any remaining space
    if (size < dest->key_size) {
        // SECURITY FIX: Use secure memory clearing for sensitive data
        explicit_bzero(dest->key_data + size, dest->key_size - size);
    }
    
    return 0;
}
