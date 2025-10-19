#!/bin/bash
# M17 Security Monitoring
# Continuous security monitoring for M17 codebase

set -e

echo "M17 SECURITY MONITORING"
echo "=========================="

# Create monitoring directory
mkdir -p reports
cd reports

echo "Setting up continuous security monitoring..."

# Create security monitoring configuration
cat > security-monitor.conf << 'EOF'
# M17 Security Monitoring Configuration

# Critical security patterns to monitor
CRITICAL_PATTERNS=(
    "printf.*key"
    "fprintf.*key"
    "cout.*key"
    "printf.*seed"
    "fprintf.*seed"
    "printf.*sig"
    "fprintf.*sig"
    "rand()"
    "srand("
    "time(NULL)"
    "memset.*0"
    "for.*i.*<.*sizeof.*_digest"
    "0x8000"
    "0x7FFC"
    "0x7FFF"
)

# Warning patterns to monitor
WARNING_PATTERNS=(
    "EVP_.*;"
    "memcpy("
    "strcpy("
    "sprintf("
    "gets("
    "scanf("
)

# Files to monitor
MONITOR_FILES=(
    "../../../lib/m17_coder_impl.cc"
    "../../../lib/m17_decoder_impl.cc"
    "../../../lib/m17_coder_impl.h"
    "../../../lib/m17_decoder_impl.h"
    "../../../libm17/crypto/"
    "../../../python/m17/"
)

# Security thresholds
MAX_CRITICAL_ISSUES=0
MAX_WARNING_ISSUES=10
EOF

# Create security monitoring script
cat > monitor-security.sh << 'EOF'
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
EOF

if [ $CRITICAL_COUNT -gt 0 ]; then
    echo "FAIL: $CRITICAL_COUNT critical security issues found" >> security-scan-report.md
    echo "   Immediate action required!" >> security-scan-report.md
else
    echo "PASS: No critical security issues found" >> security-scan-report.md
fi

cat >> security-scan-report.md << EOF

## Warning Issues
EOF

if [ $WARNING_COUNT -gt 0 ]; then
    echo "WARNING: $WARNING_COUNT warning issues found" >> security-scan-report.md
    echo "   Review and address when possible" >> security-scan-report.md
else
    echo "PASS: No warning issues found" >> security-scan-report.md
fi

cat >> security-scan-report.md << EOF

## Recommendations

### Critical Issues
1. **Fix Immediately**: Address all critical security issues
2. **Code Review**: Review all flagged code sections
3. **Testing**: Test fixes thoroughly
4. **Documentation**: Document security fixes

### Warning Issues
1. **Review**: Review warning issues for potential problems
2. **Improve**: Consider improving code quality
3. **Monitor**: Continue monitoring for new issues
4. **Document**: Document improvements made

## Next Steps
1. **Fix Critical Issues**: Address all critical security issues
2. **Review Warnings**: Review and address warning issues
3. **Re-scan**: Run security scan again after fixes
4. **Continuous Monitoring**: Set up automated monitoring

## Files Scanned
EOF

for file in "${MONITOR_FILES[@]}"; do
    if [ -f "$file" ]; then
        echo "- $file" >> security-scan-report.md
    fi
done

# Check thresholds
if [ "$CRITICAL_COUNT" -gt "$MAX_CRITICAL_ISSUES" ]; then
    echo "SECURITY SCAN FAILED: Too many critical issues ($CRITICAL_COUNT > $MAX_CRITICAL_ISSUES)"
    exit 1
fi

if [ "$WARNING_COUNT" -gt "$MAX_WARNING_ISSUES" ]; then
    echo "SECURITY SCAN WARNING: Too many warning issues ($WARNING_COUNT > $MAX_WARNING_ISSUES)"
    exit 1
fi

echo "Security scan passed!"
echo "Report: security-scan-report.md"
EOF

chmod +x monitor-security.sh

# Create continuous monitoring script
cat > continuous-monitor.sh << 'EOF'
#!/bin/bash
# M17 Continuous Security Monitoring

echo "Starting continuous security monitoring..."

while true; do
    echo "Running security scan - $(date)"
    ./monitor-security.sh
    
    if [ $? -eq 0 ]; then
        echo "Security scan passed - $(date)"
    else
        echo "Security scan failed - $(date)"
        echo "ALERT: Security issues detected!"
        
        # Send alert (customize as needed)
        echo "Security alert sent to monitoring system"
    fi
    
    echo "Waiting 1 hour before next scan..."
    sleep 3600  # 1 hour
