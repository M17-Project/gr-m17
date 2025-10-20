//--------------------------------------------------------------------
// M17 C library - m17_simd.c
//
// SIMD optimizations implementation for M17 library
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 12 March 2025
//--------------------------------------------------------------------
#include <m17_simd.h>
#include "m17.h"
#include <string.h>
#include <math.h>

// Bring in SIMD intrinsic headers before any use
#if defined(__SSE__) || defined(__SSE2__) || defined(__SSE3__) || defined(__SSSE3__) || defined(__SSE4_1__) || defined(__SSE4_2__) || defined(__AVX__) || defined(__AVX2__)
  #include <xmmintrin.h>   // SSE
  #include <emmintrin.h>   // SSE2
  #include <immintrin.h>   // AVX/AVX2 and others
#endif

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
  #include <arm_neon.h>
#endif

// SIMD capability detection
m17_simd_capabilities_t m17_get_simd_capabilities(void)
{
    m17_simd_capabilities_t caps = M17_SIMD_NONE;
    
#ifdef __x86_64__
    // Check CPUID for x86_64 capabilities
    uint32_t eax, ebx, ecx, edx;
    
    // Check basic CPUID
    __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0));
    if (eax >= 1) {
        __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
        
        if (edx & (1 << 26)) caps |= M17_SIMD_SSE2;
        if (ecx & (1 << 0))  caps |= M17_SIMD_SSE3;
        if (ecx & (1 << 9))  caps |= M17_SIMD_SSSE3;
        if (ecx & (1 << 19)) caps |= M17_SIMD_SSE4_1;
        if (ecx & (1 << 20)) caps |= M17_SIMD_SSE4_2;
        if (ecx & (1 << 28)) caps |= M17_SIMD_AVX;
    }
    
    // Check AVX2
    if (eax >= 7) {
        __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(7), "c"(0));
        if (ebx & (1 << 5)) caps |= M17_SIMD_AVX2;
    }
#endif

#ifdef __aarch64__
    // ARM64 always has NEON
    caps |= M17_SIMD_NEON;
#endif

    return caps;
}

// SIMD-optimized slice_symbols implementations
void slice_symbols_simd_sse(uint16_t* out, const float* inp) {
#ifdef __SSE2__
    // SSE2 implementation for slice_symbols
    // Process 4 symbols at a time using SSE2
    for (int i = 0; i < SYM_PER_PLD; i += 4) {
        // Load 4 input symbols
        __m128 input = _mm_loadu_ps(&inp[i]);
        
        // Compare against symbol thresholds
        __m128 thresh1 = _mm_set1_ps(symbol_list[1]);
        __m128 thresh2 = _mm_set1_ps(symbol_list[2]);
        __m128 thresh3 = _mm_set1_ps(symbol_list[3]);
        
        // Generate soft decisions
        __m128i bit0 = _mm_castps_si128(_mm_cmpge_ps(input, thresh2));
        __m128i bit1 = _mm_castps_si128(_mm_cmpge_ps(input, thresh1));
        
        // Store results using movemask to avoid non-const lane indices
        int mask1 = _mm_movemask_ps(_mm_castsi128_ps(bit1));
        int mask0 = _mm_movemask_ps(_mm_castsi128_ps(bit0));
        for (int j = 0; j < 4 && (i + j) < SYM_PER_PLD; j++) {
            out[(i + j) * 2]     = (mask1 & (1 << j)) ? 0xFFFF : 0x0000;
            out[(i + j) * 2 + 1] = (mask0 & (1 << j)) ? 0xFFFF : 0x0000;
        }
    }
#else
    // Fallback to scalar implementation
    for (int i = 0; i < SYM_PER_PLD; i++) {
        if (inp[i] >= symbol_list[3]) {
            out[i * 2 + 1] = 0xFFFF;
        } else if (inp[i] >= symbol_list[2]) {
            out[i * 2 + 1] = 0x0000;
        } else if (inp[i] >= symbol_list[1]) {
            out[i * 2 + 1] = 0x0000;
        } else if (inp[i] >= symbol_list[0]) {
            out[i * 2 + 1] = 0xFFFF;
        } else {
            out[i * 2 + 1] = 0xFFFF;
        }
        
        if (inp[i] >= symbol_list[2]) {
            out[i * 2] = 0x0000;
        } else if (inp[i] >= symbol_list[1]) {
            out[i * 2] = 0x7FFF;
        } else {
            out[i * 2] = 0xFFFF;
        }
    }
#endif
}

