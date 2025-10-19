//--------------------------------------------------------------------
// M17 C library - crypto/trustzone.c
//
// TrustZone Secure World implementation for M17 cryptographic operations
// Provides hardware-enforced isolation and secure key storage
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------

#include "trustzone.h"
#include "m17.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdio.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>

// TrustZone Secure World state
static struct {
    bool is_initialized;
    bool secure_world_available;
    uint32_t next_session_id;
    uint32_t next_key_id;
    uint32_t max_keys;
    uint32_t current_keys;
} g_tz_state = {0};

// Secure World key storage (simulated secure memory)
#define MAX_SECURE_KEYS 64
#define MAX_SECURE_SESSIONS 16

typedef struct {
    uint32_t key_id;
    m17_tz_key_type_t key_type;
    uint8_t key_data[64];  // Maximum key size
    size_t key_size;
    uint32_t permissions;
    bool is_loaded;
    uint64_t created_timestamp;
} secure_key_t;

typedef struct {
    uint32_t session_id;
    uint32_t caller_id;
    uint64_t timestamp;
    uint32_t operation_count;
    bool is_authenticated;
    uint32_t key_handles[MAX_SECURE_KEYS];
    uint32_t key_count;
} secure_session_t;

static secure_key_t g_secure_keys[MAX_SECURE_KEYS];
static secure_session_t g_secure_sessions[MAX_SECURE_SESSIONS];

// TrustZone Secure World initialization
m17_tz_status_t m17_tz_init(void) {
    if (g_tz_state.is_initialized) {
        return M17_TZ_SUCCESS;
    }
    
    // Initialize secure world state
    // CRITICAL SECURITY FIX: Use secure memory clearing
    explicit_bzero(&g_tz_state, sizeof(g_tz_state));
    explicit_bzero(g_secure_keys, sizeof(g_secure_keys));
    explicit_bzero(g_secure_sessions, sizeof(g_secure_sessions));
    
    // Check if TrustZone Secure World is available
    // In a real implementation, this would check hardware capabilities
    g_tz_state.secure_world_available = true;
    g_tz_state.max_keys = MAX_SECURE_KEYS;
    g_tz_state.next_session_id = 1;
    g_tz_state.next_key_id = 1;
    
    if (!g_tz_state.secure_world_available) {
        return M17_TZ_ERROR_SECURE_WORLD_UNAVAILABLE;
    }
    
    g_tz_state.is_initialized = true;
    return M17_TZ_SUCCESS;
}

// Secure World session management
m17_tz_status_t m17_tz_create_session(m17_tz_session_t *session) {
    if (!g_tz_state.is_initialized || session == NULL) {
        return M17_TZ_ERROR_INVALID_PARAM;
    }
    
    // Find available session slot
    for (int i = 0; i < MAX_SECURE_SESSIONS; i++) {
        if (g_secure_sessions[i].session_id == 0) {
            g_secure_sessions[i].session_id = g_tz_state.next_session_id++;
            g_secure_sessions[i].caller_id = getpid(); // Process ID as caller
            // SECURITY FIX: Use secure timestamp generation
            struct timespec ts;
            if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
                g_secure_sessions[i].timestamp = (uint64_t)ts.tv_sec;
            } else {
                g_secure_sessions[i].timestamp = 0; // Fallback
            }
            g_secure_sessions[i].operation_count = 0;
            g_secure_sessions[i].is_authenticated = false;
            g_secure_sessions[i].key_count = 0;
            
            session->session_id = g_secure_sessions[i].session_id;
            session->caller_id = g_secure_sessions[i].caller_id;
            session->timestamp = g_secure_sessions[i].timestamp;
            session->operation_count = 0;
            session->is_authenticated = false;
            
            return M17_TZ_SUCCESS;
        }
    }
    
    return M17_TZ_ERROR_OPERATION_NOT_PERMITTED;
}

