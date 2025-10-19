//--------------------------------------------------------------------
// M17 C library - crypto/security_monitoring.c
//
// Security monitoring and rate limiting
// Implements protection against brute force and monitoring
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------

#include "m17.h"
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

// Rate limiting configuration
#define M17_MAX_AUTH_ATTEMPTS 5
#define M17_RATE_LIMIT_WINDOW_SECONDS 300  // 5 minutes
#define M17_BACKOFF_BASE_SECONDS 60        // 1 minute base
#define M17_MAX_BACKOFF_SECONDS 3600       // 1 hour max

// Security event types (defined in header)

// Rate limiting entry
typedef struct {
    char identifier[32];           // IP, callsign, etc.
    uint32_t attempt_count;
    uint64_t first_attempt;
    uint64_t last_attempt;
    uint64_t backoff_until;
    bool blocked;
} m17_rate_limit_entry_t;

// Security monitoring state
typedef struct {
    m17_rate_limit_entry_t entries[100];  // Max 100 tracked entities
    uint32_t entry_count;
    uint64_t last_cleanup;
    uint32_t total_events;
    uint32_t blocked_attempts;
} m17_security_state_t;

// Global security state
static m17_security_state_t g_security_state = {0};

// Initialize security monitoring
int m17_security_monitoring_init(void) {
    // CRITICAL SECURITY FIX: Use secure memory clearing
    explicit_bzero(&g_security_state, sizeof(m17_security_state_t));
    // SECURITY FIX: Use secure timestamp generation
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        g_security_state.last_cleanup = (uint64_t)ts.tv_sec;
    } else {
        g_security_state.last_cleanup = 0; // Fallback
    }
    return 0;
}

// Check rate limit for identifier
bool m17_check_rate_limit(const char *identifier) {
    if (identifier == NULL) {
        return false;
    }
    
    // SECURITY FIX: Use secure timestamp generation
    struct timespec ts;
    uint64_t current_time;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        current_time = (uint64_t)ts.tv_sec;
    } else {
        current_time = 0; // Fallback
    }
    
    // Cleanup old entries periodically
    if ((current_time - g_security_state.last_cleanup) > 600) { // 10 minutes
        m17_security_cleanup_old_entries();
        g_security_state.last_cleanup = current_time;
    }
    
    // Find existing entry
    m17_rate_limit_entry_t *entry = NULL;
    for (uint32_t i = 0; i < g_security_state.entry_count; i++) {
        if (strcmp(g_security_state.entries[i].identifier, identifier) == 0) {
            entry = &g_security_state.entries[i];
            break;
        }
    }
    
    // Create new entry if not found
    if (entry == NULL) {
        if (g_security_state.entry_count >= 100) {
            return false; // Too many tracked entities
        }
        
        entry = &g_security_state.entries[g_security_state.entry_count];
        strncpy(entry->identifier, identifier, 31);
        entry->identifier[31] = '\0';
        entry->attempt_count = 0;
        entry->first_attempt = current_time;
        entry->last_attempt = current_time;
        entry->backoff_until = 0;
        entry->blocked = false;
        g_security_state.entry_count++;
    }
    
    // Check if currently in backoff period
    if (entry->backoff_until > current_time) {
        g_security_state.blocked_attempts++;
        return false;
    }
    
    // Check if within rate limit window
    if ((current_time - entry->first_attempt) < M17_RATE_LIMIT_WINDOW_SECONDS) {
        if (entry->attempt_count >= M17_MAX_AUTH_ATTEMPTS) {
            // Exceeded rate limit - apply backoff
            uint64_t backoff_time = M17_BACKOFF_BASE_SECONDS;
            if (entry->attempt_count > M17_MAX_AUTH_ATTEMPTS) {
                // Exponential backoff
                backoff_time *= (1 << (entry->attempt_count - M17_MAX_AUTH_ATTEMPTS));
                if (backoff_time > M17_MAX_BACKOFF_SECONDS) {
                    backoff_time = M17_MAX_BACKOFF_SECONDS;
                }
            }
            
            entry->backoff_until = current_time + backoff_time;
            entry->blocked = true;
            g_security_state.blocked_attempts++;
            return false;
        }
    } else {
        // Reset window
        entry->attempt_count = 0;
        entry->first_attempt = current_time;
        entry->blocked = false;
    }
    
    return true;
}