void slice_symbols_simd_avx(uint16_t* out, const float* inp) {
#ifdef __AVX__
    // AVX implementation for slice_symbols
    // Process 8 symbols at a time using AVX
    for (int i = 0; i < SYM_PER_PLD; i += 8) {
        // Load 8 input symbols
        __m256 input = _mm256_loadu_ps(&inp[i]);
        
        // Compare against symbol thresholds
        __m256 thresh1 = _mm256_set1_ps(symbol_list[1]);
        __m256 thresh2 = _mm256_set1_ps(symbol_list[2]);
        __m256 thresh3 = _mm256_set1_ps(symbol_list[3]);
        
        // Generate soft decisions
        __m256i bit0 = _mm256_castps_si256(_mm256_cmp_ps(input, thresh2, _CMP_GE_OQ));
        __m256i bit1 = _mm256_castps_si256(_mm256_cmp_ps(input, thresh1, _CMP_GE_OQ));
        
        // Store results using movemask to avoid lane extract
        int mask1 = _mm256_movemask_ps(_mm256_castsi256_ps(bit1));
        int mask0 = _mm256_movemask_ps(_mm256_castsi256_ps(bit0));
        for (int j = 0; j < 8 && (i + j) < SYM_PER_PLD; j++) {
            out[(i + j) * 2]     = (mask1 & (1 << j)) ? 0xFFFF : 0x0000;
            out[(i + j) * 2 + 1] = (mask0 & (1 << j)) ? 0xFFFF : 0x0000;
        }
    }
#else
    // Fallback to SSE implementation
    slice_symbols_simd_sse(out, inp);
#endif
}

void slice_symbols_simd_neon(uint16_t* out, const float* inp) {
#ifdef __ARM_NEON
    // NEON implementation for slice_symbols
    // Process 4 symbols at a time using NEON
    for (int i = 0; i < SYM_PER_PLD; i += 4) {
        // Load 4 input symbols
        float32x4_t input = vld1q_f32(&inp[i]);
        
        // Compare against symbol thresholds
        float32x4_t thresh1 = vdupq_n_f32(symbol_list[1]);
        float32x4_t thresh2 = vdupq_n_f32(symbol_list[2]);
        float32x4_t thresh3 = vdupq_n_f32(symbol_list[3]);
        
        // Generate soft decisions
        uint32x4_t bit0 = vcgeq_f32(input, thresh2);
        uint32x4_t bit1 = vcgeq_f32(input, thresh1);
        
        // Store results
        for (int j = 0; j < 4 && (i + j) < SYM_PER_PLD; j++) {
            out[(i + j) * 2] = vgetq_lane_u32(bit1, j) ? 0xFFFF : 0x0000;
            out[(i + j) * 2 + 1] = vgetq_lane_u32(bit0, j) ? 0xFFFF : 0x0000;
        }
    }
#else
    // Fallback to scalar implementation
    for (int i = 0; i < SYM_PER_PLD; i++) {
        if (inp[i] >= symbol_list[3]) {
            out[i * 2 + 1] = 0xFFFF;
        } else if (inp[i] >= symbol_list[2]) {
            out[i * 2 + 1] = 0x0000;
        } else if (inp[i] >= symbol_list[1]) {
            out[i * 2 + 1] = 0x0000;
        } else if (inp[i] >= symbol_list[0]) {
            out[i * 2 + 1] = 0xFFFF;
        } else {
            out[i * 2 + 1] = 0xFFFF;
        }
        
        if (inp[i] >= symbol_list[2]) {
            out[i * 2] = 0x0000;
        } else if (inp[i] >= symbol_list[1]) {
            out[i * 2] = 0x7FFF;
        } else {
            out[i * 2] = 0xFFFF;
        }
    }
#endif
}

// Scalar fallback implementations
void m17_scalar_euclidean_norm(const float* in1, const int8_t* in2, float* result, size_t n)
{
    float sum = 0.0f;
    for (size_t i = 0; i < n; i++) {
        float diff = in1[i] - (float)in2[i];
        sum += diff * diff;
    }
    *result = sqrtf(sum);
}

