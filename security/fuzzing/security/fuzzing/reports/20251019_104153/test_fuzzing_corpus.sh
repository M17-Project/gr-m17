#!/bin/bash
# M17 Fuzzing Corpus Regression Test
# Tests the fuzzing corpus to ensure all test cases pass

PASSED=0
FAILED=0
TOTAL=0

echo "M17 Fuzzing Corpus Regression Test"
echo "=================================="
echo "Testing fuzzing corpus for stability..."
echo

# Test decoder fuzzing corpus
echo "Testing M17 Decoder Fuzzing Corpus:"
for testcase in regression_tests/fuzzing/*; do
    if [ -f "$testcase" ]; then
        ((TOTAL++))
        if timeout 5 ./m17_decoder_fuzz < "$testcase" >/dev/null 2>&1; then
            ((PASSED++))
            echo "PASS: $(basename "$testcase")"
        else
            echo "FAIL: $(basename "$testcase")"
            ((FAILED++))
        fi
    fi
done

echo
echo "Regression Test Results:"
echo "======================="
echo "Total test cases: $TOTAL"
echo "Passed: $PASSED"
echo "Failed: $FAILED"
echo

if [ $FAILED -eq 0 ]; then
    echo "SUCCESS: All fuzzing corpus tests PASSED"
    echo "SUCCESS: M17 codebase is stable with fuzzing corpus"
    exit 0
else
    echo "ERROR: $FAILED test cases FAILED"
    echo "ERROR: M17 codebase may have regressions"
    exit 1
fi
