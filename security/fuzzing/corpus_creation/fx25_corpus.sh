#!/bin/bash
# FX.25 Fuzzing Corpus Generator
CORPUS_DIR="/home/haaken/github-projects/gr-m17/security/fuzzing/corpus_creation/fx25_corpus"
mkdir -p "$CORPUS_DIR"

echo "Creating FX.25 fuzzing corpus..."

# FX.25 correlation tags (0x01-0x0B)
for tag in {1..11}; do
    printf "\x%02x" $tag > "$CORPUS_DIR/correlation_tag_$tag"
done

# Reed-Solomon parity blocks
dd if=/dev/urandom of="$CORPUS_DIR/rs_parity_1" bs=32 count=1 2>/dev/null
dd if=/dev/urandom of="$CORPUS_DIR/rs_parity_2" bs=64 count=1 2>/dev/null

# AX.25 data with varying corruption
dd if=/dev/urandom of="$CORPUS_DIR/ax25_corrupted_light" bs=128 count=1 2>/dev/null
dd if=/dev/urandom of="$CORPUS_DIR/ax25_corrupted_heavy" bs=128 count=1 2>/dev/null

# Boundary cases
printf "\x00" > "$CORPUS_DIR/single_byte"
dd if=/dev/zero of="$CORPUS_DIR/all_zeros" bs=64 count=1 2>/dev/null

echo "FX.25 Corpus created: $(ls -1 "$CORPUS_DIR" | wc -l) files"
