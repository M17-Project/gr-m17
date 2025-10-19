//--------------------------------------------------------------------
// M17 C library - test_critical_security.c
//
// Critical security test suite for M17 cryptographic operations
// Tests all critical security fixes and protections
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------

#include "m17.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

// Test proper key derivation with context
void test_key_derivation_with_context() {
    printf("Testing key derivation with context...\n");
    
    // Create test context
    m17_key_context_t context;
    int result = m17_create_key_context(&context, "SP5WWP", "SP6ABC", 12345, M17_KEY_TYPE_ENCRYPTION);
    assert(result == 0);
    
    // Verify context
    assert(m17_verify_key_context(&context) == true);
    
    // Test key derivation
    uint8_t shared_secret[32];
    for (int i = 0; i < 32; i++) {
        shared_secret[i] = (uint8_t)(i * 3 + 7);
    }
    
    uint8_t derived_key[32];
    result = m17_derive_session_key(shared_secret, 32, &context, derived_key, 32);
    assert(result == 0);
    
    // Test multiple key derivation
    uint8_t enc_key[32], auth_key[32], int_key[32];
    result = m17_derive_session_keys(shared_secret, 32, &context, 
                                   enc_key, 32, auth_key, 32, int_key, 32);
    assert(result == 0);
    
    // Verify keys are different
    assert(m17_secure_key_compare(enc_key, auth_key, 32) == false);
    assert(m17_secure_key_compare(enc_key, int_key, 32) == false);
    assert(m17_secure_key_compare(auth_key, int_key, 32) == false);
    
    printf("[PASS] Key derivation with context works correctly\n");
}

// Test replay attack protection
void test_replay_attack_protection() {
    printf("Testing replay attack protection...\n");
    
    // Initialize replay protection
    assert(m17_replay_protection_init() == 0);
    
    // Test valid frame numbers
    assert(m17_check_frame_replay(1000) == true);
    assert(m17_add_frame_to_window(1000) == 0);
    
    assert(m17_check_frame_replay(1001) == true);
    assert(m17_add_frame_to_window(1001) == 0);
    
    // Test replay detection
    assert(m17_check_frame_replay(1000) == false); // Should be rejected as replay
    
    // Test frame sequence validation
    assert(m17_validate_frame_sequence(1002, 1002) == true);
    assert(m17_validate_frame_sequence(1003, 1002) == true); // Within tolerance
    assert(m17_validate_frame_sequence(1020, 1002) == false); // Too far
    
    printf("[PASS] Replay attack protection works correctly\n");
}

// Test buffer overflow protection
void test_buffer_overflow_protection() {
    printf("Testing buffer overflow protection...\n");
    
    // Test safe memory copy
    uint8_t src[16], dest[16];
    for (int i = 0; i < 16; i++) {
        src[i] = (uint8_t)(i * 2);
    }
    
    // Valid copy
    assert(m17_safe_memcpy(dest, 16, src, 16) == 0);
    assert(memcmp(dest, src, 16) == 0);
    
    // Overflow attempt
    assert(m17_safe_memcpy(dest, 8, src, 16) == -1);
    
    // Test safe string operations
    char test_str[32];
    assert(m17_safe_strcpy(test_str, 32, "Hello") == 0);
    assert(strcmp(test_str, "Hello") == 0);
    
    // Overflow attempt
    assert(m17_safe_strcpy(test_str, 4, "Hello World") == -1);
    
    // Test UTF-8 parsing
    uint8_t output[32];
    size_t bytes_written;
    assert(m17_safe_utf8_parse("Hello", 5, output, 32, &bytes_written) == 0);
    assert(bytes_written == 5);
    
    // Overflow attempt
    assert(m17_safe_utf8_parse("Hello", 5, output, 3, &bytes_written) == -1);
    
    // Test bounds validation
    assert(m17_validate_buffer_bounds(output, 32, 0, 16) == true);
    assert(m17_validate_buffer_bounds(output, 32, 0, 40) == false);
    assert(m17_validate_buffer_bounds(output, 32, 40, 1) == false);
    
    printf("[PASS] Buffer overflow protection works correctly\n");
}

// Test rate limiting and security monitoring
void test_rate_limiting_and_monitoring() {
    printf("Testing rate limiting and security monitoring...\n");
    
    // Initialize security monitoring
    assert(m17_security_monitoring_init() == 0);
    
    // Test rate limiting
    assert(m17_check_rate_limit("SP5WWP") == true);
    assert(m17_check_rate_limit("SP5WWP") == true);
    
    // Simulate multiple failures
    for (int i = 0; i < 6; i++) {
        m17_record_security_event(M17_EVENT_AUTH_FAILURE, "SP5WWP", "Test failure");
    }
    
    // Should now be rate limited
    assert(m17_check_rate_limit("SP5WWP") == false);
    assert(m17_is_identifier_blocked("SP5WWP") == true);
    
    // Test different identifier
    assert(m17_check_rate_limit("SP6ABC") == true);
    
    // Test security statistics
    uint32_t total_events, blocked_attempts, active_entries;
    m17_get_security_stats(&total_events, &blocked_attempts, &active_entries);
    assert(total_events > 0);
    assert(blocked_attempts > 0);
    assert(active_entries > 0);
    
    // Test suspicious activity detection
    bool suspicious = m17_detect_suspicious_activity();
    // May or may not be true depending on timing
    
    printf("[PASS] Rate limiting and security monitoring works correctly\n");
}

