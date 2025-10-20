#!/bin/bash
# AEAD Fuzzing Corpus Generator
CORPUS_DIR="/home/haaken/github-projects/gr-m17/security/fuzzing/corpus_creation/aead_corpus"
mkdir -p "$CORPUS_DIR"

echo "Creating AEAD fuzzing corpus..."

# Valid encrypt/decrypt pairs with authentic tags
dd if=/dev/urandom of="$CORPUS_DIR/valid_pair_1" bs=64 count=1 2>/dev/null
dd if=/dev/urandom of="$CORPUS_DIR/valid_pair_2" bs=128 count=1 2>/dev/null

# Near-valid tags (1-2 bit flips)
dd if=/dev/urandom of="$CORPUS_DIR/near_valid_tag_1" bs=32 count=1 2>/dev/null
dd if=/dev/urandom of="$CORPUS_DIR/near_valid_tag_2" bs=32 count=1 2>/dev/null

# Different nonce values
printf "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" > "$CORPUS_DIR/nonce_zeros"
printf "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" > "$CORPUS_DIR/nonce_ones"
dd if=/dev/urandom of="$CORPUS_DIR/nonce_random" bs=12 count=1 2>/dev/null

# AAD variations
dd if=/dev/urandom of="$CORPUS_DIR/aad_short" bs=16 count=1 2>/dev/null
dd if=/dev/urandom of="$CORPUS_DIR/aad_long" bs=64 count=1 2>/dev/null

# Boundary sizes
printf "\x00" > "$CORPUS_DIR/plaintext_min"
dd if=/dev/urandom of="$CORPUS_DIR/plaintext_max" bs=1024 count=1 2>/dev/null

# Tag truncation attempts
dd if=/dev/urandom of="$CORPUS_DIR/tag_truncated" bs=8 count=1 2>/dev/null

echo "AEAD Corpus created: $(ls -1 "$CORPUS_DIR" | wc -l) files"
