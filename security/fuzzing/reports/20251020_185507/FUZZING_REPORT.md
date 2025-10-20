# M17 Fuzzing Campaign Report

**Date**: ma. 20. okt. 19:11:54 +0200 2025
**Duration**: 28800s (480m)
**Mode**: overnight

## Summary

This fuzzing campaign tested the M17 codebase for security vulnerabilities,
crashes, and unexpected behavior.

## Targets Tested

### 1. M17 Decoder Fuzzing
- **Binary**: m17_decoder_fuzz
- **Purpose**: Test M17 frame decoding, syncword detection, LSF parsing
- **Instrumentation**: AFL++, AddressSanitizer, UndefinedBehaviorSanitizer
- **Results**: See findings_decoder/

### 2. M17 Cryptographic Fuzzing
- **Binary**: m17_crypto_fuzz
- **Purpose**: Test AES, SHA-256, signatures, scrambler
- **Instrumentation**: AFL++, AddressSanitizer, UndefinedBehaviorSanitizer
- **Results**: See findings_crypto/

## Findings

### Critical Issues (Crashes)
0 crash(es) found

### Performance Issues (Hangs)
0 hang(s) found

### Coverage Analysis
- Decoder paths explored: 
- Crypto paths explored: 

## Detailed Results

### Decoder Fuzzing
```
start_time        : 1760979309
last_update       : 1760980314
run_time          : 1004
fuzzer_pid        : 1893965
cycles_done       : 5
cycles_wo_finds   : 0
time_wo_finds     : 539
execs_done        : 134382
execs_per_sec     : 133.81
execs_ps_last_min : 134.85
corpus_count      : 32
corpus_favored    : 21
corpus_found      : 19
corpus_imported   : 0
corpus_variable   : 0
max_depth         : 3
cur_item          : 2
pending_favs      : 0
pending_total     : 5
stability         : 100.00%
bitmap_cvg        : 0.00%
saved_crashes     : 0
saved_hangs       : 0
last_find         : 1760980138
last_crash        : 0
last_hang         : 0
execs_since_crash : 134382
exec_timeout      : 40
slowest_exec_ms   : 0
peak_rss_mb       : 6
cpu_affinity      : 0
edges_found       : 97
total_edges       : 8388608
var_byte_count    : 0
havoc_expansion   : 2
auto_dict_entries : 0
testcache_size    : 20466
testcache_count   : 32
testcache_evict   : 0
afl_banner        : ./m17_decoder_fuzz
afl_version       : ++4.09c
target_mode       : shmem_testcase default
command_line      : afl-fuzz -i testcases -o findings_decoder -m none ./m17_decoder_fuzz @@
```

### Crypto Fuzzing
```
start_time        : 1760979310
last_update       : 1760980314
run_time          : 1004
fuzzer_pid        : 1893966
cycles_done       : 5
cycles_wo_finds   : 1
time_wo_finds     : 596
execs_done        : 75144
execs_per_sec     : 74.84
execs_ps_last_min : 75.81
corpus_count      : 20
corpus_favored    : 5
corpus_found      : 7
corpus_imported   : 0
corpus_variable   : 0
max_depth         : 2
cur_item          : 12
pending_favs      : 0
pending_total     : 7
stability         : 100.00%
bitmap_cvg        : 0.00%
saved_crashes     : 0
saved_hangs       : 0
last_find         : 1760979915
last_crash        : 0
last_hang         : 0
execs_since_crash : 75144
exec_timeout      : 40
slowest_exec_ms   : 0
peak_rss_mb       : 8
cpu_affinity      : 0
edges_found       : 45
total_edges       : 8388608
var_byte_count    : 0
havoc_expansion   : 3
auto_dict_entries : 0
testcache_size    : 16247
testcache_count   : 20
testcache_evict   : 0
afl_banner        : ./m17_crypto_fuzz
afl_version       : ++4.09c
target_mode       : shmem_testcase default
command_line      : afl-fuzz -i testcases -o findings_crypto -m none ./m17_crypto_fuzz @@
```

## Recommendations

1. **Address all crashes immediately** - These are potential security vulnerabilities
2. **Investigate hangs** - May indicate infinite loops or resource exhaustion
3. **Improve input validation** - Add bounds checking and error handling
4. **Increase fuzzing duration** - Run for at least 24-72 hours for thorough testing
5. **Continuous fuzzing** - Set up automated fuzzing in CI/CD pipeline

## Action Items

- [ ] Fix all crash-inducing inputs
- [ ] Fix all hang-inducing inputs
- [ ] Add test cases for found issues
- [ ] Re-fuzz after fixes
- [ ] Document security findings

## Files Generated

- `m17_decoder_fuzz`: Decoder fuzzing binary
- `m17_crypto_fuzz`: Crypto fuzzing binary
- `findings_decoder/`: Decoder fuzzing results
- `findings_crypto/`: Crypto fuzzing results
- `testcases/`: Input corpus

## Next Steps

1. Analyze crash inputs: `xxd findings_*/default/crashes/id:*`
2. Reproduce crashes: `./m17_*_fuzz < findings_*/default/crashes/id:*`
3. Debug with GDB: `gdb ./m17_*_fuzz` then `run < crash_input`
4. Fix root causes
5. Add regression tests
6. Re-run fuzzing campaign