// Test constant-time operations
void test_constant_time_operations() {
    printf("Testing constant-time operations...\n");
    
    uint8_t key1[32], key2[32], key3[32];
    
    // Fill with test data
    for (int i = 0; i < 32; i++) {
        key1[i] = (uint8_t)(i * 3);
        key2[i] = (uint8_t)(i * 3);
        key3[i] = (uint8_t)(i * 3 + 1);
    }
    
    // Test constant-time comparison
    assert(m17_secure_key_compare(key1, key2, 32) == true);
    assert(m17_secure_key_compare(key1, key3, 32) == false);
    
    // Test secure key wiping
    m17_secure_key_wipe(key1, 32);
    
    // Verify key is wiped (should be all zeros)
    bool is_wiped = true;
    for (int i = 0; i < 32; i++) {
        if (key1[i] != 0x00) {
            is_wiped = false;
            break;
        }
    }
    assert(is_wiped);
    
    printf("[PASS] Constant-time operations work correctly\n");
}

// Test comprehensive error handling
void test_comprehensive_error_handling() {
    printf("Testing comprehensive error handling...\n");
    
    // Test NULL pointer handling
    assert(m17_create_key_context(NULL, "SP5WWP", "SP6ABC", 12345, 1) == -1);
    assert(m17_create_key_context(&(m17_key_context_t){0}, NULL, "SP6ABC", 12345, 1) == -1);
    assert(m17_create_key_context(&(m17_key_context_t){0}, "SP5WWP", NULL, 12345, 1) == -1);
    
    // Test invalid parameters
    assert(m17_derive_session_key(NULL, 32, &(m17_key_context_t){0}, NULL, 32) == -1);
    assert(m17_derive_session_key((uint8_t[32]){0}, 0, &(m17_key_context_t){0}, (uint8_t[32]){0}, 32) == -1);
    assert(m17_derive_session_key((uint8_t[32]){0}, 32, &(m17_key_context_t){0}, (uint8_t[32]){0}, 0) == -1);
    
    // Test bounds checking
    assert(m17_safe_memcpy(NULL, 16, (uint8_t[16]){0}, 16) == -1);
    assert(m17_safe_memcpy((uint8_t[16]){0}, 0, (uint8_t[16]){0}, 16) == -1);
    assert(m17_safe_memcpy((uint8_t[16]){0}, 16, (uint8_t[16]){0}, 0) == -1);
    
    // Test rate limiting with invalid input
    assert(m17_check_rate_limit(NULL) == false);
    
    printf("[PASS] Comprehensive error handling works correctly\n");
}

// Test secure memory operations
void test_secure_memory_operations() {
    printf("Testing secure memory operations...\n");
    
    // Test secure memory allocation
    void *ptr = m17_safe_malloc(64);
    assert(ptr != NULL);
    
    // Test secure memory reallocation
    void *new_ptr = m17_safe_realloc(ptr, 128);
    assert(new_ptr != NULL);
    
    // Test secure memory wiping
    m17_secure_wipe(new_ptr, 128);
    
    // Test secure memory free
    m17_secure_free(new_ptr, 128);
    
    // Test integer overflow protection
    size_t result;
    assert(m17_safe_add(1000, 2000, &result) == true);
    assert(result == 3000);
    
    assert(m17_safe_add(SIZE_MAX, 1, &result) == false); // Overflow
    
    assert(m17_safe_multiply(100, 200, &result) == true);
    assert(result == 20000);
    
    assert(m17_safe_multiply(SIZE_MAX, 2, &result) == false); // Overflow
    
    printf("[PASS] Secure memory operations work correctly\n");
}

// Main test function
int main() {
    printf("=== M17 Critical Security Test Suite ===\n");
    printf("Testing all critical security fixes and protections...\n\n");
    
    test_key_derivation_with_context();
    test_replay_attack_protection();
    test_buffer_overflow_protection();
    test_rate_limiting_and_monitoring();
    test_constant_time_operations();
    test_comprehensive_error_handling();
    test_secure_memory_operations();
    
    printf("\n=== All Critical Security Tests Passed ===\n");
    printf("[SUCCESS] All critical security fixes are working correctly\n");
    
    return 0;
}








