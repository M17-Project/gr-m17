#!/bin/bash
# SIMD Fuzzing Corpus Generator
CORPUS_DIR="/home/haaken/github-projects/gr-m17/security/fuzzing/corpus_creation/simd_corpus"
mkdir -p "$CORPUS_DIR"

echo "Creating SIMD fuzzing corpus..."

# Aligned data buffers (16, 32, 64 byte boundaries)
for align in 16 32 64; do
    dd if=/dev/urandom of="$CORPUS_DIR/aligned_${align}" bs=$align count=1 2>/dev/null
done

# SIMD register width data
dd if=/dev/urandom of="$CORPUS_DIR/simd_128bit" bs=16 count=1 2>/dev/null
dd if=/dev/urandom of="$CORPUS_DIR/simd_256bit" bs=32 count=1 2>/dev/null
dd if=/dev/urandom of="$CORPUS_DIR/simd_512bit" bs=64 count=1 2>/dev/null

# Edge cases
printf "\x00" > "$CORPUS_DIR/single_byte"
dd if=/dev/zero of="$CORPUS_DIR/empty_buffer" bs=0 count=1 2>/dev/null
dd if=/dev/urandom of="$CORPUS_DIR/max_size" bs=4096 count=1 2>/dev/null

# Patterns for specific SIMD operations
dd if=/dev/urandom of="$CORPUS_DIR/bit_manipulation" bs=32 count=1 2>/dev/null
dd if=/dev/urandom of="$CORPUS_DIR/convolution" bs=64 count=1 2>/dev/null
dd if=/dev/urandom of="$CORPUS_DIR/correlation" bs=128 count=1 2>/dev/null

echo "SIMD Corpus created: $(ls -1 "$CORPUS_DIR" | wc -l) files"