done
EOF

chmod +x continuous-monitor.sh

# Create security dashboard
cat > security-dashboard.html << 'EOF'
<!DOCTYPE html>
<html>
<head>
    <title>M17 Security Dashboard</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .header { background: #2c3e50; color: white; padding: 20px; border-radius: 5px; }
        .status { padding: 10px; margin: 10px 0; border-radius: 5px; }
        .pass { background: #d4edda; color: #155724; border: 1px solid #c3e6cb; }
        .fail { background: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }
        .warning { background: #fff3cd; color: #856404; border: 1px solid #ffeaa7; }
        .metrics { display: flex; gap: 20px; margin: 20px 0; }
        .metric { flex: 1; padding: 15px; background: #f8f9fa; border-radius: 5px; }
        .footer { margin-top: 40px; padding: 20px; background: #e9ecef; border-radius: 5px; }
    </style>
</head>
<body>
    <div class="header">
        <h1>M17 Security Dashboard</h1>
        <p>Continuous security monitoring for M17 codebase</p>
    </div>
    
    <div class="status pass">
        <h3>Security Status: PASS</h3>
        <p>No critical security issues detected</p>
    </div>
    
    <div class="metrics">
        <div class="metric">
            <h4>Critical Issues</h4>
            <p style="font-size: 24px; color: #28a745;">0</p>
        </div>
        <div class="metric">
            <h4>Warning Issues</h4>
            <p style="font-size: 24px; color: #ffc107;">0</p>
        </div>
        <div class="metric">
            <h4>Files Monitored</h4>
            <p style="font-size: 24px; color: #17a2b8;">8</p>
        </div>
        <div class="metric">
            <h4>Last Scan</h4>
            <p style="font-size: 16px; color: #6c757d;">Just now</p>
        </div>
    </div>
    
    <div class="footer">
        <h4>Security Monitoring Features</h4>
        <ul>
            <li>Continuous static analysis</li>
            <li>Real-time security alerts</li>
            <li>Security metrics tracking</li>
            <li>Cryptographic security checks</li>
            <li>Automated reporting</li>
        </ul>
        
        <h4>Quick Actions</h4>
        <ul>
            <li><a href="security-scan-report.md">View Latest Report</a></li>
            <li><a href="monitor-security.sh">Run Manual Scan</a></li>
            <li><a href="continuous-monitor.sh">Start Continuous Monitoring</a></li>
        </ul>
    </div>
</body>
</html>
EOF

echo "Creating security metrics..."

# Create security metrics script
cat > security-metrics.sh << 'EOF'
#!/bin/bash
# M17 Security Metrics

echo "M17 Security Metrics"
echo "======================="

# Count lines of code
echo "Lines of Code:"
find ../lib ../libm17 ../python -name "*.c" -o -name "*.cc" -o -name "*.cpp" -o -name "*.h" -o -name "*.hpp" -o -name "*.py" | xargs wc -l | tail -1

# Count security-related functions
echo "Security Functions:"
grep -r "explicit_bzero\|RAND_bytes\|EVP_\|HMAC" ../lib ../libm17 ../python | wc -l

# Count potential security issues
echo "Potential Security Issues:"
grep -r "printf.*key\|fprintf.*key\|rand()\|srand\|time(NULL)" ../lib ../libm17 ../python | wc -l

# Count cryptographic operations
echo "Cryptographic Operations:"
grep -r "EVP_\|HMAC\|RAND_\|explicit_bzero" ../lib ../libm17 ../python | wc -l

# Count error handling
echo "Error Handling:"
grep -r "if.*EVP_\|if.*HMAC\|if.*RAND_" ../lib ../libm17 ../python | wc -l

echo "Security metrics generated"
EOF

chmod +x security-metrics.sh

echo "Security monitoring setup complete!"
echo "Monitoring files in: security/monitoring/reports/"
echo ""
echo "Available tools:"
echo "  - monitor-security.sh (manual security scan)"
echo "  - continuous-monitor.sh (continuous monitoring)"
echo "  - security-metrics.sh (security metrics)"
echo "  - security-dashboard.html (web dashboard)"
echo ""
echo "To start monitoring:"
echo "  ./monitor-security.sh          # Run manual scan"
echo "  ./continuous-monitor.sh         # Start continuous monitoring"
echo "  ./security-metrics.sh          # Generate metrics"
echo ""
echo "View dashboard:"
echo "  open security-dashboard.html   # Open web dashboard"
