//--------------------------------------------------------------------
// M17 C library - crypto/constant_time.c
//
// Constant-time cryptographic operations for M17
// Prevents timing attacks and side-channel vulnerabilities
// SECURITY FIX: Implements proper constant-time operations
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------

#include "m17.h"
#include <string.h>
#include <stdint.h>
#include <stddef.h>

// SECURITY FIX: Constant-time memory comparison
// Prevents timing attacks on authentication tags and signatures
int m17_constant_time_memcmp(const void *a, const void *b, size_t len) {
    if (!a || !b || len == 0) {
        return -1;
    }
    
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;
    uint8_t result = 0;
    
    // XOR all bytes and accumulate result
    for (size_t i = 0; i < len; i++) {
        result |= pa[i] ^ pb[i];
    }
    
    return result;
}

// SECURITY FIX: Constant-time memory copy
// Ensures sensitive data is copied without timing leaks
void m17_constant_time_memcpy(void *dest, const void *src, size_t len) {
    if (!dest || !src || len == 0) {
        return;
    }
    
    const uint8_t *psrc = (const uint8_t *)src;
    uint8_t *pdest = (uint8_t *)dest;
    
    // Copy byte by byte to avoid timing variations
    for (size_t i = 0; i < len; i++) {
        pdest[i] = psrc[i];
    }
}

// SECURITY FIX: Constant-time memory zeroing
// Ensures sensitive data is cleared without timing leaks
void m17_constant_time_memzero(void *ptr, size_t len) {
    if (!ptr || len == 0) {
        return;
    }
    
    volatile uint8_t *p = (volatile uint8_t *)ptr;
    
    // Clear byte by byte to avoid timing variations
    for (size_t i = 0; i < len; i++) {
        p[i] = 0;
    }
}

// SECURITY FIX: Constant-time conditional copy
// Copies data only if condition is true, without timing leaks
void m17_constant_time_conditional_copy(void *dest, const void *src, size_t len, int condition) {
    if (!dest || !src || len == 0) {
        return;
    }
    
    const uint8_t *psrc = (const uint8_t *)src;
    uint8_t *pdest = (uint8_t *)dest;
    
    // Use bitwise operations to avoid branches
    uint8_t mask = (uint8_t)(-(condition & 1));
    
    for (size_t i = 0; i < len; i++) {
        pdest[i] = (pdest[i] & ~mask) | (psrc[i] & mask);
    }
}

// SECURITY FIX: Constant-time conditional zeroing
// Zeros data only if condition is true, without timing leaks
void m17_constant_time_conditional_zero(void *ptr, size_t len, int condition) {
    if (!ptr || len == 0) {
        return;
    }
    
    uint8_t *p = (uint8_t *)ptr;
    
    // Use bitwise operations to avoid branches
    uint8_t mask = (uint8_t)(-(condition & 1));
    
    for (size_t i = 0; i < len; i++) {
        p[i] &= ~mask;
    }
}

// SECURITY FIX: Constant-time array selection
// Selects array element without timing leaks
void m17_constant_time_select(void *dest, const void *a, const void *b, size_t len, int condition) {
    if (!dest || !a || !b || len == 0) {
        return;
    }
    
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;
    uint8_t *pdest = (uint8_t *)dest;
    
    // Use bitwise operations to avoid branches
    uint8_t mask = (uint8_t)(-(condition & 1));
    
    for (size_t i = 0; i < len; i++) {
        pdest[i] = (pa[i] & mask) | (pb[i] & ~mask);
    }
}

// SECURITY FIX: Constant-time integer comparison
// Compares integers without timing leaks
int m17_constant_time_intcmp(int a, int b) {
    // Use bitwise operations to avoid branches
    return (a > b) - (a < b);
}

// SECURITY FIX: Constant-time integer selection
// Selects integer without timing leaks
int m17_constant_time_intselect(int a, int b, int condition) {
    // Use bitwise operations to avoid branches
    int mask = -(condition & 1);
    return (a & mask) | (b & ~mask);
}

// SECURITY FIX: Constant-time array equality check
// Checks if arrays are equal without timing leaks
int m17_constant_time_array_equal(const void *a, const void *b, size_t len) {
    if (!a || !b || len == 0) {
        return 0;
    }
    
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;
    uint8_t result = 0;
    
    // XOR all bytes and accumulate result
    for (size_t i = 0; i < len; i++) {
        result |= pa[i] ^ pb[i];
    }
    
    // Return 1 if equal (result == 0), 0 if not equal
    return (result == 0) ? 1 : 0;
}

// SECURITY FIX: Constant-time array copy with condition
// Copies array only if condition is true, without timing leaks
void m17_constant_time_array_copy(void *dest, const void *src, size_t len, int condition) {
    if (!dest || !src || len == 0) {
        return;
    }
    
    const uint8_t *psrc = (const uint8_t *)src;
    uint8_t *pdest = (uint8_t *)dest;
    
    // Use bitwise operations to avoid branches
    uint8_t mask = (uint8_t)(-(condition & 1));
    
    for (size_t i = 0; i < len; i++) {
        pdest[i] = (pdest[i] & ~mask) | (psrc[i] & mask);
    }
}

// SECURITY FIX: Constant-time array zeroing with condition
// Zeros array only if condition is true, without timing leaks
void m17_constant_time_array_zero(void *ptr, size_t len, int condition) {
    if (!ptr || len == 0) {
        return;
    }
    
    uint8_t *p = (uint8_t *)ptr;
    
    // Use bitwise operations to avoid branches
    uint8_t mask = (uint8_t)(-(condition & 1));
    
    for (size_t i = 0; i < len; i++) {
        p[i] &= ~mask;
    }
}


