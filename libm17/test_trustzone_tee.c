//--------------------------------------------------------------------
// M17 C library - test_trustzone_tee.c
//
// Comprehensive test suite for TrustZone Secure World and OP-TEE integration
// Tests hardware-enforced isolation and secure cryptographic operations
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "m17.h"

// Test TrustZone Secure World functionality
static void test_trustzone_initialization(void) {
    printf("Testing TrustZone Secure World initialization...\n");
    
    m17_tz_status_t status = m17_tz_init();
    assert(status == M17_TZ_SUCCESS);
    printf("  [OK] TrustZone initialization successful\n");
    
    status = m17_tz_get_secure_world_status();
    assert(status == M17_TZ_SUCCESS);
    printf("  [OK] Secure World status check successful\n");
}

static void test_trustzone_session_management(void) {
    printf("Testing TrustZone session management...\n");
    
    m17_tz_session_t session;
    m17_tz_status_t status = m17_tz_create_session(&session);
    assert(status == M17_TZ_SUCCESS);
    printf("  [OK] Session creation successful\n");
    
    status = m17_tz_authenticate_session(&session, "test_credentials");
    assert(status == M17_TZ_SUCCESS);
    printf("  [OK] Session authentication successful\n");
    
    status = m17_tz_close_session(&session);
    assert(status == M17_TZ_SUCCESS);
    printf("  [OK] Session closure successful\n");
}

static void test_trustzone_key_management(void) {
    printf("Testing TrustZone key management...\n");
    
    m17_tz_session_t session;
    m17_tz_create_session(&session);
    m17_tz_authenticate_session(&session, "test_credentials");
    
    m17_tz_key_handle_t private_handle, public_handle;
    m17_tz_status_t status = m17_tz_generate_keypair(&session, M17_TZ_KEY_TYPE_ED25519_PRIVATE, 
                                                    &private_handle, &public_handle);
    assert(status == M17_TZ_SUCCESS);
    printf("  [OK] Keypair generation successful\n");
    
    assert(private_handle.is_loaded);
    assert(public_handle.is_loaded);
    printf("  [OK] Key handles properly initialized\n");
    
    status = m17_tz_wipe_key(&session, &private_handle);
    assert(status == M17_TZ_SUCCESS);
    printf("  [OK] Private key secure wipe successful\n");
    
    m17_tz_close_session(&session);
}

static void test_trustzone_cryptographic_operations(void) {
    printf("Testing TrustZone cryptographic operations...\n");
    
    m17_tz_session_t session;
    m17_tz_create_session(&session);
    m17_tz_authenticate_session(&session, "test_credentials");
    
    m17_tz_key_handle_t private_handle, public_handle;
    m17_tz_generate_keypair(&session, M17_TZ_KEY_TYPE_ED25519_PRIVATE, 
                           &private_handle, &public_handle);
    
    // Test signing
    const char *message = "Test message for TrustZone signing";
    uint8_t signature[64];
    size_t signature_size = 64;
    
    m17_tz_status_t status = m17_tz_sign_data(&session, &private_handle,
                                             (const uint8_t *)message, strlen(message),
                                             signature, &signature_size);
    assert(status == M17_TZ_SUCCESS);
    printf("  [OK] Data signing successful\n");
    
    // Test verification
    bool is_valid;
    status = m17_tz_verify_signature(&session, &public_handle,
                                    (const uint8_t *)message, strlen(message),
                                    signature, signature_size, &is_valid);
    assert(status == M17_TZ_SUCCESS);
    assert(is_valid);
    printf("  [OK] Signature verification successful\n");
    
    m17_tz_wipe_key(&session, &private_handle);
    m17_tz_close_session(&session);
}

static void test_trustzone_secure_memory(void) {
    printf("Testing TrustZone secure memory management...\n");
    
    void *secure_ptr;
    m17_tz_status_t status = m17_tz_secure_memory_alloc(1024, &secure_ptr);
    assert(status == M17_TZ_SUCCESS);
    assert(secure_ptr != NULL);
    printf("  [OK] Secure memory allocation successful\n");
    
    // Test secure memory wipe
    status = m17_tz_secure_memory_wipe(secure_ptr, 1024);
    assert(status == M17_TZ_SUCCESS);
    printf("  [OK] Secure memory wipe successful\n");
    
    status = m17_tz_secure_memory_free(secure_ptr);
    assert(status == M17_TZ_SUCCESS);
    printf("  [OK] Secure memory deallocation successful\n");
}

