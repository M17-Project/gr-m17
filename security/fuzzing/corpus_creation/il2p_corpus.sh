#!/bin/bash
# IL2P Fuzzing Corpus Generator
CORPUS_DIR="/home/haaken/github-projects/gr-m17/security/fuzzing/corpus_creation/il2p_corpus"
mkdir -p "$CORPUS_DIR"

echo "Creating IL2P fuzzing corpus..."

# 1. Header types 0x00-0x03 (different payload encoding modes)
echo "Creating header type variations..."
for header_type in 0x00 0x01 0x02 0x03; do
    printf "%02x" $header_type > "$CORPUS_DIR/header_type_${header_type}"
done

# 2. Payload sizes: 0, 1, max (1023), overflow (1024+)
echo "Creating payload size variations..."
# Empty payload
printf "" > "$CORPUS_DIR/payload_empty"
# Single byte payload
printf "\x41" > "$CORPUS_DIR/payload_single"
# Maximum valid payload (1023 bytes)
dd if=/dev/urandom of="$CORPUS_DIR/payload_max_1023" bs=1023 count=1 2>/dev/null
# Overflow payload (1024+ bytes)
dd if=/dev/urandom of="$CORPUS_DIR/payload_overflow_1024" bs=1024 count=1 2>/dev/null
dd if=/dev/urandom of="$CORPUS_DIR/payload_overflow_2048" bs=2048 count=1 2>/dev/null

# 3. Valid IL2P headers with different payload sizes
for size in 64 128 256 512 1024; do
    dd if=/dev/urandom of="$CORPUS_DIR/il2p_header_${size}" bs=$size count=1 2>/dev/null
done

# 4. Sync word variations (valid/invalid preamble)
echo "Creating sync word variations..."
# Valid IL2P sync word
printf "\x1A\xCF\xFC\x1D" > "$CORPUS_DIR/sync_valid"
# Invalid sync words
printf "\x00\x00\x00\x00" > "$CORPUS_DIR/sync_invalid_zeros"
printf "\xFF\xFF\xFF\xFF" > "$CORPUS_DIR/sync_invalid_ones"
printf "\x1A\xCF\xFC\x1C" > "$CORPUS_DIR/sync_near_valid"  # Off by 1 bit

# 5. LDPC parity blocks (valid, corrupted, missing)
echo "Creating LDPC parity variations..."
# Valid LDPC blocks
dd if=/dev/urandom of="$CORPUS_DIR/ldpc_valid_1" bs=32 count=1 2>/dev/null
dd if=/dev/urandom of="$CORPUS_DIR/ldpc_valid_2" bs=64 count=1 2>/dev/null
# Corrupted LDPC blocks (single bit errors)
dd if=/dev/urandom of="$CORPUS_DIR/ldpc_corrupted_1" bs=32 count=1 2>/dev/null
printf "\x01" | dd of="$CORPUS_DIR/ldpc_corrupted_1" bs=1 count=1 conv=notrunc 2>/dev/null
# Missing LDPC blocks (empty)
printf "" > "$CORPUS_DIR/ldpc_missing"

# 6. Maximum frame size testing (Header + max payload + parity)
echo "Creating maximum frame size tests..."
# Complete frame: header + max payload + parity
dd if=/dev/urandom of="$CORPUS_DIR/frame_max_size" bs=1023 count=1 2>/dev/null
# Frame with overflow
dd if=/dev/urandom of="$CORPUS_DIR/frame_overflow" bs=1024 count=1 2>/dev/null

# 7. Concatenated packets (if IL2P supports streams)
echo "Creating concatenated packet tests..."
# Multiple packets concatenated
dd if=/dev/urandom of="$CORPUS_DIR/concatenated_2" bs=512 count=2 2>/dev/null
dd if=/dev/urandom of="$CORPUS_DIR/concatenated_4" bs=256 count=4 2>/dev/null

# 8. Fragmented payloads (multi-block transfers)
echo "Creating fragmented payload tests..."
# Fragmented data blocks
dd if=/dev/urandom of="$CORPUS_DIR/fragment_1" bs=256 count=1 2>/dev/null
dd if=/dev/urandom of="$CORPUS_DIR/fragment_2" bs=256 count=1 2>/dev/null
dd if=/dev/urandom of="$CORPUS_DIR/fragment_3" bs=256 count=1 2>/dev/null

# 9. Scrambled/descrambled data pairs
dd if=/dev/urandom of="$CORPUS_DIR/scrambled_data" bs=128 count=1 2>/dev/null
dd if=/dev/urandom of="$CORPUS_DIR/descrambled_data" bs=128 count=1 2>/dev/null

# 10. Edge cases
printf "\x00" > "$CORPUS_DIR/single_byte"
dd if=/dev/zero of="$CORPUS_DIR/all_zeros" bs=64 count=1 2>/dev/null

echo "IL2P Corpus created: $(ls -1 "$CORPUS_DIR" | wc -l) files"
