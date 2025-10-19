# M17 Fuzzing Framework

## Overview

The M17 Fuzzing Framework provides comprehensive fuzz testing for the M17 digital voice protocol implementation. It uses AFL++ (American Fuzzy Lop++) to discover security vulnerabilities, crashes, and unexpected behavior.

## Features

### Fuzzing Modes

| Mode | Duration | Use Case |
|------|----------|----------|
| **ultra-quick** | 10 minutes | Quick testing, CI/CD pipelines |
| **quick** | 1 hour | Development testing |
| **overnight** | 8 hours | Thorough testing |
| **thorough** | 24 hours | Security assessment |
| **weekend** | 72 hours | Comprehensive analysis |
| **continuous** | Until stopped | Long-term monitoring |

### Security Instrumentation

- **AFL++**: Advanced fuzzing engine with coverage-guided testing
- **AddressSanitizer**: Detects buffer overflows, use-after-free, memory leaks
- **UndefinedBehaviorSanitizer**: Detects undefined behavior, integer overflows
- **Memory Sanitizer**: Detects uninitialized memory usage

### Fuzz Targets

#### 1. M17 Decoder Fuzzing
- **Purpose**: Test M17 frame decoding, syncword detection, LSF parsing
- **Binary**: `m17_decoder_fuzz`
- **Features**:
 - Frame structure validation
 - Syncword detection testing
 - LSF (Link Setup Frame) parsing
 - Payload decoding
 - CRC validation
 - Callsign validation

#### 2. M17 Cryptographic Fuzzing
- **Purpose**: Test cryptographic operations with fuzzed inputs
- **Binary**: `m17_crypto_fuzz`
- **Features**:
 - AES encryption/decryption testing
 - SHA-256 hashing with random inputs
 - Signature operation testing
 - Scrambler LFSR testing
 - OpenSSL integration testing

## Usage

### Basic Usage

```bash
# Run ultra-quick fuzzing (10 minutes)
./security/fuzzing/fuzz-testing-improved.sh ultra-quick

# Run quick fuzzing (1 hour)
./security/fuzzing/fuzz-testing-improved.sh quick

# Run overnight fuzzing (8 hours)
./security/fuzzing/fuzz-testing-improved.sh overnight

# Run thorough fuzzing (24 hours)
./security/fuzzing/fuzz-testing-improved.sh thorough

# Run continuous fuzzing (until stopped)
./security/fuzzing/fuzz-testing-improved.sh continuous
```

### Using the Security Audit Script

```bash
# Run fuzzing through security audit script
./security-audit fuzz

# This runs ultra-quick mode by default
```

## Prerequisites

### Required Software

```bash
# Install AFL++
sudo apt install afl++

# Or build from source
git clone https://github.com/AFLplusplus/AFLplusplus.git
cd AFLplusplus
make
sudo make install
```

### Required Libraries

```bash
# OpenSSL for cryptographic fuzzing
sudo apt install libssl-dev

# Build tools
sudo apt install build-essential
```

## Output and Results

### Directory Structure

```
security/fuzzing/reports/YYYYMMDD_HHMMSS/
 m17_decoder_fuzz # Decoder fuzzing binary
 m17_crypto_fuzz # Crypto fuzzing binary
 testcases/ # Input corpus
 valid_sync # Valid M17 syncword
 lsf_frame # LSF frame structure
 payload1 # Payload frame 1
 payload2 # Payload frame 2
 crypto_key # Cryptographic key
 crypto_iv # Initialization vector
 crypto_combined # Combined key+IV
 empty # Empty input
 single_byte # Single byte input
 zeros # Zero-filled input
 random_large # Large random input
 callsign1 # Callsign test case 1
 callsign2 # Callsign test case 2
 broadcast # Broadcast callsign
 findings_decoder/ # Decoder fuzzing results
 default/
 crashes/ # Crash-inducing inputs
 hangs/ # Hang-inducing inputs
 fuzzer_stats # Fuzzing statistics
 findings_crypto/ # Crypto fuzzing results
 default/
 crashes/ # Crash-inducing inputs
 hangs/ # Hang-inducing inputs
 fuzzer_stats # Fuzzing statistics
 FUZZING_REPORT.md # Detailed report
```

### Analyzing Results

#### Check for Crashes

```bash
# Count crashes
find findings_*/default/crashes -type f ! -name "README.txt" | wc -l

# List crash files
ls -la findings_*/default/crashes/

# Analyze crash input
xxd findings_decoder/default/crashes/id:000000*
```

#### Check for Hangs

