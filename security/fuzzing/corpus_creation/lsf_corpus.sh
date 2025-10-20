#!/bin/bash
# M17 LSF (Link Setup Frame) Fuzzing Corpus Generator
# Creates realistic M17 LSF packets for comprehensive fuzzing

CORPUS_DIR="/home/haaken/github-projects/gr-m17/security/fuzzing/corpus_creation/lsf_corpus"
mkdir -p "$CORPUS_DIR"

echo "Creating M17 LSF fuzzing corpus..."

# M17 Base-40 character set: " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-/."
BASE40_CHARS=" 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-/."

# Convert callsign to base-40 encoded 48-bit integer
encode_callsign_base40() {
    local callsign="$1"
    local result=0
    
    # Pad to 9 characters with spaces
    callsign=$(printf "%-9s" "$callsign")
    callsign="${callsign:0:9}"
    
    # Convert each character to base-40
    for ((i=0; i<9; i++)); do
        local char="${callsign:$i:1}"
        local value=0
        
        # Find character in base-40 set
        for ((j=0; j<40; j++)); do
            if [ "${BASE40_CHARS:$j:1}" = "$char" ]; then
                value=$j
                break
            fi
        done
        
        # Multiply by 40^i and add to result
        result=$((result * 40 + value))
    done
    
    # Convert to 6-byte hex (48 bits)
    printf "%012x" $result
}