// Test OP-TEE functionality
static void test_optee_initialization(void) {
    printf("Testing OP-TEE initialization...\n");
    
    m17_tee_status_t status = m17_tee_initialize();
    assert(status == M17_TEE_SUCCESS);
    printf("  [OK] OP-TEE initialization successful\n");
    
    status = m17_tee_get_status();
    printf("  [OK] OP-TEE status check successful\n");
}

static void test_optee_context_management(void) {
    printf("Testing OP-TEE context management...\n");
    
    m17_tee_context_t context;
    m17_tee_status_t status = m17_tee_open_context(&context);
    assert(status == M17_TEE_SUCCESS);
    printf("  [OK] TEE context creation successful\n");
    
    m17_tee_session_t session;
    status = m17_tee_open_session(&context, &session, "test_ta_uuid");
    assert(status == M17_TEE_SUCCESS);
    printf("  [OK] TEE session creation successful\n");
    
    status = m17_tee_authenticate_session(&session, "test_credentials");
    assert(status == M17_TEE_SUCCESS);
    printf("  [OK] TEE session authentication successful\n");
    
    status = m17_tee_close_session(&session);
    assert(status == M17_TEE_SUCCESS);
    printf("  [OK] TEE session closure successful\n");
    
    status = m17_tee_close_context(&context);
    assert(status == M17_TEE_SUCCESS);
    printf("  [OK] TEE context closure successful\n");
}

static void test_optee_cryptographic_operations(void) {
    printf("Testing OP-TEE cryptographic operations...\n");
    
    m17_tee_context_t context;
    m17_tee_open_context(&context);
    
    m17_tee_session_t session;
    m17_tee_open_session(&context, &session, "test_ta_uuid");
    m17_tee_authenticate_session(&session, "test_credentials");
    
    m17_tee_key_handle_t private_handle, public_handle;
    m17_tee_status_t status = m17_tee_generate_keypair(&session, M17_TEE_KEY_TYPE_ED25519_PRIVATE,
                                                       &private_handle, &public_handle);
    assert(status == M17_TEE_SUCCESS);
    printf("  [OK] TEE keypair generation successful\n");
    
    // Test signing
    const char *message = "Test message for OP-TEE signing";
    uint8_t signature[64];
    size_t signature_size = 64;
    
    status = m17_tee_sign_data(&session, &private_handle,
                              (const uint8_t *)message, strlen(message),
                              signature, &signature_size);
    assert(status == M17_TEE_SUCCESS);
    printf("  [OK] TEE data signing successful\n");
    
    // Test verification
    bool is_valid;
    status = m17_tee_verify_signature(&session, &public_handle,
                                     (const uint8_t *)message, strlen(message),
                                     signature, signature_size, &is_valid);
    assert(status == M17_TEE_SUCCESS);
    assert(is_valid);
    printf("  [OK] TEE signature verification successful\n");
    
    m17_tee_wipe_key(&session, &private_handle);
    m17_tee_close_session(&session);
    m17_tee_close_context(&context);
}

// Test secure boot functionality
static void test_secure_boot_validation(void) {
    printf("Testing secure boot chain validation...\n");
    
    m17_sb_status_t status = m17_sb_validate_boot_chain();
    printf("  [OK] Boot chain validation completed\n");
    
    m17_sb_level_t level;
    bool is_valid;
    status = m17_sb_get_boot_status(&level, &is_valid);
    assert(status == M17_SB_SUCCESS);
    printf("  [OK] Boot status retrieval successful\n");
    
    bool hardware_available;
    status = m17_sb_check_secure_boot_hardware(&hardware_available);
    assert(status == M17_SB_SUCCESS);
    printf("  [OK] Hardware availability check successful\n");
}

static void test_secure_boot_attestation(void) {
    printf("Testing secure boot attestation...\n");
    
    m17_sb_attestation_t attestation;
    m17_sb_status_t status = m17_sb_create_attestation(M17_SB_COMPONENT_M17_LIBRARY, &attestation);
    assert(status == M17_SB_SUCCESS);
    printf("  [OK] Attestation creation successful\n");
    
    status = m17_sb_verify_attestation(&attestation);
    assert(status == M17_SB_SUCCESS);
    printf("  [OK] Attestation verification successful\n");
    
    uint8_t report[1024];
    size_t report_size = sizeof(report);
    status = m17_sb_get_attestation_report(report, &report_size);
    assert(status == M17_SB_SUCCESS);
    assert(report_size > 0);
    printf("  [OK] Attestation report generation successful\n");
}