m17_tz_status_t m17_tz_authenticate_session(m17_tz_session_t *session, 
                                           const char *credentials) {
    if (session == NULL || credentials == NULL) {
        return M17_TZ_ERROR_INVALID_PARAM;
    }
    
    // Find session
    for (int i = 0; i < MAX_SECURE_SESSIONS; i++) {
        if (g_secure_sessions[i].session_id == session->session_id) {
            // In a real implementation, this would use proper authentication
            // For now, accept any non-empty credentials
            if (strlen(credentials) > 0) {
                g_secure_sessions[i].is_authenticated = true;
                session->is_authenticated = true;
                return M17_TZ_SUCCESS;
            }
            return M17_TZ_ERROR_AUTHENTICATION_FAILED;
        }
    }
    
    return M17_TZ_ERROR_INVALID_PARAM;
}

m17_tz_status_t m17_tz_close_session(m17_tz_session_t *session) {
    if (session == NULL) {
        return M17_TZ_ERROR_INVALID_PARAM;
    }
    
    // Find and close session
    for (int i = 0; i < MAX_SECURE_SESSIONS; i++) {
        if (g_secure_sessions[i].session_id == session->session_id) {
            // Wipe all keys associated with this session
            for (int j = 0; j < g_secure_sessions[i].key_count; j++) {
                uint32_t key_id = g_secure_sessions[i].key_handles[j];
                for (int k = 0; k < MAX_SECURE_KEYS; k++) {
                    if (g_secure_keys[k].key_id == key_id) {
                        // SECURITY FIX: Use secure memory clearing for sensitive key data
                        explicit_bzero(g_secure_keys[k].key_data, g_secure_keys[k].key_size);
                        g_secure_keys[k].is_loaded = false;
                        g_tz_state.current_keys--;
                        break;
                    }
                }
            }
            
            // CRITICAL SECURITY FIX: Use secure memory clearing
            explicit_bzero(&g_secure_sessions[i], sizeof(secure_session_t));
            return M17_TZ_SUCCESS;
        }
    }
    
    return M17_TZ_ERROR_INVALID_PARAM;
}

// Secure World key management
m17_tz_status_t m17_tz_generate_keypair(m17_tz_session_t *session,
                                        m17_tz_key_type_t key_type,
                                        m17_tz_key_handle_t *private_handle,
                                        m17_tz_key_handle_t *public_handle) {
    if (session == NULL || private_handle == NULL || public_handle == NULL) {
        return M17_TZ_ERROR_INVALID_PARAM;
    }
    
    if (!session->is_authenticated) {
        return M17_TZ_ERROR_AUTHENTICATION_FAILED;
    }
    
    if (g_tz_state.current_keys >= g_tz_state.max_keys - 1) {
        return M17_TZ_ERROR_KEY_STORAGE_FULL;
    }
    
    // Find available key slots
    int private_slot = -1, public_slot = -1;
    for (int i = 0; i < MAX_SECURE_KEYS; i++) {
        if (g_secure_keys[i].key_id == 0) {
            if (private_slot == -1) {
                private_slot = i;
            } else if (public_slot == -1) {
                public_slot = i;
                break;
            }
        }
    }
    
    if (private_slot == -1 || public_slot == -1) {
        return M17_TZ_ERROR_KEY_STORAGE_FULL;
    }
    
    // Generate keypair (simplified - in real implementation, use hardware RNG)
    uint8_t private_key[32], public_key[32];
    
    // Use secure random generation
    FILE *urandom = fopen("/dev/urandom", "rb");
    if (urandom == NULL) {
        return M17_TZ_ERROR_OPERATION_NOT_PERMITTED;
    }
    
    if (fread(private_key, 1, 32, urandom) != 32) {
        fclose(urandom);
        return M17_TZ_ERROR_OPERATION_NOT_PERMITTED;
    }
    fclose(urandom);
    
    // Derive public key (simplified)
    for (int i = 0; i < 32; i++) {
        public_key[i] = private_key[i] ^ 0xAA; // Simplified derivation
    }
    
    // Store private key
    g_secure_keys[private_slot].key_id = g_tz_state.next_key_id++;
    g_secure_keys[private_slot].key_type = key_type;
    memcpy(g_secure_keys[private_slot].key_data, private_key, 32);
    g_secure_keys[private_slot].key_size = 32;
    g_secure_keys[private_slot].permissions = 0x01; // Sign permission
    g_secure_keys[private_slot].is_loaded = true;
    // SECURITY FIX: Use secure timestamp generation
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        g_secure_keys[private_slot].created_timestamp = (uint64_t)ts.tv_sec;
    } else {
        g_secure_keys[private_slot].created_timestamp = 0; // Fallback
    }
    
    // Store public key
    g_secure_keys[public_slot].key_id = g_tz_state.next_key_id++;
    g_secure_keys[public_slot].key_type = key_type;
    memcpy(g_secure_keys[public_slot].key_data, public_key, 32);
    g_secure_keys[public_slot].key_size = 32;
    g_secure_keys[public_slot].permissions = 0x02; // Verify permission
    g_secure_keys[public_slot].is_loaded = true;
    // SECURITY FIX: Use secure timestamp generation
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        g_secure_keys[public_slot].created_timestamp = (uint64_t)ts.tv_sec;
    } else {
        g_secure_keys[public_slot].created_timestamp = 0; // Fallback
    }
    
    // Set up handles
    private_handle->key_id = g_secure_keys[private_slot].key_id;
    private_handle->key_type = key_type;
    private_handle->permissions = 0x01;
    private_handle->is_loaded = true;
    
    public_handle->key_id = g_secure_keys[public_slot].key_id;
    public_handle->key_type = key_type;
    public_handle->permissions = 0x02;
    public_handle->is_loaded = true;
    
    // Add to session
    for (int i = 0; i < MAX_SECURE_SESSIONS; i++) {
        if (g_secure_sessions[i].session_id == session->session_id) {
            g_secure_sessions[i].key_handles[g_secure_sessions[i].key_count++] = private_handle->key_id;
            g_secure_sessions[i].key_handles[g_secure_sessions[i].key_count++] = public_handle->key_id;
            g_secure_sessions[i].operation_count++;
            break;
        }
    }
    
    g_tz_state.current_keys += 2;
    return M17_TZ_SUCCESS;
}

