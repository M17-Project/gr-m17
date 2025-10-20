#!/bin/bash
# AX.25 Fuzzing Corpus Generator
# Creates realistic AX.25 frames for comprehensive fuzzing

CORPUS_DIR="/home/haaken/github-projects/gr-m17/security/fuzzing/corpus_creation/ax25_corpus"
mkdir -p "$CORPUS_DIR"

echo "Creating AX.25 fuzzing corpus..."

# Helper function to create AX.25 frame
create_ax25_frame() {
    local dest="$1"
    local src="$2"
    local ctrl="$3"
    local pid="$4"
    local info="$5"
    local name="$6"
    
    # Convert callsigns to AX.25 format (6 bytes, left-shifted)
    local dest_bytes=""
    for ((i=0; i<6; i++)); do
        if [ $i -lt ${#dest} ]; then
            local char="${dest:$i:1}"
            local byte=$(printf "%d" "'$char")
            dest_bytes+=$(printf "%02x" $((byte << 1)))
        else
            dest_bytes+="40"  # Space
        fi
    done
    
    local src_bytes=""
    for ((i=0; i<6; i++)); do
        if [ $i -lt ${#src} ]; then
            local char="${src:$i:1}"
            local byte=$(printf "%d" "'$char")
            src_bytes+=$(printf "%02x" $((byte << 1)))
        else
            src_bytes+="40"  # Space
        fi
    done
    
    # Set address extension bits
    local dest_last=$(printf "%02x" $((0x01)))  # Last address, not repeated
    local src_last=$(printf "%02x" $((0x01)))   # Last address, not repeated
    
    # Create frame: dest(7) + src(7) + ctrl(1) + pid(1) + info
    local frame="${dest_bytes}${dest_last}${src_bytes}${src_last}${ctrl}${pid}${info}"
    
    echo "$frame" | xxd -r -p > "$CORPUS_DIR/$name"
}

# 1. Valid AX.25 frames from real packet radio captures
echo "Creating valid AX.25 frames..."

# UI frame - APRS position report
create_ax25_frame "APRS" "N0CALL" "03" "F0" "!4903.50N/07201.75W-Test" "ui_aprs_position"

# UI frame - APRS message
create_ax25_frame "N0CALL" "W1ABC" "03" "F0" ":W1ABC :Hello from N0CALL" "ui_aprs_message"

# I-frame - Information frame
create_ax25_frame "N0CALL" "W1ABC" "00" "F0" "This is an information frame" "i_frame_data"

# S-frame - RR (Receive Ready)
create_ax25_frame "N0CALL" "W1ABC" "01" "" "" "s_frame_rr"

# S-frame - RNR (Receive Not Ready)
create_ax25_frame "N0CALL" "W1ABC" "05" "" "" "s_frame_rnr"

# U-frame - SABM (Set Asynchronous Balanced Mode)
create_ax25_frame "N0CALL" "W1ABC" "2F" "" "" "u_frame_sabm"

# U-frame - UA (Unnumbered Acknowledgment)
create_ax25_frame "N0CALL" "W1ABC" "63" "" "" "u_frame_ua"

# U-frame - DISC (Disconnect)
create_ax25_frame "N0CALL" "W1ABC" "43" "" "" "u_frame_disc"

# 2. Frames with various callsign formats (SSID variations, wildcards)
echo "Creating callsign variations..."

# SSID variations
create_ax25_frame "N0CALL-1" "W1ABC-15" "03" "F0" "SSID test" "callsign_ssid_variations"

# Wildcard callsigns
create_ax25_frame "CQ" "N0CALL" "03" "F0" "CQ CQ CQ" "callsign_cq"

# Broadcast address
create_ax25_frame "QST" "N0CALL" "03" "F0" "General announcement" "callsign_broadcast"

# 3. Different PID values
echo "Creating different PID values..."

# No layer 3
create_ax25_frame "N0CALL" "W1ABC" "03" "F0" "No layer 3 data" "pid_no_layer3"

# IP datagram
create_ax25_frame "N0CALL" "W1ABC" "03" "CC" "IP datagram data" "pid_ip"

# Compressed TCP/IP
create_ax25_frame "N0CALL" "W1ABC" "03" "06" "Compressed TCP data" "pid_tcp_compressed"

# 4. Control field variations
echo "Creating control field variations..."

# I-frame with P/F bits
create_ax25_frame "N0CALL" "W1ABC" "80" "F0" "I-frame with P bit" "ctrl_i_poll"

# S-frame with P/F bits
create_ax25_frame "N0CALL" "W1ABC" "11" "" "" "ctrl_s_poll"

# U-frame with P/F bits
create_ax25_frame "N0CALL" "W1ABC" "3F" "" "" "ctrl_u_poll"

# 5. Minimum and maximum length frames
echo "Creating length variations..."

# Minimum length (UI frame with no info)
create_ax25_frame "N0CALL" "W1ABC" "03" "F0" "" "min_length"

# Maximum length (UI frame with max info - 256 bytes)
local max_info=""
for i in {1..256}; do
    max_info+="A"
done
create_ax25_frame "N0CALL" "W1ABC" "03" "F0" "$max_info" "max_length"

# 6. Address extension bits
echo "Creating address extension variations..."

# Multiple addresses (digipeater path)
create_ax25_frame "WIDE1-1,WIDE2-1" "N0CALL" "03" "F0" "Digipeater test" "digipeater_path"

# Digipeater chain variations
create_ax25_frame "N0CALL" "W1ABC" "03" "F0" "data" "digi_chain_empty"
create_ax25_frame "N0CALL" "W1ABC,WIDE1-1,WIDE2-2" "03" "F0" "data" "digi_chain_short"
create_ax25_frame "N0CALL" "W1ABC,D1,D2,D3,D4,D5,D6,D7,D8" "03" "F0" "data" "digi_chain_max"
create_ax25_frame "N0CALL" "W1ABC,D1,D2,D3,D4,D5,D6,D7,D8,D9" "03" "F0" "data" "digi_chain_overflow"

# 7. Edge cases and malformed frames
echo "Creating edge cases..."

# Empty frame (just addresses)
printf "\x40\x40\x40\x40\x40\x40\x40\x40\x40\x40\x40\x40\x40\x40" > "$CORPUS_DIR/empty_frame"

# Single byte
printf "\x40" > "$CORPUS_DIR/single_byte"

# All zeros
dd if=/dev/zero of="$CORPUS_DIR/all_zeros" bs=32 count=1 2>/dev/null

# All ones
dd if=/dev/zero of="$CORPUS_DIR/all_ones" bs=32 count=1 2>/dev/null
printf "\xFF" | dd of="$CORPUS_DIR/all_ones" bs=1 count=32 conv=notrunc 2>/dev/null

# Random data
dd if=/dev/urandom of="$CORPUS_DIR/random_data" bs=64 count=1 2>/dev/null

# 8. Real-world packet captures (simplified)
echo "Creating real-world examples..."

# APRS weather report
create_ax25_frame "APRS" "W1ABC-1" "03" "F0" "!4903.50N/07201.75W_180/000g000t068r000p000P000h50b10201L000" "aprs_weather"

# APRS object
create_ax25_frame "APRS" "W1ABC-1" "03" "F0" ";TEST*111111z4903.50N/07201.75WrTest object" "aprs_object"

# 9. Stress test cases
echo "Creating stress test cases..."

# Very long callsign (edge case)
create_ax25_frame "ABCDEF" "GHIJKL" "03" "F0" "Long callsign test" "long_callsign"

# Special characters in callsign
create_ax25_frame "TEST-1" "CALL-15" "03" "F0" "Special chars test" "special_chars"

# 10. Protocol-specific variations
echo "Creating protocol-specific cases..."

# AX.25 with different data rates (simulated)
create_ax25_frame "N0CALL" "W1ABC" "03" "F0" "1200 baud data" "data_1200baud"
create_ax25_frame "N0CALL" "W1ABC" "03" "F0" "9600 baud data" "data_9600baud"

# Create summary
echo ""
echo "AX.25 Corpus created in: $CORPUS_DIR"
echo "Files created: $(ls -1 "$CORPUS_DIR" | wc -l)"
echo ""
echo "Corpus contents:"
ls -la "$CORPUS_DIR"
