//--------------------------------------------------------------------
// M17 C library - crypto/security_monitoring.h
//
// Security monitoring and rate limiting
// Implements protection against brute force and monitoring
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------

#ifndef M17_SECURITY_MONITORING_H
#define M17_SECURITY_MONITORING_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Security event types
typedef enum {
    M17_EVENT_AUTH_FAILURE,
    M17_EVENT_DECRYPT_FAILURE,
    M17_EVENT_SIGNATURE_FAILURE,
    M17_EVENT_REPLAY_ATTACK,
    M17_EVENT_SUSPICIOUS_PATTERN,
    M17_EVENT_RATE_LIMIT_EXCEEDED
} m17_security_event_t;

// Initialize security monitoring
int m17_security_monitoring_init(void);

// Check rate limit for identifier
bool m17_check_rate_limit(const char *identifier);

// Record security event
void m17_record_security_event(m17_security_event_t event_type, 
                               const char *identifier,
                               const char *details);

// Check for suspicious patterns
bool m17_detect_suspicious_activity(void);

// Get security statistics
void m17_get_security_stats(uint32_t *total_events, 
                           uint32_t *blocked_attempts,
                           uint32_t *active_entries);

// Cleanup old entries
void m17_security_cleanup_old_entries(void);

// Reset security monitoring (for new session)
void m17_security_monitoring_reset(void);

// Check if identifier is blocked
bool m17_is_identifier_blocked(const char *identifier);

// Unblock identifier (for legitimate users)
void m17_unblock_identifier(const char *identifier);

#ifdef __cplusplus
}
#endif

#endif // M17_SECURITY_MONITORING_H