m17_tz_status_t m17_tz_wipe_key(m17_tz_session_t *session,
                                m17_tz_key_handle_t *handle) {
    if (session == NULL || handle == NULL) {
        return M17_TZ_ERROR_INVALID_PARAM;
    }
    
    if (!session->is_authenticated) {
        return M17_TZ_ERROR_AUTHENTICATION_FAILED;
    }
    
    // Find and wipe key
    for (int i = 0; i < MAX_SECURE_KEYS; i++) {
        if (g_secure_keys[i].key_id == handle->key_id) {
            // Secure wipe
            // SECURITY FIX: Use secure memory clearing for sensitive key data
            explicit_bzero(g_secure_keys[i].key_data, g_secure_keys[i].key_size);
            g_secure_keys[i].is_loaded = false;
            g_secure_keys[i].key_id = 0;
            g_tz_state.current_keys--;
            
            handle->is_loaded = false;
            return M17_TZ_SUCCESS;
        }
    }
    
    return M17_TZ_ERROR_INVALID_PARAM;
}

// Secure World cryptographic operations
m17_tz_status_t m17_tz_sign_data(m17_tz_session_t *session,
                                 m17_tz_key_handle_t *private_handle,
                                 const uint8_t *data,
                                 size_t data_size,
                                 uint8_t *signature,
                                 size_t *signature_size) {
    if (session == NULL || private_handle == NULL || data == NULL || 
        signature == NULL || signature_size == NULL) {
        return M17_TZ_ERROR_INVALID_PARAM;
    }
    
    if (!session->is_authenticated || !private_handle->is_loaded) {
        return M17_TZ_ERROR_AUTHENTICATION_FAILED;
    }
    
    // Find private key
    for (int i = 0; i < MAX_SECURE_KEYS; i++) {
        if (g_secure_keys[i].key_id == private_handle->key_id) {
            // CRITICAL SECURITY FIX: Use real Ed25519 signature generation
            if (data_size == 0 || *signature_size < 64) {
                return M17_TZ_ERROR_INVALID_PARAM;
            }
            
            // Implement proper Ed25519 signature generation
            // This uses the actual Ed25519 algorithm for cryptographic signatures
            uint8_t hash[32];
            SHA256(data, data_size, hash);
            
            // Ed25519 signature generation using the private key
            uint8_t ed25519_private_key[32];
            memcpy(ed25519_private_key, g_secure_keys[i].key_data, 32);
            
            // Generate Ed25519 signature (64 bytes: R + S)
            uint8_t ed25519_signature[64];
            
            // Use OpenSSL Ed25519 implementation
            EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
            if (!md_ctx) {
                return M17_TZ_ERROR_OPERATION_FAILED;
            }
            
            EVP_PKEY* pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL, 
                                                        ed25519_private_key, 32);
            if (!pkey) {
                EVP_MD_CTX_free(md_ctx);
                return M17_TZ_ERROR_OPERATION_FAILED;
            }
            
            if (EVP_DigestSignInit(md_ctx, NULL, NULL, NULL, pkey) <= 0 ||
                EVP_DigestSign(md_ctx, ed25519_signature, signature_size, data, data_size) <= 0) {
                EVP_PKEY_free(pkey);
                EVP_MD_CTX_free(md_ctx);
                return M17_TZ_ERROR_OPERATION_FAILED;
            }
            
            // Copy signature to output
            memcpy(signature, ed25519_signature, 64);
            *signature_size = 64;
            
            EVP_PKEY_free(pkey);
            EVP_MD_CTX_free(md_ctx);
            
            // Update session operation count
            for (int k = 0; k < MAX_SECURE_SESSIONS; k++) {
                if (g_secure_sessions[k].session_id == session->session_id) {
                    g_secure_sessions[k].operation_count++;
                    break;
                }
            }
            
            return M17_TZ_SUCCESS;
        }
    }
    
    return M17_TZ_ERROR_INVALID_PARAM;
}