```bash
# Count hangs
find findings_*/default/hangs -type f ! -name "README.txt" | wc -l

# List hang files
ls -la findings_*/default/hangs/
```

#### Monitor Progress

```bash
# Check fuzzing statistics
afl-whatsup findings_decoder/
afl-whatsup findings_crypto/

# View detailed stats
cat findings_decoder/default/fuzzer_stats
cat findings_crypto/default/fuzzer_stats
```

## Reproducing Issues

### Reproduce Crashes

```bash
# Reproduce a specific crash
./m17_decoder_fuzz < findings_decoder/default/crashes/id:000000*

# Debug with GDB
gdb ./m17_decoder_fuzz
(gdb) run < findings_decoder/default/crashes/id:000000*
```

### Debug with Valgrind

```bash
# Check for memory issues
valgrind --tool=memcheck ./m17_decoder_fuzz < crash_input

# Check for undefined behavior
valgrind --tool=exp-sgcheck ./m17_decoder_fuzz < crash_input
```

## Integration with CI/CD

### GitHub Actions Example

```yaml
name: Fuzzing
on: [push, pull_request]

jobs:
 fuzz:
 runs-on: ubuntu-latest
 steps:
 - uses: actions/checkout@v2
 - name: Install AFL++
 run: sudo apt install afl++
 - name: Run ultra-quick fuzzing
 run: ./security/fuzzing/fuzz-testing-improved.sh ultra-quick
 - name: Upload results
 uses: actions/upload-artifact@v2
 with:
 name: fuzzing-results
 path: security/fuzzing/reports/
```

### Jenkins Pipeline Example

```groovy
pipeline {
 agent any
 stages {
 stage('Fuzz Test') {
 steps {
 sh './security/fuzzing/fuzz-testing-improved.sh ultra-quick'
 }
 }
 stage('Analyze Results') {
 steps {
 sh 'find security/fuzzing/reports/ -name "crashes" -type d | wc -l'
 }
 }
 }
 post {
 always {
 archiveArtifacts artifacts: 'security/fuzzing/reports/**/*'
 }
 }
}
```

## Best Practices

### Fuzzing Strategy

1. **Start with ultra-quick mode** for development
2. **Use overnight mode** for regular testing
3. **Run thorough mode** before releases
4. **Use continuous mode** for long-term monitoring

### Crash Analysis

1. **Reproduce crashes** with minimal inputs
2. **Debug with GDB** to understand root cause
3. **Fix the issue** and add regression tests
4. **Re-fuzz** to ensure fix is complete

### Performance Optimization

1. **Use multiple cores** for parallel fuzzing
2. **Optimize test cases** for better coverage
3. **Monitor system resources** during long runs
4. **Use persistent mode** for faster execution

## Troubleshooting

### Common Issues

#### AFL++ Not Found
```bash
# Install AFL++
sudo apt install afl++

# Or build from source
git clone https://github.com/AFLplusplus/AFLplusplus.git
cd AFLplusplus
make
sudo make install
```

#### Compilation Errors
```bash
# Install required libraries
sudo apt install libssl-dev build-essential

# Check OpenSSL installation
pkg-config --libs openssl
```

#### Permission Denied
```bash
# Make script executable
chmod +x security/fuzzing/fuzz-testing-improved.sh
```

#### Out of Memory
```bash
# Reduce memory usage
export AFL_MAP_SIZE=65536

# Use smaller test cases
# Edit the script to reduce test case sizes
```

## Security Considerations

### Fuzzing Security

- **Never fuzz production systems**
- **Use isolated environments** for fuzzing
- **Monitor system resources** during long runs
- **Clean up fuzzing artifacts** after testing

### Crash Analysis Security

- **Analyze crash inputs carefully**
- **Don't execute crash inputs** on production systems
- **Use sandboxed environments** for analysis
- **Report security issues** responsibly

## Contributing

### Adding New Fuzz Targets

1. Create a new C++ file with `LLVMFuzzerTestOneInput` function
2. Add compilation to the script
3. Create appropriate test cases
4. Update documentation

### Improving Test Cases

1. Analyze existing test cases
2. Add edge cases and boundary conditions
3. Include real-world M17 data
4. Test with malformed inputs

## References

- [AFL++ Documentation](https://github.com/AFLplusplus/AFLplusplus)
- [Fuzzing Best Practices](https://github.com/google/fuzzing)
- [M17 Protocol Specification](https://github.com/M17-Project/M17_spec)
- [OpenSSL Documentation](https://www.openssl.org/docs/)

---

**Last Updated**: $(date) 
**Version**: 2.0 
**Status**: Active

