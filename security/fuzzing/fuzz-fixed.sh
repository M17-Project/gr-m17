#!/bin/bash
# Fixed optimized AFL++ fuzzing

set -e

echo "FIXED OPTIMIZED AFL++ FUZZING"
echo "============================="

# Create fuzzing directory
FUZZ_DIR="security/fuzzing/reports/$(date +%Y%m%d_%H%M%S)"
mkdir -p "$FUZZ_DIR"
cd "$FUZZ_DIR"

echo "Working directory: $FUZZ_DIR"

# Create FIXED fast fuzz target
cat > fast_fuzz.cpp << "EOL"
#include <cstdint>
#include <cstring>
#include <cstdio>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 1 || size > 1000) return 0;
    uint8_t buffer[256];
    size_t copy_size = (size < sizeof(buffer)) ? size : sizeof(buffer);
    memcpy(buffer, data, copy_size);
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) return 1;
    FILE* f = fopen(argv[1], "rb");
    if (!f) return 1;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size > 1000) { fclose(f); return 1; }
    uint8_t* data = new uint8_t[size];
    fread(data, 1, size, f);
    fclose(f);
    int result = LLVMFuzzerTestOneInput(data, size);
    delete[] data;
    return result;
}
EOL

# Compile with speed optimizations
echo "Compiling FIXED fast fuzz target..."
AFL_FAST_CAL=1 afl-g++ -O2 -funroll-loops -fomit-frame-pointer -o fast_fuzz fast_fuzz.cpp

# Create test cases
mkdir -p testcases
printf "test" > testcases/simple
dd if=/dev/urandom of=testcases/random bs=32 count=1 2>/dev/null

echo "Starting FIXED fast fuzzing with optimized settings..."

# OPTIMIZED AFL++ SETTINGS for maximum speed
AFL_SKIP_CPUFREQ=1 \
AFL_FAST_CAL=1 \
AFL_NO_AFFINITY=1 \
timeout 3600 afl-fuzz \
    -i testcases \
    -o findings_fast \
    -t 1 \
    -m none \
    -x /dev/null \
    ./fast_fuzz @@ &

echo "FIXED fast fuzzing started with PID: $!"
echo "Monitor execution rate with: watch -n 1 \"cat findings_fast/default/fuzzer_stats | grep execs_per_sec\""