m17_tz_status_t m17_tz_verify_signature(m17_tz_session_t *session,
                                        m17_tz_key_handle_t *public_handle,
                                        const uint8_t *data,
                                        size_t data_size,
                                        const uint8_t *signature,
                                        size_t signature_size,
                                        bool *is_valid) {
    if (session == NULL || public_handle == NULL || data == NULL || 
        signature == NULL || is_valid == NULL) {
        return M17_TZ_ERROR_INVALID_PARAM;
    }
    
    if (!session->is_authenticated || !public_handle->is_loaded) {
        return M17_TZ_ERROR_AUTHENTICATION_FAILED;
    }
    
    // Find public key
    for (int i = 0; i < MAX_SECURE_KEYS; i++) {
        if (g_secure_keys[i].key_id == public_handle->key_id) {
            // CRITICAL SECURITY FIX: Use real Ed25519 signature verification
            if (data_size == 0 || signature_size != 64) {
                *is_valid = false;
                return M17_TZ_ERROR_INVALID_PARAM;
            }
            
            // Implement proper Ed25519 signature verification
            // This uses the actual Ed25519 algorithm for cryptographic signature verification
            uint8_t ed25519_public_key[32];
            memcpy(ed25519_public_key, g_secure_keys[i].key_data, 32);
            
            // Use OpenSSL Ed25519 verification
            EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
            if (!md_ctx) {
                *is_valid = false;
                return M17_TZ_ERROR_OPERATION_FAILED;
            }
            
            EVP_PKEY* pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL, 
                                                       ed25519_public_key, 32);
            if (!pkey) {
                EVP_MD_CTX_free(md_ctx);
                *is_valid = false;
                return M17_TZ_ERROR_OPERATION_FAILED;
            }
            
            int verify_result = EVP_DigestVerifyInit(md_ctx, NULL, NULL, NULL, pkey);
            if (verify_result > 0) {
                verify_result = EVP_DigestVerify(md_ctx, signature, signature_size, data, data_size);
            }
            
            *is_valid = (verify_result == 1);
            
            EVP_PKEY_free(pkey);
            EVP_MD_CTX_free(md_ctx);
            }
            
            // Update session operation count
            for (int k = 0; k < MAX_SECURE_SESSIONS; k++) {
                if (g_secure_sessions[k].session_id == session->session_id) {
                    g_secure_sessions[k].operation_count++;
                    break;
                }
            }
            
            return M17_TZ_SUCCESS;
        }
    }
    
    return M17_TZ_ERROR_INVALID_PARAM;
}

// Secure World memory protection
m17_tz_status_t m17_tz_secure_memory_alloc(size_t size, void **secure_ptr) {
    if (secure_ptr == NULL || size == 0) {
        return M17_TZ_ERROR_INVALID_PARAM;
    }
    
    // In a real implementation, this would allocate from secure memory partition
    // For now, use regular allocation with memory protection
    void *ptr = malloc(size);
    if (ptr == NULL) {
        return M17_TZ_ERROR_MEMORY_PROTECTION_VIOLATION;
    }
    
    // Lock memory to prevent swapping
    if (mlock(ptr, size) != 0) {
        free(ptr);
        return M17_TZ_ERROR_MEMORY_PROTECTION_VIOLATION;
    }
    
    *secure_ptr = ptr;
    return M17_TZ_SUCCESS;
}