// Record security event
void m17_record_security_event(m17_security_event_t event_type, 
                               const char *identifier,
                               const char *details) {
    if (identifier == NULL) {
        return;
    }
    
    g_security_state.total_events++;
    
    // Log event (in production, use proper logging)
    (void)event_type; (void)identifier; (void)details; // Suppress unused warnings
    
    // In production, log to secure log file
    // For now, just increment counters
    
    // Update rate limiting
    if (event_type == M17_EVENT_AUTH_FAILURE || 
        event_type == M17_EVENT_DECRYPT_FAILURE ||
        event_type == M17_EVENT_SIGNATURE_FAILURE) {
        
        // Find or create entry
        m17_rate_limit_entry_t *entry = NULL;
        for (uint32_t i = 0; i < g_security_state.entry_count; i++) {
            if (strcmp(g_security_state.entries[i].identifier, identifier) == 0) {
                entry = &g_security_state.entries[i];
                break;
            }
        }
        
        if (entry != NULL) {
            entry->attempt_count++;
            // SECURITY FIX: Use secure timestamp generation
            struct timespec ts;
            if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
                entry->last_attempt = (uint64_t)ts.tv_sec;
            } else {
                entry->last_attempt = 0; // Fallback
            }
        }
    }
}

// Check for suspicious patterns
bool m17_detect_suspicious_activity(void) {
    // SECURITY FIX: Use secure timestamp generation
    struct timespec ts;
    uint64_t current_time;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        current_time = (uint64_t)ts.tv_sec;
    } else {
        current_time = 0; // Fallback
    }
    
    // Check for high failure rate
    uint32_t recent_failures = 0;
    for (uint32_t i = 0; i < g_security_state.entry_count; i++) {
        m17_rate_limit_entry_t *entry = &g_security_state.entries[i];
        if ((current_time - entry->last_attempt) < 300) { // 5 minutes
            if (entry->attempt_count > 3) {
                recent_failures++;
            }
        }
    }
    
    if (recent_failures > 10) {
        return true; // Suspicious activity detected
    }
    
    // Check for rapid successive attempts
    uint32_t rapid_attempts = 0;
    for (uint32_t i = 0; i < g_security_state.entry_count; i++) {
        m17_rate_limit_entry_t *entry = &g_security_state.entries[i];
        if (entry->attempt_count > 0 && 
            (current_time - entry->last_attempt) < 60) { // 1 minute
            rapid_attempts++;
        }
    }
    
    if (rapid_attempts > 5) {
        return true; // Rapid attempts detected
    }
    
    return false;
}

// Get security statistics
void m17_get_security_stats(uint32_t *total_events, 
                           uint32_t *blocked_attempts,
                           uint32_t *active_entries) {
    if (total_events != NULL) {
        *total_events = g_security_state.total_events;
    }
    if (blocked_attempts != NULL) {
        *blocked_attempts = g_security_state.blocked_attempts;
    }
    if (active_entries != NULL) {
        *active_entries = g_security_state.entry_count;
    }
}

// Cleanup old entries
void m17_security_cleanup_old_entries(void) {
    // SECURITY FIX: Use secure timestamp generation
    struct timespec ts;
    uint64_t current_time;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        current_time = (uint64_t)ts.tv_sec;
    } else {
        current_time = 0; // Fallback
    }
    uint32_t write_index = 0;
    
    for (uint32_t i = 0; i < g_security_state.entry_count; i++) {
        m17_rate_limit_entry_t *entry = &g_security_state.entries[i];
        
        // Keep entries that are recent or still in backoff
        if ((current_time - entry->last_attempt) < 3600 || // 1 hour
            entry->backoff_until > current_time) {
            
            if (write_index != i) {
                g_security_state.entries[write_index] = *entry;
            }
            write_index++;
        }
    }
    
    g_security_state.entry_count = write_index;
}

// Reset security monitoring (for new session)
void m17_security_monitoring_reset(void) {
    // CRITICAL SECURITY FIX: Use secure memory clearing
    explicit_bzero(&g_security_state, sizeof(m17_security_state_t));
    // SECURITY FIX: Use secure timestamp generation
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        g_security_state.last_cleanup = (uint64_t)ts.tv_sec;
    } else {
        g_security_state.last_cleanup = 0; // Fallback
    }
}

// Check if identifier is blocked
bool m17_is_identifier_blocked(const char *identifier) {
    if (identifier == NULL) {
        return false;
    }
    
    // SECURITY FIX: Use secure timestamp generation
    struct timespec ts;
    uint64_t current_time;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        current_time = (uint64_t)ts.tv_sec;
    } else {
        current_time = 0; // Fallback
    }
    
    for (uint32_t i = 0; i < g_security_state.entry_count; i++) {
        m17_rate_limit_entry_t *entry = &g_security_state.entries[i];
        if (strcmp(entry->identifier, identifier) == 0) {
            if (entry->blocked && entry->backoff_until > current_time) {
                return true;
            }
            break;
        }
    }
    
    return false;
}

// Unblock identifier (for legitimate users)
void m17_unblock_identifier(const char *identifier) {
    if (identifier == NULL) {
        return;
    }
    
    for (uint32_t i = 0; i < g_security_state.entry_count; i++) {
        m17_rate_limit_entry_t *entry = &g_security_state.entries[i];
        if (strcmp(entry->identifier, identifier) == 0) {
            entry->blocked = false;
            entry->backoff_until = 0;
            entry->attempt_count = 0;
            break;
        }
    }
}
