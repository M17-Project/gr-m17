#!/bin/bash
# Optimized AFL++ fuzzing with proper settings for high execution rates

set -e

echo "OPTIMIZED AFL++ FUZZING"
echo "======================"

# Parse arguments
FUZZ_MODE=${1:-quick}
TIMEOUT=${2:-21600}  # 6 hours default

case $FUZZ_MODE in
    quick) TIMEOUT=3600;;
    6hour) TIMEOUT=21600;;
    overnight) TIMEOUT=28800;;
    *) echo "Usage: $0 {quick|6hour|overnight}"; exit 1;;
esac

# Create optimized fuzzing directory
FUZZ_DIR="security/fuzzing/reports/$(date +%Y%m%d_%H%M%S)"
mkdir -p "$FUZZ_DIR"
cd "$FUZZ_DIR"

echo "Setting up optimized AFL++ fuzzing..."
echo "Working directory: $FUZZ_DIR"

# Check AFL++ installation
if ! command -v afl-g++ &> /dev/null; then
    echo "ERROR: AFL++ not found. Please install AFL++ first."
    exit 1
fi

# OPTIMIZED COMPILE FLAGS for speed
COMPILE_FLAGS="-O2 -funroll-loops -fomit-frame-pointer"

echo "Creating optimized fuzz targets..."

# Create optimized fuzz targets (same as before but with better compilation)
cat > m17_decoder_fuzz.cpp << "EOL"
#include <iostream>
#include <cstring>
#include <cstdint>
#include <cstdlib>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 1 || size > 10000) return 0;
    
    // Fast M17 frame processing
    if (size >= 8) {
        // Check syncword
        uint8_t sync[8] = {0x55, 0xF7, 0x7F, 0xD7, 0x7F, 0x55, 0xF7, 0x7F};
        bool match = true;
        for (int i = 0; i < 8; i++) {
            if (data[i] != sync[i]) { match = false; break; }
        }
    }
    
    // Fast payload processing
    if (size >= 16) {
        uint8_t buffer[1024];
        size_t copy_size = (size < sizeof(buffer)) ? size : sizeof(buffer);
        memcpy(buffer, data, copy_size);
    }
    
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) return 1;
    FILE* f = fopen(argv[1], "rb");
    if (!f) return 1;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size > 10000) { fclose(f); return 1; }
    uint8_t* data = new uint8_t[size];
    fread(data, 1, size, f);
    fclose(f);
    int result = LLVMFuzzerTestOneInput(data, size);
    delete[] data;
    return result;
}
EOL

# Compile with optimized settings
echo "Compiling optimized fuzz targets..."
AFL_FAST_CAL=1 afl-g++ -O2 -funroll-loops -fomit-frame-pointer -o m17_decoder_fuzz m17_decoder_fuzz.cpp

# Create test cases
mkdir -p testcases
printf "\\x55\\xF7\\x7F\\xD7\\x7F\\x55\\xF7\\x7F" > testcases/valid_sync
dd if=/dev/urandom of=testcases/random bs=64 count=1 2>/dev/null
echo "test" > testcases/text

echo "Starting optimized fuzzing..."
echo "Duration: $TIMEOUT seconds"

# OPTIMIZED AFL++ SETTINGS
AFL_SKIP_CPUFREQ=1 \
AFL_FAST_CAL=1 \
AFL_NO_AFFINITY=1 \
timeout $TIMEOUT afl-fuzz \
    -i testcases \
    -o findings_decoder \
    -t 5 \
    -m none \
    -x /dev/null \
    ./m17_decoder_fuzz @@ &

echo "Optimized fuzzing started with PID: $!"
echo "Monitor with: watch -n 1 cat