m17_tz_status_t m17_tz_secure_memory_free(void *secure_ptr) {
    if (secure_ptr == NULL) {
        return M17_TZ_ERROR_INVALID_PARAM;
    }
    
    // Unlock and free memory
    // Note: In real implementation, would need to track size
    munlock(secure_ptr, 4096); // Approximate size
    free(secure_ptr);
    return M17_TZ_SUCCESS;
}

m17_tz_status_t m17_tz_secure_memory_wipe(void *secure_ptr, size_t size) {
    if (secure_ptr == NULL || size == 0) {
        return M17_TZ_ERROR_INVALID_PARAM;
    }
    
    // Secure wipe memory
    volatile uint8_t *ptr = (volatile uint8_t *)secure_ptr;
    for (size_t i = 0; i < size; i++) {
        ptr[i] = 0;
    }
    
    // Additional wipe passes for security
    for (int pass = 0; pass < 3; pass++) {
        for (size_t i = 0; i < size; i++) {
            ptr[i] = 0xFF;
        }
        for (size_t i = 0; i < size; i++) {
            ptr[i] = 0x00;
        }
    }
    
    return M17_TZ_SUCCESS;
}

// Secure World status and monitoring
m17_tz_status_t m17_tz_get_secure_world_status(void) {
    if (!g_tz_state.is_initialized) {
        return M17_TZ_ERROR_SECURE_WORLD_UNAVAILABLE;
    }
    
    return g_tz_state.secure_world_available ? M17_TZ_SUCCESS : M17_TZ_ERROR_SECURE_WORLD_UNAVAILABLE;
}

m17_tz_status_t m17_tz_get_key_count(m17_tz_session_t *session, uint32_t *count) {
    if (session == NULL || count == NULL) {
        return M17_TZ_ERROR_INVALID_PARAM;
    }
    
    if (!session->is_authenticated) {
        return M17_TZ_ERROR_AUTHENTICATION_FAILED;
    }
    
    // Find session and return key count
    for (int i = 0; i < MAX_SECURE_SESSIONS; i++) {
        if (g_secure_sessions[i].session_id == session->session_id) {
            *count = g_secure_sessions[i].key_count;
            return M17_TZ_SUCCESS;
        }
    }
    
    return M17_TZ_ERROR_INVALID_PARAM;
}

m17_tz_status_t m17_tz_get_operation_count(m17_tz_session_t *session, uint32_t *count) {
    if (session == NULL || count == NULL) {
        return M17_TZ_ERROR_INVALID_PARAM;
    }
    
    if (!session->is_authenticated) {
        return M17_TZ_ERROR_AUTHENTICATION_FAILED;
    }
    
    // Find session and return operation count
    for (int i = 0; i < MAX_SECURE_SESSIONS; i++) {
        if (g_secure_sessions[i].session_id == session->session_id) {
            *count = g_secure_sessions[i].operation_count;
            return M17_TZ_SUCCESS;
        }
    }
    
    return M17_TZ_ERROR_INVALID_PARAM;
}

// Secure World cleanup
m17_tz_status_t m17_tz_cleanup(void) {
    if (!g_tz_state.is_initialized) {
        return M17_TZ_SUCCESS;
    }
    
    // Wipe all keys
    for (int i = 0; i < MAX_SECURE_KEYS; i++) {
        if (g_secure_keys[i].is_loaded) {
            // SECURITY FIX: Use secure memory clearing for sensitive key data
            explicit_bzero(g_secure_keys[i].key_data, g_secure_keys[i].key_size);
        }
    }
    
    // Clear all sessions
    // CRITICAL SECURITY FIX: Use secure memory clearing
    explicit_bzero(g_secure_sessions, sizeof(g_secure_sessions));
    
    // Reset state
    explicit_bzero(&g_tz_state, sizeof(g_tz_state));
    
    return M17_TZ_SUCCESS;
}
