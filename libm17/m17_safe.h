//--------------------------------------------------------------------
// M17 C library - m17_safe.h
//
// Safety and error handling utilities for M17 library
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 12 March 2025
//--------------------------------------------------------------------
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Error codes for M17 operations
typedef enum {
    M17_SUCCESS = 0,
    M17_ERROR_NULL_POINTER,
    M17_ERROR_INVALID_PARAM,
    M17_ERROR_BUFFER_OVERFLOW,
    M17_ERROR_INVALID_LENGTH,
    M17_ERROR_INVALID_SYNCWORD,
    M17_ERROR_DECODE_FAILED,
    M17_ERROR_CRC_MISMATCH,
    M17_ERROR_INVALID_CALLSIGN,
    M17_ERROR_INVALID_FRAME_TYPE,
    M17_ERROR_MEMORY_ALLOCATION,
    M17_ERROR_THREAD_SAFETY,
    M17_ERROR_INTERNAL
} m17_error_t;

// Thread safety macros
#ifdef M17_THREAD_SAFE
#include <pthread.h>
#define M17_MUTEX_DECLARE(name) static pthread_mutex_t name = PTHREAD_MUTEX_INITIALIZER
#define M17_MUTEX_LOCK(mutex) pthread_mutex_lock(&mutex)
#define M17_MUTEX_UNLOCK(mutex) pthread_mutex_unlock(&mutex)
#else
#define M17_MUTEX_DECLARE(name)
#define M17_MUTEX_LOCK(mutex)
#define M17_MUTEX_UNLOCK(mutex)
#endif

// Bounds checking macros
#define M17_BOUNDS_CHECK(ptr, size, max_size) \
    do { \
        if ((ptr) == NULL || (size) > (max_size) || (size) == 0) { \
            return M17_ERROR_INVALID_PARAM; \
        } \
    } while(0)

#define M17_BUFFER_CHECK(ptr, size, max_size) \
    do { \
        if ((ptr) == NULL || (size) > (max_size)) { \
            return M17_ERROR_BUFFER_OVERFLOW; \
        } \
    } while(0)

// Safe memory operations
int m17_safe_memcpy(void* dest, size_t dest_size, const void* src, size_t src_size);
m17_error_t m17_safe_memset(void* dest, size_t dest_size, int value, size_t count);

// Input validation functions
m17_error_t m17_validate_callsign(const char* callsign);
m17_error_t m17_validate_frame_type(uint8_t frame_type);
m17_error_t m17_validate_syncword(uint16_t syncword);

// Error reporting
const char* m17_error_string(m17_error_t error);

#ifdef __cplusplus
}
#endif

