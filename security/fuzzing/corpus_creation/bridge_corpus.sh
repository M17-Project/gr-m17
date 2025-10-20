#!/bin/bash
# Bridge Fuzzing Corpus Generator
CORPUS_DIR="/home/haaken/github-projects/gr-m17/security/fuzzing/corpus_creation/bridge_corpus"
mkdir -p "$CORPUS_DIR"

echo "Creating Bridge fuzzing corpus..."

# 1. State transition sequences (M17 LSF → M17 stream → AX.25)
echo "Creating state transition sequences..."

# M17 LSF → M17 stream → AX.25 conversion
printf "LSF_START\nSTREAM_DATA\nAX25_CONVERSION\n" > "$CORPUS_DIR/state_m17_lsf_to_ax25"

# AX.25 UI → M17 packet conversion
printf "AX25_UI_FRAME\nM17_PACKET_CONVERSION\n" > "$CORPUS_DIR/state_ax25_to_m17_packet"

# Mixed protocol boundaries
printf "M17_BOUNDARY\nAX25_BOUNDARY\nFX25_BOUNDARY\n" > "$CORPUS_DIR/state_mixed_boundaries"

# Partial conversions (interrupted mid-frame)
printf "M17_START\nINTERRUPTED\n" > "$CORPUS_DIR/state_partial_conversion"

# 2. Multi-protocol chains (M17 → AX.25 → M17)
echo "Creating multi-protocol chains..."

# Protocol switching: M17 → AX.25 → M17
printf "M17_INIT\nAX25_SWITCH\nM17_RETURN\n" > "$CORPUS_DIR/chain_m17_ax25_m17"

# Complex protocol chain
printf "M17_LSF\nM17_STREAM\nAX25_UI\nFX25_DATA\nM17_PACKET\n" > "$CORPUS_DIR/chain_complex"

# 3. Address translation edge cases
echo "Creating address translation edge cases..."

# Invalid callsign mappings
printf "INVALID_CALLSIGN\n" > "$CORPUS_DIR/addr_invalid"
printf "TOO_LONG_CALLSIGN_123456789\n" > "$CORPUS_DIR/addr_too_long"
printf "SPECIAL_CHARS_!@#\n" > "$CORPUS_DIR/addr_special_chars"

# Edge case addresses
printf "N0CALL" > "$CORPUS_DIR/addr_simple"
printf "W1ABC-15" > "$CORPUS_DIR/addr_ssid"
printf "CQ" > "$CORPUS_DIR/addr_broadcast"
printf "FFFFFFFFFFFF" > "$CORPUS_DIR/addr_broadcast_hex"
printf "000000000000" > "$CORPUS_DIR/addr_empty"

# 4. Timing sequences (fast protocol switching)
echo "Creating timing sequence tests..."

# Fast protocol switching
printf "M17\nAX25\nM17\nAX25\nM17\n" > "$CORPUS_DIR/timing_fast_switch"

# Burst switching
printf "BURST_START\nM17\nAX25\nFX25\nM17\nBURST_END\n" > "$CORPUS_DIR/timing_burst_switch"

# 5. Handshake patterns (setup → data → teardown)
echo "Creating handshake patterns..."

# Complete handshake
printf "SETUP\nHANDSHAKE\nDATA_TRANSFER\nTEARDOWN\n" > "$CORPUS_DIR/handshake_complete"

# Interrupted handshake
printf "SETUP\nHANDSHAKE\nINTERRUPTED\n" > "$CORPUS_DIR/handshake_interrupted"

# Failed handshake
printf "SETUP\nHANDSHAKE_FAILED\n" > "$CORPUS_DIR/handshake_failed"

# 6. Error recovery sequences
echo "Creating error recovery tests..."

# Conversion failure recovery
printf "M17_START\nCONVERSION_FAILED\nERROR_RECOVERY\nRETRY\n" > "$CORPUS_DIR/error_recovery_conversion"

# Protocol mismatch recovery
printf "PROTOCOL_MISMATCH\nERROR_DETECTED\nRECOVERY_ATTEMPT\n" > "$CORPUS_DIR/error_recovery_protocol"

# 7. Buffer management (multiple queued conversions)
echo "Creating buffer management tests..."

# Multiple queued conversions
printf "QUEUE_1\nQUEUE_2\nQUEUE_3\nPROCESS_ALL\n" > "$CORPUS_DIR/buffer_multiple_queue"

# Buffer overflow simulation
printf "BUFFER_FILL\nOVERFLOW_ATTEMPT\n" > "$CORPUS_DIR/buffer_overflow_test"

# 8. Concurrent multi-protocol streams
dd if=/dev/urandom of="$CORPUS_DIR/concurrent_stream_1" bs=32 count=1 2>/dev/null
dd if=/dev/urandom of="$CORPUS_DIR/concurrent_stream_2" bs=32 count=1 2>/dev/null

# 9. Mixed protocol packets
dd if=/dev/urandom of="$CORPUS_DIR/mixed_protocol_1" bs=64 count=1 2>/dev/null
dd if=/dev/urandom of="$CORPUS_DIR/mixed_protocol_2" bs=128 count=1 2>/dev/null

echo "Bridge Corpus created: $(ls -1 "$CORPUS_DIR" | wc -l) files"
