#!/bin/bash
# M17 Security Monitoring Script

source security-monitor.conf

echo "M17 Security Scan - $(date)"
echo "================================"

CRITICAL_COUNT=0
WARNING_COUNT=0

# Check for critical security issues
echo "Checking for critical security issues..."
for pattern in "${CRITICAL_PATTERNS[@]}"; do
    for file in "${MONITOR_FILES[@]}"; do
        if [ -f "$file" ]; then
            if grep -n "$pattern" "$file" >/dev/null 2>&1; then
                echo "CRITICAL: Found '$pattern' in $file"
                grep -n "$pattern" "$file"
                CRITICAL_COUNT=$((CRITICAL_COUNT + 1))
            fi
        fi
    done
done

# Check for warning issues
echo "Checking for warning issues..."
for pattern in "${WARNING_PATTERNS[@]}"; do
    for file in "${MONITOR_FILES[@]}"; do
        if [ -f "$file" ]; then
            if grep -n "$pattern" "$file" >/dev/null 2>&1; then
                echo "WARNING: Found '$pattern' in $file"
                grep -n "$pattern" "$file"
                WARNING_COUNT=$((WARNING_COUNT + 1))
            fi
        fi
    done
done

# Generate security report
cat > security-scan-report.md << EOF
# M17 Security Scan Report
**Date**: $(date)
**Scanner**: M17 Security Monitor

## Summary
- **Critical Issues**: $CRITICAL_COUNT
- **Warning Issues**: $WARNING_COUNT
- **Status**: $([ $CRITICAL_COUNT -eq 0 ] && echo "PASS" || echo "FAIL")

## Critical Issues
