//--------------------------------------------------------------------
// M17 C library - crypto/constant_time.h
//
// Constant-time cryptographic operations for M17
// Prevents timing attacks and side-channel vulnerabilities
// SECURITY FIX: Implements proper constant-time operations
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------

#ifndef M17_CONSTANT_TIME_H
#define M17_CONSTANT_TIME_H

#include "m17.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// SECURITY FIX: Constant-time memory comparison
// Prevents timing attacks on authentication tags and signatures
int m17_constant_time_memcmp(const void *a, const void *b, size_t len);

// SECURITY FIX: Constant-time memory copy
// Ensures sensitive data is copied without timing leaks
void m17_constant_time_memcpy(void *dest, const void *src, size_t len);

// SECURITY FIX: Constant-time memory zeroing
// Ensures sensitive data is cleared without timing leaks
void m17_constant_time_memzero(void *ptr, size_t len);

// SECURITY FIX: Constant-time conditional copy
// Copies data only if condition is true, without timing leaks
void m17_constant_time_conditional_copy(void *dest, const void *src, size_t len, int condition);

// SECURITY FIX: Constant-time conditional zeroing
// Zeros data only if condition is true, without timing leaks
void m17_constant_time_conditional_zero(void *ptr, size_t len, int condition);

// SECURITY FIX: Constant-time array selection
// Selects array element without timing leaks
void m17_constant_time_select(void *dest, const void *a, const void *b, size_t len, int condition);

// SECURITY FIX: Constant-time integer comparison
// Compares integers without timing leaks
int m17_constant_time_intcmp(int a, int b);

// SECURITY FIX: Constant-time integer selection
// Selects integer without timing leaks
int m17_constant_time_intselect(int a, int b, int condition);

// SECURITY FIX: Constant-time array equality check
// Checks if arrays are equal without timing leaks
int m17_constant_time_array_equal(const void *a, const void *b, size_t len);

// SECURITY FIX: Constant-time array copy with condition
// Copies array only if condition is true, without timing leaks
void m17_constant_time_array_copy(void *dest, const void *src, size_t len, int condition);

// SECURITY FIX: Constant-time array zeroing with condition
// Zeros array only if condition is true, without timing leaks
void m17_constant_time_array_zero(void *ptr, size_t len, int condition);

#ifdef __cplusplus
}
#endif

#endif // M17_CONSTANT_TIME_H