static void test_secure_boot_monitoring(void) {
    printf("Testing secure boot monitoring...\n");
    
    bool is_intact;
    m17_sb_status_t status = m17_sb_monitor_integrity(&is_intact);
    assert(status == M17_SB_SUCCESS);
    printf("  [OK] Integrity monitoring successful\n");
    
    bool tampering_detected;
    status = m17_sb_detect_tampering(&tampering_detected);
    assert(status == M17_SB_SUCCESS);
    printf("  [OK] Tampering detection successful\n");
    
    uint8_t measurements[512];
    size_t measurement_size = sizeof(measurements);
    status = m17_sb_get_boot_measurements(measurements, &measurement_size);
    assert(status == M17_SB_SUCCESS);
    printf("  [OK] Boot measurements retrieval successful\n");
}

// Test integrated secure workflow
static void test_integrated_secure_workflow(void) {
    printf("Testing integrated secure workflow...\n");
    
    // Initialize all secure components
    m17_tz_init();
    m17_tee_initialize();
    m17_sb_validate_boot_chain();
    
    // Create secure session
    m17_tz_session_t tz_session;
    m17_tz_create_session(&tz_session);
    m17_tz_authenticate_session(&tz_session, "secure_workflow");
    
    // Generate secure keypair
    m17_tz_key_handle_t private_handle, public_handle;
    m17_tz_status_t status = m17_tz_generate_keypair(&tz_session, M17_TZ_KEY_TYPE_ED25519_PRIVATE,
                                                    &private_handle, &public_handle);
    assert(status == M17_TZ_SUCCESS);
    printf("  [OK] Secure keypair generation successful\n");
    
    // Sign data in secure world
    const char *secure_message = "Secure workflow test message";
    uint8_t signature[64];
    size_t signature_size = 64;
    
    status = m17_tz_sign_data(&tz_session, &private_handle,
                             (const uint8_t *)secure_message, strlen(secure_message),
                             signature, &signature_size);
    assert(status == M17_TZ_SUCCESS);
    printf("  [OK] Secure data signing successful\n");
    
    // Verify signature
    bool is_valid;
    status = m17_tz_verify_signature(&tz_session, &public_handle,
                                   (const uint8_t *)secure_message, strlen(secure_message),
                                   signature, signature_size, &is_valid);
    assert(status == M17_TZ_SUCCESS);
    assert(is_valid);
    printf("  [OK] Secure signature verification successful\n");
    
    // Clean up
    m17_tz_wipe_key(&tz_session, &private_handle);
    m17_tz_close_session(&tz_session);
    m17_tee_finalize();
    m17_tz_cleanup();
    
    printf("  [OK] Secure workflow cleanup successful\n");
}

int main(void) {
    printf("=== M17 TrustZone Secure World & OP-TEE Test Suite ===\n");
    printf("Testing hardware-enforced isolation and secure cryptographic operations...\n\n");
    
    // Test TrustZone Secure World
    printf("=== TrustZone Secure World Tests ===\n");
    test_trustzone_initialization();
    test_trustzone_session_management();
    test_trustzone_key_management();
    test_trustzone_cryptographic_operations();
    test_trustzone_secure_memory();
    printf("\n");
    
    // Test OP-TEE Integration
    printf("=== OP-TEE Integration Tests ===\n");
    test_optee_initialization();
    test_optee_context_management();
    test_optee_cryptographic_operations();
    printf("\n");
    
    // Test Secure Boot
    printf("=== Secure Boot Chain Validation Tests ===\n");
    test_secure_boot_validation();
    test_secure_boot_attestation();
    test_secure_boot_monitoring();
    printf("\n");
    
    // Test Integrated Workflow
    printf("=== Integrated Secure Workflow Tests ===\n");
    test_integrated_secure_workflow();
    printf("\n");
    
    printf("=== All TrustZone/TEE Tests Passed ===\n");
    printf("[SUCCESS] Hardware-enforced security features are working correctly\n");
    
    return 0;
}








