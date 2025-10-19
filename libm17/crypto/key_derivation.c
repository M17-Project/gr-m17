//--------------------------------------------------------------------
// M17 C library - crypto/key_derivation.c
//
// Secure key derivation using HKDF with context information
// Implements proper session key derivation from ECDH shared secrets
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------

#include "m17.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

// Context structure for key derivation (defined in header)

// Key types for derivation
#define M17_KEY_TYPE_ENCRYPTION    1
#define M17_KEY_TYPE_AUTHENTICATION 2
#define M17_KEY_TYPE_INTEGRITY     3
#define M17_KEY_TYPE_SESSION      4

// Generate unique session ID
static int generate_session_id(uint8_t session_id[16]) {
    FILE *urandom = fopen("/dev/urandom", "rb");
    if (urandom == NULL) {
        return -1;
    }
    
    size_t bytes_read = fread(session_id, 1, 16, urandom);
    fclose(urandom);
    
    if (bytes_read != 16) {
        return -1;
    }
    
    return 0;
}

// Create key derivation context
int m17_create_key_context(m17_key_context_t *ctx, 
                          const char *sender_callsign,
                          const char *receiver_callsign,
                          uint16_t frame_number,
                          uint8_t key_type) {
    if (ctx == NULL || sender_callsign == NULL || receiver_callsign == NULL) {
        return -1;
    }
    
    // Generate unique session ID
    if (generate_session_id(ctx->session_id) != 0) {
        return -1;
    }
    
    // Set timestamp
    // SECURITY FIX: Use secure timestamp generation
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        ctx->timestamp = (uint64_t)ts.tv_sec;
    } else {
        ctx->timestamp = 0; // Fallback
    }
    
    // Copy callsigns (with bounds checking)
    strncpy((char*)ctx->sender_callsign, sender_callsign, 8);
    ctx->sender_callsign[8] = '\0';
    
    strncpy((char*)ctx->receiver_callsign, receiver_callsign, 8);
    ctx->receiver_callsign[8] = '\0';
    
    ctx->frame_number = frame_number;
    ctx->key_type = key_type;
    
    return 0;
}

// Derive session key from ECDH shared secret
int m17_derive_session_key(const uint8_t *shared_secret, size_t shared_secret_len,
                          const m17_key_context_t *context,
                          uint8_t *derived_key, size_t key_len) {
    if (shared_secret == NULL || context == NULL || derived_key == NULL) {
        return -1;
    }
    
    if (shared_secret_len == 0 || key_len == 0) {
        return -1;
    }
    
    // Create context string for HKDF
    uint8_t context_data[64];
    size_t context_len = 0;
    
    // Add session ID
    memcpy(context_data + context_len, context->session_id, 16);
    context_len += 16;
    
    // Add timestamp
    memcpy(context_data + context_len, &context->timestamp, 8);
    context_len += 8;
    
    // Add sender callsign
    size_t sender_len = strlen((char*)context->sender_callsign);
    memcpy(context_data + context_len, context->sender_callsign, sender_len);
    context_len += sender_len;
    
    // Add receiver callsign
    size_t receiver_len = strlen((char*)context->receiver_callsign);
    memcpy(context_data + context_len, context->receiver_callsign, receiver_len);
    context_len += receiver_len;
    
    // Add frame number
    memcpy(context_data + context_len, &context->frame_number, 2);
    context_len += 2;
    
    // Add key type
    context_data[context_len] = context->key_type;
    context_len++;
    
    // Use HKDF to derive key
    int result = m17_hkdf_derive(shared_secret, shared_secret_len,
                               context_data, context_len,
                               (uint8_t*)"M17-Session-Key", 15,
                               derived_key, key_len);
    
    return result;
}

// Derive multiple keys for a session
int m17_derive_session_keys(const uint8_t *shared_secret, size_t shared_secret_len,
                           const m17_key_context_t *context,
                           uint8_t *encryption_key, size_t enc_key_len,
                           uint8_t *authentication_key, size_t auth_key_len,
                           uint8_t *integrity_key, size_t int_key_len) {
    if (shared_secret == NULL || context == NULL) {
        return -1;
    }
    
    int result = 0;
    
    // Derive encryption key
    if (encryption_key != NULL && enc_key_len > 0) {
        m17_key_context_t enc_ctx = *context;
        enc_ctx.key_type = M17_KEY_TYPE_ENCRYPTION;
        result |= m17_derive_session_key(shared_secret, shared_secret_len,
                                       &enc_ctx, encryption_key, enc_key_len);
    }
    
    // Derive authentication key
    if (authentication_key != NULL && auth_key_len > 0) {
        m17_key_context_t auth_ctx = *context;
        auth_ctx.key_type = M17_KEY_TYPE_AUTHENTICATION;
        result |= m17_derive_session_key(shared_secret, shared_secret_len,
                                       &auth_ctx, authentication_key, auth_key_len);
    }
    
    // Derive integrity key
    if (integrity_key != NULL && int_key_len > 0) {
        m17_key_context_t int_ctx = *context;
        int_ctx.key_type = M17_KEY_TYPE_INTEGRITY;
        result |= m17_derive_session_key(shared_secret, shared_secret_len,
                                       &int_ctx, integrity_key, int_key_len);
    }
    
    return result;
}

// Verify key derivation context
bool m17_verify_key_context(const m17_key_context_t *context) {
    if (context == NULL) {
        return false;
    }
    
    // Check timestamp is reasonable (not too old, not in future)
    // SECURITY FIX: Use secure timestamp generation
    struct timespec ts;
    uint64_t current_time;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        current_time = (uint64_t)ts.tv_sec;
    } else {
        current_time = 0; // Fallback
    }
    if (context->timestamp > current_time || 
        (current_time - context->timestamp) > 3600) { // 1 hour max
        return false;
    }
    
    // Check frame number is valid
    if (context->frame_number > 0xFFFF) {
        return false;
    }
    
    // Check key type is valid
    if (context->key_type < 1 || context->key_type > 4) {
        return false;
    }
    
    // Check callsigns are not empty
    if (strlen((char*)context->sender_callsign) == 0 ||
        strlen((char*)context->receiver_callsign) == 0) {
        return false;
    }
    
    return true;
}

// Secure key comparison (constant-time)
bool m17_secure_key_compare(const uint8_t *key1, const uint8_t *key2, size_t len) {
    if (key1 == NULL || key2 == NULL || len == 0) {
        return false;
    }
    
    uint8_t result = 0;
    for (size_t i = 0; i < len; i++) {
        result |= (key1[i] ^ key2[i]);
    }
    
    return (result == 0);
}

// Secure key wiping
void m17_secure_key_wipe(uint8_t *key, size_t len) {
    if (key == NULL || len == 0) {
        return;
    }
    
    // Multiple passes with different patterns
    volatile uint8_t *volatile_key = (volatile uint8_t *)key;
    
    for (size_t i = 0; i < len; i++) volatile_key[i] = 0x00;
    for (size_t i = 0; i < len; i++) volatile_key[i] = 0xFF;
    for (size_t i = 0; i < len; i++) volatile_key[i] = 0xAA;
    for (size_t i = 0; i < len; i++) volatile_key[i] = 0x55;
    for (size_t i = 0; i < len; i++) volatile_key[i] = 0x00;
    
    // Memory barrier
    __asm__ __volatile__("" ::: "memory");
}
