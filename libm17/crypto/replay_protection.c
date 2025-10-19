//--------------------------------------------------------------------
// M17 C library - crypto/replay_protection.c
//
// Replay attack protection with sequence number validation
// Implements sliding window for frame number validation
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------

#include "m17.h"
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

// Replay protection configuration
#define M17_REPLAY_WINDOW_SIZE 64
#define M17_REPLAY_MAX_AGE_SECONDS 3600  // 1 hour
#define M17_REPLAY_MAX_FRAMES 10000

// Frame record structure
typedef struct {
    uint16_t frame_number;
    uint64_t timestamp;
    uint8_t seen;
} m17_frame_record_t;

// Replay protection state
typedef struct {
    m17_frame_record_t window[M17_REPLAY_WINDOW_SIZE];
    uint16_t window_start;
    uint16_t window_end;
    uint64_t last_cleanup;
    uint32_t total_frames;
    uint32_t rejected_frames;
} m17_replay_state_t;

// Global replay protection state
static m17_replay_state_t g_replay_state = {0};

// Initialize replay protection
int m17_replay_protection_init(void) {
    // CRITICAL SECURITY FIX: Use secure memory clearing
    explicit_bzero(&g_replay_state, sizeof(m17_replay_state_t));
    // SECURITY FIX: Use secure timestamp generation
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        g_replay_state.last_cleanup = (uint64_t)ts.tv_sec;
    } else {
        g_replay_state.last_cleanup = 0; // Fallback
    }
    return 0;
}

// Check if frame number is valid (not replayed)
bool m17_check_frame_replay(uint16_t frame_number) {
    // SECURITY FIX: Use secure timestamp generation
    struct timespec ts;
    uint64_t current_time;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        current_time = (uint64_t)ts.tv_sec;
    } else {
        current_time = 0; // Fallback
    }
    
    // Cleanup old entries periodically
    if ((current_time - g_replay_state.last_cleanup) > 300) { // 5 minutes
        m17_replay_cleanup_old_entries();
        g_replay_state.last_cleanup = current_time;
    }
    
    // Check if frame is in window
    for (int i = 0; i < M17_REPLAY_WINDOW_SIZE; i++) {
        if (g_replay_state.window[i].seen && 
            g_replay_state.window[i].frame_number == frame_number) {
            // Frame already seen - replay attack!
            g_replay_state.rejected_frames++;
            return false;
        }
    }
    
    // Check if frame is too old
    if (g_replay_state.total_frames > 0) {
        uint16_t oldest_frame = g_replay_state.window[g_replay_state.window_start].frame_number;
        if (frame_number < oldest_frame && 
            (oldest_frame - frame_number) > M17_REPLAY_MAX_FRAMES) {
            // Frame is too old
            g_replay_state.rejected_frames++;
            return false;
        }
    }
    
    return true;
}

// Add frame to replay protection window
int m17_add_frame_to_window(uint16_t frame_number) {
    // SECURITY FIX: Use secure timestamp generation
    struct timespec ts;
    uint64_t current_time;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        current_time = (uint64_t)ts.tv_sec;
    } else {
        current_time = 0; // Fallback
    }
    
    // Find slot in window
    int slot = -1;
    for (int i = 0; i < M17_REPLAY_WINDOW_SIZE; i++) {
        if (!g_replay_state.window[i].seen) {
            slot = i;
            break;
        }
    }
    
    // If no free slot, use oldest slot
    if (slot == -1) {
        slot = g_replay_state.window_start;
        g_replay_state.window_start = (g_replay_state.window_start + 1) % M17_REPLAY_WINDOW_SIZE;
    }
    
    // Add frame to window
    g_replay_state.window[slot].frame_number = frame_number;
    g_replay_state.window[slot].timestamp = current_time;
    g_replay_state.window[slot].seen = 1;
    
    g_replay_state.total_frames++;
    
    return 0;
}

// Cleanup old entries from window
void m17_replay_cleanup_old_entries(void) {
    // SECURITY FIX: Use secure timestamp generation
    struct timespec ts;
    uint64_t current_time;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        current_time = (uint64_t)ts.tv_sec;
    } else {
        current_time = 0; // Fallback
    }
    
    for (int i = 0; i < M17_REPLAY_WINDOW_SIZE; i++) {
        if (g_replay_state.window[i].seen &&
            (current_time - g_replay_state.window[i].timestamp) > M17_REPLAY_MAX_AGE_SECONDS) {
            g_replay_state.window[i].seen = 0;
        }
    }
}

// Get replay protection statistics
void m17_get_replay_stats(uint32_t *total_frames, uint32_t *rejected_frames) {
    if (total_frames != NULL) {
        *total_frames = g_replay_state.total_frames;
    }
    if (rejected_frames != NULL) {
        *rejected_frames = g_replay_state.rejected_frames;
    }
}

// Reset replay protection (for new session)
void m17_replay_protection_reset(void) {
    // CRITICAL SECURITY FIX: Use secure memory clearing
    explicit_bzero(&g_replay_state, sizeof(m17_replay_state_t));
    // SECURITY FIX: Use secure timestamp generation
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        g_replay_state.last_cleanup = (uint64_t)ts.tv_sec;
    } else {
        g_replay_state.last_cleanup = 0; // Fallback
    }
}

// Validate frame sequence
bool m17_validate_frame_sequence(uint16_t frame_number, uint16_t expected_frame) {
    // Allow some tolerance for out-of-order frames
    const uint16_t tolerance = 10;
    
    if (frame_number == expected_frame) {
        return true;
    }
    
    // Check if frame is within tolerance
    if (frame_number > expected_frame) {
        return (frame_number - expected_frame) <= tolerance;
    } else {
        return (expected_frame - frame_number) <= tolerance;
    }
}

// Check for suspicious patterns
bool m17_detect_suspicious_patterns(void) {
    // Check for too many rejected frames
    if (g_replay_state.rejected_frames > 100) {
        return true;
    }
    
    // Check for rapid frame number changes (possible attack)
    uint32_t rapid_changes = 0;
    for (int i = 1; i < M17_REPLAY_WINDOW_SIZE; i++) {
        if (g_replay_state.window[i].seen && g_replay_state.window[i-1].seen) {
            uint16_t diff = g_replay_state.window[i].frame_number - 
                           g_replay_state.window[i-1].frame_number;
            if (diff > 100) { // Suspicious jump
                rapid_changes++;
            }
        }
    }
    
    return rapid_changes > 5;
}

