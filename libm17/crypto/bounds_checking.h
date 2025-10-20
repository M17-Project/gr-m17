//--------------------------------------------------------------------
// M17 C library - crypto/bounds_checking.h
//
// Buffer overflow protection and bounds checking utilities
// Implements safe memory operations and bounds validation
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------

#ifndef M17_BOUNDS_CHECKING_H
#define M17_BOUNDS_CHECKING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Safe memory copy with bounds checking
int m17_safe_memcpy(void *dest, size_t dest_size, 
                   const void *src, size_t src_size);

// Safe memory move with bounds checking
int m17_safe_memmove(void *dest, size_t dest_size,
                    const void *src, size_t src_size);

// Safe string copy with bounds checking
int m17_safe_strcpy(char *dest, size_t dest_size, const char *src);

// Safe string concatenation with bounds checking
int m17_safe_strcat(char *dest, size_t dest_size, const char *src);

// Safe UTF-8 string parsing with bounds checking
int m17_safe_utf8_parse(const char *input, size_t input_len,
                        uint8_t *output, size_t output_size,
                        size_t *bytes_written);

// Safe buffer bounds validation
bool m17_validate_buffer_bounds(const void *ptr, size_t size, 
                               size_t offset, size_t length);

// Safe array access with bounds checking
int m17_safe_array_access(const void *array, size_t element_size, 
                         size_t array_length, size_t index,
                         void *element);

// Safe pointer arithmetic with bounds checking
void* m17_safe_pointer_add(const void *ptr, size_t offset, size_t max_size);

// Safe memory allocation with bounds checking
void* m17_safe_malloc(size_t size);

// Safe memory reallocation with bounds checking
void* m17_safe_realloc(void *ptr, size_t new_size);

// Safe integer operations with overflow checking
bool m17_safe_add(size_t a, size_t b, size_t *result);
bool m17_safe_multiply(size_t a, size_t b, size_t *result);

// Validate string bounds
bool m17_validate_string_bounds(const char *str, size_t max_length);

// Safe string length with bounds checking
size_t m17_safe_strlen(const char *str, size_t max_length);

#ifdef __cplusplus
}
#endif

#endif // M17_BOUNDS_CHECKING_H










