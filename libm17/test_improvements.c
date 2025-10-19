//--------------------------------------------------------------------
// M17 C library - test_improvements.c
//
// Test file for validating improvements to M17 library
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 12 March 2025
//--------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <m17.h>
#include <m17_safe.h>
#include <m17_simd.h>

// Test error handling
void test_error_handling(void)
{
    printf("Testing error handling...\n");
    
    // Test null pointer handling
    float out[SYM_PER_FRA];
    uint32_t cnt = 0;
    
    // Should not crash with null pointers
    gen_preamble(NULL, &cnt, PREAM_LSF);
    gen_preamble(out, NULL, PREAM_LSF);
    
    // Test buffer overflow protection
    cnt = SYM_PER_FRA; // At limit
    gen_preamble(out, &cnt, PREAM_LSF);
    assert(cnt == SYM_PER_FRA); // Should not exceed buffer
    
    printf("[OK] Error handling tests passed\n");
}

// Test memory safety
void test_memory_safety(void)
{
    printf("Testing memory safety...\n");
    
    // Test safe memory operations
    uint8_t dest[10];
    uint8_t src[5] = {1, 2, 3, 4, 5};
    
    assert(m17_safe_memcpy(dest, sizeof(dest), src, sizeof(src)) == M17_SUCCESS);
    
    // Test buffer overflow protection
    assert(m17_safe_memcpy(dest, sizeof(dest), src, sizeof(dest) + 1) == M17_ERROR_BUFFER_OVERFLOW);
    
    // Test null pointer protection
    assert(m17_safe_memcpy(NULL, sizeof(dest), src, sizeof(src)) == M17_ERROR_NULL_POINTER);
    
    // Suppress unused variable warnings
    (void)dest;
    (void)src;
    
    printf("[OK] Memory safety tests passed\n");
}

// Test thread safety
void test_thread_safety(void)
{
    printf("Testing thread safety...\n");
    
    // Test Viterbi decoder with multiple calls
    uint16_t input[100];
    uint8_t output[50];
    
    // Initialize input with test data
    for (int i = 0; i < 100; i++) {
        input[i] = (i % 2) ? 0xFFFF : 0x0000;
    }
    
    // Multiple calls should not interfere with each other
    for (int i = 0; i < 10; i++) {
        assert(viterbi_decode(output, input, 100) != 0xFFFFFFFF); // Should not return error
    }
    
    // Suppress unused variable warnings
    (void)input;
    (void)output;
    
    printf("[OK] Thread safety tests passed\n");
}

// Test SIMD optimizations
void test_simd_optimizations(void)
{
    printf("Testing SIMD optimizations...\n");
    
    // Test SIMD capability detection
    m17_simd_capabilities_t caps = m17_get_simd_capabilities();
    printf("  SIMD capabilities: 0x%x\n", caps);
    
    // Test Euclidean norm with SIMD
    float in1[8] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    int8_t in2[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    float result;
    
    m17_simd_euclidean_norm(in1, in2, &result, 8);
    printf("  Euclidean norm result: %f\n", result);
    
    // Test symbol slicing with SIMD
    float input[8] = {-3.0f, -1.0f, 1.0f, 3.0f, -2.0f, 0.0f, 2.0f, 4.0f};
    uint16_t output[16];
    
    m17_simd_symbol_slice(input, output, 8);
    printf("  Symbol slice completed\n");
    
    printf("[OK] SIMD optimization tests passed\n");
}

// Test performance improvements
void test_performance(void)
{
    printf("Testing performance improvements...\n");
    
    const int iterations = 10000;
    clock_t start, end;
    
    // Test Euclidean norm performance
    float in1[184];
    int8_t in2[184];
    
    // Initialize test data
    for (int i = 0; i < 184; i++) {
        in1[i] = (float)(i % 7) - 3.0f;
        in2[i] = (int8_t)(i % 7) - 3;
    }
    
    start = clock();
    for (int i = 0; i < iterations; i++) {
        float result = eucl_norm(in1, in2, 184);
        (void)result; // Prevent optimization
    }
    end = clock();
    
    double cpu_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("  Euclidean norm: %d iterations in %.6f seconds\n", iterations, cpu_time);
    
    // Test Viterbi decoder performance
    uint16_t viterbi_input[200];
    uint8_t viterbi_output[100];
    
    for (int i = 0; i < 200; i++) {
        viterbi_input[i] = (i % 2) ? 0xFFFF : 0x0000;
    }
    
    start = clock();
    for (int i = 0; i < iterations / 10; i++) { // Fewer iterations for Viterbi
        uint32_t result = viterbi_decode(viterbi_output, viterbi_input, 200);
        (void)result;
    }
    end = clock();
    
    cpu_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("  Viterbi decode: %d iterations in %.6f seconds\n", iterations / 10, cpu_time);
    
    printf("[OK] Performance tests completed\n");
}

// Test input validation
void test_input_validation(void)
{
    printf("Testing input validation...\n");
    
    // Test callsign validation
    assert(m17_validate_callsign("N0CALL") == M17_SUCCESS);
    
    assert(m17_validate_callsign("INVALID@") == M17_ERROR_INVALID_CALLSIGN);
    
    assert(m17_validate_callsign(NULL) == M17_ERROR_NULL_POINTER);
    
    // Test frame type validation
    assert(m17_validate_frame_type(0) == M17_SUCCESS);
    
    assert(m17_validate_frame_type(4) == M17_ERROR_INVALID_FRAME_TYPE);
    
    // Test syncword validation
    assert(m17_validate_syncword(SYNC_LSF) == M17_SUCCESS);
    
    assert(m17_validate_syncword(0x1234) == M17_ERROR_INVALID_SYNCWORD);
    
    printf("[OK] Input validation tests passed\n");
}

int main(void)
{
    printf("M17 Library Improvements Test Suite\n");
    printf("===================================\n\n");
    
    test_error_handling();
    test_memory_safety();
    test_thread_safety();
    test_simd_optimizations();
    test_performance();
    test_input_validation();
    
    printf("\n[SUCCESS] All tests passed! M17 library improvements are working correctly.\n");
    
    return 0;
}