# Calculate CRC16-CCITT
calculate_crc16() {
    local data="$1"
    local crc=0xFFFF
    
    # Process each byte
    for ((i=0; i<${#data}; i+=2)); do
        local byte="0x${data:$i:2}"
        crc=$((crc ^ byte))
        
        for ((j=0; j<8; j++)); do
            if [ $((crc & 1)) -eq 1 ]; then
                crc=$((crc >> 1))
                crc=$((crc ^ 0x8408))
            else
                crc=$((crc >> 1))
            fi
        done
    done
    
    printf "%04x" $crc
}

# Helper function to create LSF frame
create_lsf_frame() {
    local dest="$1"
    local src="$2"
    local packet_type="$3"    # 0-7 (3 bits)
    local data_type="$4"      # 0-3 (2 bits) 
    local enc_type="$5"       # 0-63 (6 bits)
    local meta="$6"           # 14 bytes hex
    local name="$7"
    
    # M17 LSF structure: DST(6) + SRC(6) + TYPE(2) + META(14) + CRC(2) = 30 bytes
    
    # Encode callsigns to base-40
    local dst_encoded=$(encode_callsign_base40 "$dest")
    local src_encoded=$(encode_callsign_base40 "$src")
    
    # Build TYPE field (16 bits)
    # Bits 0-2: Packet/Stream type (0-7)
    # Bits 3-4: Data type (0-3) 
    # Bits 5-10: Encryption type (0-63)
    # Bits 11-15: Reserved (0)
    local type_field=$((packet_type | (data_type << 3) | (enc_type << 5)))
    local type_hex=$(printf "%04x" $type_field)
    
    # Create LSF without CRC
    local lsf="${dst_encoded}${src_encoded}${type_hex}${meta}"
    
    # Calculate CRC16-CCITT
    local crc=$(calculate_crc16 "$lsf")
    
    # Final frame with CRC
    local frame="${lsf}${crc}"
    
    echo "$frame" | xxd -r -p > "$CORPUS_DIR/$name"
}

# 1. Valid LSF packets with different destination addresses
echo "Creating valid LSF packets..."

# Basic voice stream (packet_type=0, data_type=0, enc_type=0)
create_lsf_frame "N0CALL" "W1ABC" "0" "0" "0" "0000000000000000000000000000" "voice_stream"

# Data stream (packet_type=1, data_type=0, enc_type=0)
create_lsf_frame "N0CALL" "W1ABC" "1" "0" "0" "0000000000000000000000000000" "data_stream"

# Encrypted voice (packet_type=0, data_type=0, enc_type=1)
create_lsf_frame "N0CALL" "W1ABC" "0" "0" "1" "0000000000000000000000000000" "encrypted_voice"

# Encrypted data (packet_type=1, data_type=0, enc_type=1)
create_lsf_frame "N0CALL" "W1ABC" "1" "0" "1" "0000000000000000000000000000" "encrypted_data"

# 2. Different data/stream types and encryption flags
echo "Creating stream type variations..."

# Stream with encryption type 1
create_lsf_frame "N0CALL" "W1ABC" "0" "0" "1" "0000000000000000000000000000" "stream_enc_type1"

# Stream with encryption type 2
create_lsf_frame "N0CALL" "W1ABC" "0" "0" "2" "0000000000000000000000000000" "stream_enc_type2"

# Stream with encryption type 3
create_lsf_frame "N0CALL" "W1ABC" "0" "0" "3" "0000000000000000000000000000" "stream_enc_type3"

# 3. META field variations (proper M17 META usage)
echo "Creating META field variations..."

# Empty META (no special data)
create_lsf_frame "N0CALL" "W1ABC" "0" "0" "0" "0000000000000000000000000000" "meta_empty"

# CAN counter in META (first 4 bytes)
create_lsf_frame "N0CALL" "W1ABC" "0" "0" "0" "0100000000000000000000000000" "meta_can_counter"

# GNSS location in META (simplified)
create_lsf_frame "N0CALL" "W1ABC" "0" "0" "0" "0200000000000000000000000000" "meta_gnss_location"

# Text message in META
create_lsf_frame "N0CALL" "W1ABC" "0" "0" "0" "48656C6C6F20576F726C6400000000" "meta_text_message"

# Binary data in META
create_lsf_frame "N0CALL" "W1ABC" "0" "0" "0" "0123456789ABCDEF0123456789ABCD" "meta_binary"

# All zeros META
create_lsf_frame "N0CALL" "W1ABC" "0" "0" "0" "0000000000000000000000000000" "meta_zeros"

# All ones META
create_lsf_frame "N0CALL" "W1ABC" "0" "0" "0" "FFFFFFFFFFFFFFFFFFFFFFFFFFFF" "meta_ones"

# 4. CRC16 valid and near-valid frames
echo "Creating CRC variations..."

# Valid CRC (will be calculated properly)
create_lsf_frame "N0CALL" "W1ABC" "0" "0" "0" "0000000000000000000000000000" "crc_valid"

# Near-valid CRC (off by 1 bit in META)
create_lsf_frame "N0CALL" "W1ABC" "0" "0" "0" "0000000000000000000000000001" "crc_near_valid1"

# Near-valid CRC (off by 2 bits in META)
create_lsf_frame "N0CALL" "W1ABC" "0" "0" "0" "0000000000000000000000000003" "crc_near_valid2"

# 5. Frames with different encoding types
echo "Creating encoding type variations..."

# Base-40 callsign encoding edge cases
create_lsf_frame "A" "B" "0" "0" "0" "0000000000000000000000000000" "callsign_minimal"

# Long callsigns (truncated)
create_lsf_frame "ABCDEF" "GHIJKL" "0" "0" "0" "0000000000000000000000000000" "callsign_long"

# Special characters in callsigns
create_lsf_frame "TEST-1" "CALL-15" "0" "0" "0" "0000000000000000000000000000" "callsign_special"

# 6. Edge cases in callsign encoding (base-40)
echo "Creating base-40 edge cases..."

# Numbers only
create_lsf_frame "123456" "789012" "0" "0" "0" "0000000000000000000000000000" "callsign_numbers"

# Mixed alphanumeric
create_lsf_frame "ABC123" "DEF456" "0" "0" "0" "0000000000000000000000000000" "callsign_mixed"

# 7. Boundary and stress test cases
echo "Creating boundary cases..."

# Minimum valid LSF (30 bytes)
printf "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" > "$CORPUS_DIR/lsf_minimum"

# Maximum valid LSF (30 bytes, all 0xFF)
printf "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" > "$CORPUS_DIR/lsf_maximum"

# 8. Real-world M17 LSF examples
echo "Creating real-world examples..."

# Typical voice call
create_lsf_frame "N0CALL" "W1ABC" "0" "0" "0" "0000000000000000000000000000" "real_voice_call"

# Data transmission
create_lsf_frame "N0CALL" "W1ABC" "1" "0" "0" "48656C6C6F20576F726C6400000000" "real_data_transmission"

# Encrypted communication
create_lsf_frame "N0CALL" "W1ABC" "0" "0" "1" "0123456789ABCDEF0123456789ABCD" "real_encrypted"

# 9. Error injection cases
echo "Creating error injection cases..."

# Single bit errors in different positions
create_lsf_frame "N0CALL" "W1ABC" "0" "0" "0" "0000000000000000000000000000" "error_single_bit1"
printf "\x01" | dd of="$CORPUS_DIR/error_single_bit1" bs=1 count=1 conv=notrunc 2>/dev/null

create_lsf_frame "N0CALL" "W1ABC" "0" "0" "0" "0000000000000000000000000000" "error_single_bit2"
printf "\x02" | dd of="$CORPUS_DIR/error_single_bit2" bs=1 count=1 conv=notrunc 2>/dev/null

# 10. Protocol-specific variations
echo "Creating protocol-specific cases..."

# Different packet types
create_lsf_frame "N0CALL" "W1ABC" "2" "0" "0" "0000000000000000000000000000" "packet_type_2"
create_lsf_frame "N0CALL" "W1ABC" "3" "0" "0" "0000000000000000000000000000" "packet_type_3"

# Different data types
create_lsf_frame "N0CALL" "W1ABC" "0" "1" "0" "0000000000000000000000000000" "data_type_1"
create_lsf_frame "N0CALL" "W1ABC" "0" "2" "0" "0000000000000000000000000000" "data_type_2"

# 11. Broadcast and empty addresses
echo "Creating broadcast and empty address cases..."

# Broadcast address (0xFFFFFFFFFFFF)
create_lsf_frame "FFFFFFFFFFFF" "N0CALL" "0" "0" "0" "0000000000000000000000000000" "broadcast_address"

# Empty address (0x000000000000)
create_lsf_frame "000000000000" "N0CALL" "0" "0" "0" "0000000000000000000000000000" "empty_address"

# Both broadcast
create_lsf_frame "FFFFFFFFFFFF" "FFFFFFFFFFFF" "0" "0" "0" "0000000000000000000000000000" "both_broadcast"

# Both empty
create_lsf_frame "000000000000" "000000000000" "0" "0" "0" "0000000000000000000000000000" "both_empty"

# 12. All encryption type combinations (0-63 values)
echo "Creating encryption type variations..."
for enc_type in 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 51 52 53 54 55 56 57 58 59 60 61 62 63; do
    create_lsf_frame "N0CALL" "W1ABC" "0" "0" "$enc_type" "0000000000000000000000000000" "enc_type_${enc_type}"
done

# 13. Reserved bit patterns (bits 11-15 in TYPE)
echo "Creating reserved bit pattern tests..."

# Reserved bits set to various patterns
create_lsf_frame "N0CALL" "W1ABC" "0" "0" "0" "8000000000000000000000000000" "reserved_0x8000"  # Bit 15 set
create_lsf_frame "N0CALL" "W1ABC" "0" "0" "0" "4000000000000000000000000000" "reserved_0x4000"  # Bit 14 set
create_lsf_frame "N0CALL" "W1ABC" "0" "0" "0" "2000000000000000000000000000" "reserved_0x2000"  # Bit 13 set
create_lsf_frame "N0CALL" "W1ABC" "0" "0" "0" "1000000000000000000000000000" "reserved_0x1000"  # Bit 12 set
create_lsf_frame "N0CALL" "W1ABC" "0" "0" "0" "0800000000000000000000000000" "reserved_0x0800"  # Bit 11 set
create_lsf_frame "N0CALL" "W1ABC" "0" "0" "0" "F800000000000000000000000000" "reserved_0xF800"  # All reserved bits set

# Create summary
echo ""
echo "LSF Corpus created in: $CORPUS_DIR"
echo "Files created: $(ls -1 "$CORPUS_DIR" | wc -l)"
echo ""
echo "Corpus contents:"
ls -la "$CORPUS_DIR"
