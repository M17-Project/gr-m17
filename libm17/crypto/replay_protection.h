//--------------------------------------------------------------------
// M17 C library - crypto/replay_protection.h
//
// Replay attack protection with sequence number validation
// Implements sliding window for frame number validation
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------

#ifndef M17_REPLAY_PROTECTION_H
#define M17_REPLAY_PROTECTION_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize replay protection
int m17_replay_protection_init(void);

// Check if frame number is valid (not replayed)
bool m17_check_frame_replay(uint16_t frame_number);

// Add frame to replay protection window
int m17_add_frame_to_window(uint16_t frame_number);

// Cleanup old entries from window
void m17_replay_cleanup_old_entries(void);

// Get replay protection statistics
void m17_get_replay_stats(uint32_t *total_frames, uint32_t *rejected_frames);

// Reset replay protection (for new session)
void m17_replay_protection_reset(void);

// Validate frame sequence
bool m17_validate_frame_sequence(uint16_t frame_number, uint16_t expected_frame);

// Check for suspicious patterns
bool m17_detect_suspicious_patterns(void);

#ifdef __cplusplus
}
#endif

#endif // M17_REPLAY_PROTECTION_H








