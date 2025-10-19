//--------------------------------------------------------------------
// M17 C library - m17_simd.h
//
// SIMD optimizations for M17 library
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 12 March 2025
//--------------------------------------------------------------------
#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// SIMD detection and capabilities
typedef enum {
    M17_SIMD_NONE = 0,
    M17_SIMD_SSE2 = 1,
    M17_SIMD_SSE3 = 2,
    M17_SIMD_SSSE3 = 4,
    M17_SIMD_SSE4_1 = 8,
    M17_SIMD_SSE4_2 = 16,
    M17_SIMD_AVX = 32,
    M17_SIMD_AVX2 = 64,
    M17_SIMD_NEON = 128
} m17_simd_capabilities_t;

// Get available SIMD capabilities
m17_simd_capabilities_t m17_get_simd_capabilities(void);

// SIMD-optimized functions
void m17_simd_euclidean_norm(const float* in1, const int8_t* in2, float* result, size_t n);
void m17_simd_symbol_slice(const float* input, uint16_t* output, size_t n);
void m17_simd_soft_xor(const uint16_t* a, const uint16_t* b, uint16_t* out, size_t len);
void m17_simd_soft_add(const uint16_t* a, const uint16_t* b, uint16_t* out, size_t len);
void m17_simd_viterbi_metrics(const uint16_t* s0, const uint16_t* s1, uint32_t* metrics, size_t n);

// Fallback implementations (always available)
void m17_scalar_euclidean_norm(const float* in1, const int8_t* in2, float* result, size_t n);
void m17_scalar_symbol_slice(const float* input, uint16_t* output, size_t n);
void m17_scalar_soft_xor(const uint16_t* a, const uint16_t* b, uint16_t* out, size_t len);
void m17_scalar_soft_add(const uint16_t* a, const uint16_t* b, uint16_t* out, size_t len);

#ifdef __cplusplus
}
#endif


