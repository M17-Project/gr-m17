//--------------------------------------------------------------------
// M17 C library - crypto/bounds_checking.c
//
// Buffer overflow protection and bounds checking utilities
// Implements safe memory operations and bounds validation
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------

#include "m17.h"
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

// Safe memory copy with bounds checking
// Function moved to m17_safe.c to avoid duplication

// Safe memory move with bounds checking
int m17_safe_memmove(void *dest, size_t dest_size,
                    const void *src, size_t src_size) {
    if (dest == NULL || src == NULL) {
        return -1;
    }
    
    if (dest_size == 0 || src_size == 0) {
        return -1;
    }
    
    if (src_size > dest_size) {
        return -1; // Buffer overflow would occur
    }
    
    memmove(dest, src, src_size);
    return 0;
}

// Safe string copy with bounds checking
int m17_safe_strcpy(char *dest, size_t dest_size, const char *src) {
    if (dest == NULL || src == NULL) {
        return -1;
    }
    
    if (dest_size == 0) {
        return -1;
    }
    
    size_t src_len = strlen(src);
    if (src_len >= dest_size) {
        return -1; // Would overflow
    }
    
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0'; // Ensure null termination
    return 0;
}

// Safe string concatenation with bounds checking
int m17_safe_strcat(char *dest, size_t dest_size, const char *src) {
    if (dest == NULL || src == NULL) {
        return -1;
    }
    
    if (dest_size == 0) {
        return -1;
    }
    
    size_t dest_len = strlen(dest);
    size_t src_len = strlen(src);
    
    if (dest_len + src_len >= dest_size) {
        return -1; // Would overflow
    }
    
    strncat(dest, src, dest_size - dest_len - 1);
    return 0;
}

// Safe UTF-8 string parsing with bounds checking
int m17_safe_utf8_parse(const char *input, size_t input_len,
                        uint8_t *output, size_t output_size,
                        size_t *bytes_written) {
    if (input == NULL || output == NULL || bytes_written == NULL) {
        return -1;
    }
    
    if (input_len == 0 || output_size == 0) {
        return -1;
    }
    
    *bytes_written = 0;
    size_t i = 0, j = 0;
    
    while (i < input_len && j < output_size) {
        if ((unsigned int)input[i] < 0xc2) {
            // Single byte character
            output[j] = (uint8_t)input[i];
            i++;
            j++;
        } else {
            // Multi-byte UTF-8 character
            if (i + 1 >= input_len) {
                return -1; // Incomplete UTF-8 sequence
            }
            
            if (j >= output_size) {
                return -1; // Output buffer full
            }
            
            output[j] = (input[i] - 0xc2) * 0x40 + input[i + 1];
            i += 2;
            j++;
        }
    }
    
    *bytes_written = j;
    return 0;
}

// Function moved to validation.c to avoid duplication

// Safe array access with bounds checking
int m17_safe_array_access(const void *array, size_t element_size, 
                         size_t array_length, size_t index,
                         void *element) {
    if (array == NULL || element == NULL) {
        return -1;
    }
    
    if (element_size == 0 || array_length == 0) {
        return -1;
    }
    
    if (index >= array_length) {
        return -1; // Index out of bounds
    }
    
    const uint8_t *byte_array = (const uint8_t *)array;
    memcpy(element, &byte_array[index * element_size], element_size);
    
    return 0;
}

// Safe pointer arithmetic with bounds checking
void* m17_safe_pointer_add(const void *ptr, size_t offset, size_t max_size) {
    if (ptr == NULL) {
        return NULL;
    }
    
    if (offset > max_size) {
        return NULL; // Would exceed bounds
    }
    
    return (void*)((uint8_t*)ptr + offset);
}

// Safe memory allocation with bounds checking
void* m17_safe_malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    
    // Check for potential overflow
    if (size > SIZE_MAX / 2) {
        return NULL;
    }
    
    void *ptr = malloc(size);
    if (ptr != NULL) {
        // Initialize to zero for security
        memset(ptr, 0, size);
    }
    
    return ptr;
}

// Safe memory reallocation with bounds checking
void* m17_safe_realloc(void *ptr, size_t new_size) {
    if (new_size == 0) {
        if (ptr != NULL) {
            free(ptr);
        }
        return NULL;
    }
    
    // Check for potential overflow
    if (new_size > SIZE_MAX / 2) {
        return NULL;
    }
    
    void *new_ptr = realloc(ptr, new_size);
    if (new_ptr != NULL && ptr == NULL) {
        // Initialize new memory to zero
        memset(new_ptr, 0, new_size);
    }
    
    return new_ptr;
}

// Safe integer operations with overflow checking
bool m17_safe_add(size_t a, size_t b, size_t *result) {
    if (result == NULL) {
        return false;
    }
    
    if (a > SIZE_MAX - b) {
        return false; // Overflow would occur
    }
    
    *result = a + b;
    return true;
}

bool m17_safe_multiply(size_t a, size_t b, size_t *result) {
    if (result == NULL) {
        return false;
    }
    
    if (a == 0 || b == 0) {
        *result = 0;
        return true;
    }
    
    if (a > SIZE_MAX / b) {
        return false; // Overflow would occur
    }
    
    *result = a * b;
    return true;
}

// Validate string bounds
bool m17_validate_string_bounds(const char *str, size_t max_length) {
    if (str == NULL) {
        return false;
    }
    
    size_t len = strlen(str);
    return len <= max_length;
}

// Safe string length with bounds checking
size_t m17_safe_strlen(const char *str, size_t max_length) {
    if (str == NULL) {
        return 0;
    }
    
    size_t len = 0;
    while (len < max_length && str[len] != '\0') {
        len++;
    }
    
    return len;
}