void m17_scalar_symbol_slice(const float* input, uint16_t* output, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (input[i] >= 3.0f) {
            output[i] = 0xFFFF;
        } else if (input[i] >= 1.0f) {
            output[i] = 0x7FFF;
        } else if (input[i] >= -1.0f) {
            output[i] = 0x0000;
        } else {
            output[i] = 0x8000;
        }
    }
}

void m17_scalar_soft_xor(const uint16_t* a, const uint16_t* b, uint16_t* out, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        // Soft XOR: if both are high confidence, result is low confidence
        // if one is high confidence, result follows that one
        if (a[i] > 0x7FFF && b[i] > 0x7FFF) {
            out[i] = 0x0000; // Both 1, XOR = 0
        } else if (a[i] > 0x7FFF && b[i] <= 0x7FFF) {
            out[i] = 0xFFFF; // One 1, one 0, XOR = 1
        } else if (a[i] <= 0x7FFF && b[i] > 0x7FFF) {
            out[i] = 0xFFFF; // One 0, one 1, XOR = 1
        } else {
            out[i] = 0x0000; // Both 0, XOR = 0
        }
    }
}

void m17_scalar_soft_add(const uint16_t* a, const uint16_t* b, uint16_t* out, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        uint32_t sum = (uint32_t)a[i] + (uint32_t)b[i];
        out[i] = (sum > 0xFFFF) ? 0xFFFF : (uint16_t)sum;
    }
}

// SIMD-optimized implementations
#ifdef __SSE2__
#include <emmintrin.h>

void m17_simd_euclidean_norm(const float* in1, const int8_t* in2, float* result, size_t n)
{
    __m128 sum = _mm_setzero_ps();
    size_t i;
    
    // Process 4 floats at a time
    for (i = 0; i < n - 3; i += 4) {
        __m128 v1 = _mm_loadu_ps(&in1[i]);
        __m128 v2 = _mm_set_ps(in2[i+3], in2[i+2], in2[i+1], in2[i]);
        __m128 diff = _mm_sub_ps(v1, v2);
        __m128 diff_sq = _mm_mul_ps(diff, diff);
        sum = _mm_add_ps(sum, diff_sq);
    }
    
    // Handle remaining elements
    for (; i < n; i++) {
        float diff = in1[i] - (float)in2[i];
        sum = _mm_add_ss(sum, _mm_set_ss(diff * diff));
    }
    
    // Horizontal sum
    __m128 shuf = _mm_movehl_ps(sum, sum);
    __m128 sums = _mm_add_ps(sum, shuf);
    shuf = _mm_shuffle_ps(sums, sums, 1);
    sums = _mm_add_ss(sums, shuf);
    
    *result = sqrtf(_mm_cvtss_f32(sums));
}

void m17_simd_symbol_slice(const float* input, uint16_t* output, size_t n)
{
    __m128 thresh1 = _mm_set1_ps(3.0f);
    __m128 thresh2 = _mm_set1_ps(1.0f);
    __m128 thresh3 = _mm_set1_ps(-1.0f);
    
    size_t i;
    for (i = 0; i < n - 3; i += 4) {
        __m128 v = _mm_loadu_ps(&input[i]);
        __m128 cmp1 = _mm_cmpge_ps(v, thresh1);
        __m128 cmp2 = _mm_cmpge_ps(v, thresh2);
        __m128 cmp3 = _mm_cmpge_ps(v, thresh3);
        
        // Convert to uint16_t values
        for (int j = 0; j < 4; j++) {
            if (_mm_movemask_ps(cmp1) & (1 << j)) {
                output[i + j] = 0xFFFF;
            } else if (_mm_movemask_ps(cmp2) & (1 << j)) {
                output[i + j] = 0x7FFF;
            } else if (_mm_movemask_ps(cmp3) & (1 << j)) {
                output[i + j] = 0x0000;
            } else {
                output[i + j] = 0x8000;
            }
        }
    }
    
    // Handle remaining elements
    for (; i < n; i++) {
        if (input[i] >= 3.0f) {
            output[i] = 0xFFFF;
        } else if (input[i] >= 1.0f) {
            output[i] = 0x7FFF;
        } else if (input[i] >= -1.0f) {
            output[i] = 0x0000;
        } else {
            output[i] = 0x8000;
        }
    }
}
#endif

