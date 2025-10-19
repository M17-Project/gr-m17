#!/bin/bash

# Security Code Review Script for PIN Authentication
# This script performs comprehensive security analysis of the codebase

set -e

echo "=== M17 PIN Authentication Security Code Review ==="
echo "Date: $(date)"
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    local status=$1
    local message=$2
    case $status in
        "PASS")
            echo -e "${GREEN}[PASS]${NC} $message"
            ;;
        "FAIL")
            echo -e "${RED}[FAIL]${NC} $message"
            ;;
        "WARN")
            echo -e "${YELLOW}[WARN]${NC} $message"
            ;;
        "INFO")
            echo -e "${BLUE}[INFO]${NC} $message"
            ;;
    esac
}

# Function to check for security issues
check_security_issue() {
    local pattern=$1
    local description=$2
    local severity=$3
    local file=$4
    
    if grep -n "$pattern" "$file" > /dev/null 2>&1; then
        print_status "FAIL" "$description (Severity: $severity)"
        echo "  Found in: $file"
        grep -n "$pattern" "$file" | head -5
        echo ""
        return 1
    else
        print_status "PASS" "$description"
        return 0
    fi
}

# Function to check for security improvements
check_security_improvement() {
    local pattern=$1
    local description=$2
    local file=$3
    
    if grep -n "$pattern" "$file" > /dev/null 2>&1; then
        print_status "PASS" "$description"
        return 0
    else
        print_status "WARN" "$description"
        return 1
    fi
}

echo "1. Checking for Command Injection Vulnerabilities..."
echo "=================================================="

# Check for system() calls (actual function calls, not comments)
check_security_issue "system\(.*\);" "system() function calls found" "CRITICAL" "lib/m17_coder_impl.cc"

# Check for execve() usage (good)
check_security_improvement "execve(" "execve() usage found" "lib/m17_coder_impl.cc"

# Check for secure_execute_command usage (good)
check_security_improvement "secure_execute_command" "secure command execution found" "lib/m17_coder_impl.cc"

echo ""
echo "2. Checking for Input Validation..."
echo "=================================="

# Check for input validation functions
check_security_improvement "validate_nitrokey_label" "Input validation function found" "lib/m17_coder_impl.cc"
check_security_improvement "sanitize_shell_input" "Input sanitization function found" "lib/m17_coder_impl.cc"

# Check for dangerous shell characters in validation
check_security_improvement "dangerous_chars" "Shell character validation found" "lib/m17_coder_impl.cc"

echo ""
echo "3. Checking for Buffer Overflow Vulnerabilities..."
echo "==============================================="

# Check for unsafe string operations
check_security_issue "strcpy(" "strcpy() usage found" "HIGH" "lib/m17_coder_impl.cc"
check_security_issue "strcat(" "strcat() usage found" "HIGH" "lib/m17_coder_impl.cc"
check_security_issue "sprintf(" "sprintf() usage found" "MEDIUM" "lib/m17_coder_impl.cc"

# Check for safe string operations
check_security_improvement "strncpy(" "strncpy() usage found" "lib/m17_coder_impl.cc"
check_security_improvement "snprintf(" "snprintf() usage found" "lib/m17_coder_impl.cc"

echo ""
echo "4. Checking for Memory Safety Issues..."
echo "======================================"

# Check for explicit_bzero usage
check_security_improvement "explicit_bzero" "Secure memory clearing found" "lib/m17_coder_impl.cc"

# Check for memset usage (less secure)
check_security_issue "memset.*0" "memset() usage found" "MEDIUM" "lib/m17_coder_impl.cc"

echo ""
echo "5. Checking for Information Disclosure..."
echo "======================================"

# Check for sensitive data in error messages
check_security_issue "fprintf.*stderr.*%d" "Exit codes in error messages" "MEDIUM" "lib/m17_coder_impl.cc"
check_security_issue "fprintf.*stderr.*%s" "String data in error messages" "MEDIUM" "lib/m17_coder_impl.cc"

# Check for generic error messages
check_security_improvement "ERROR: Generic" "Generic error messages found" "lib/m17_coder_impl.cc"

echo ""
echo "6. Checking for Cryptographic Security..."
echo "======================================"

# Check for key logging
check_security_issue "printf.*key" "Potential key logging" "CRITICAL" "lib/m17_coder_impl.cc"
check_security_issue "fprintf.*key" "Potential key logging" "CRITICAL" "lib/m17_coder_impl.cc"

# Check for secure random number generation
check_security_improvement "RAND_bytes" "Secure random number generation found" "lib/m17_coder_impl.cc"
check_security_issue "rand(" "Insecure random number generation" "HIGH" "lib/m17_coder_impl.cc"

echo ""
echo "7. Checking for PIN Authentication Security..."
echo "==========================================="

# Check for PIN authentication functions
check_security_improvement "check_nitrokey_pin_status" "PIN status detection found" "lib/m17_coder_impl.cc"
check_security_improvement "attempt_nitrokey_pin_authentication" "PIN authentication attempt found" "lib/m17_coder_impl.cc"
check_security_improvement "handle_nitrokey_pin_authentication" "PIN authentication handling found" "lib/m17_coder_impl.cc"

# Check for PIN protection
check_security_issue "PIN" "PIN references found" "INFO" "lib/m17_coder_impl.cc"

echo ""
echo "8. Checking for Error Handling..."
echo "==============================="

# Check for proper error handling
check_security_improvement "if.*result.*!=.*0" "Error handling found" "lib/m17_coder_impl.cc"
check_security_improvement "return false" "Error return handling found" "lib/m17_coder_impl.cc"

echo ""
echo "9. Checking for Security Headers..."
echo "=================================="

# Check for security includes
check_security_improvement "#include.*unistd.h" "Unix system calls included" "lib/m17_coder_impl.cc"
check_security_improvement "#include.*sys/wait.h" "Process waiting included" "lib/m17_coder_impl.cc"
check_security_improvement "#include.*cctype" "Character type checking included" "lib/m17_coder_impl.cc"

echo ""
echo "10. Checking for Security Comments..."
echo "==================================="

# Check for security comments
check_security_improvement "SECURITY FIX" "Security fixes documented" "lib/m17_coder_impl.cc"
check_security_improvement "// SECURITY" "Security comments found" "lib/m17_coder_impl.cc"

echo ""
echo "=== Security Code Review Summary ==="
echo "==================================="

# Count security issues
system_calls=$(grep -c "system(" lib/m17_coder_impl.cc || echo "0")
execve_calls=$(grep -c "execve(" lib/m17_coder_impl.cc || echo "0")
security_fixes=$(grep -c "SECURITY FIX" lib/m17_coder_impl.cc || echo "0")
validation_functions=$(grep -c "validate_nitrokey_label\|sanitize_shell_input" lib/m17_coder_impl.cc || echo "0")

echo "System calls found: $system_calls"
echo "Execve calls found: $execve_calls"
echo "Security fixes found: $security_fixes"
echo "Validation functions found: $validation_functions"

if [ "$system_calls" -gt 0 ]; then
    print_status "WARN" "System calls still present - consider replacing with execve()"
else
    print_status "PASS" "No system calls found"
fi

if [ "$execve_calls" -gt 0 ]; then
    print_status "PASS" "Execve calls found - good security practice"
else
    print_status "WARN" "No execve calls found - consider implementing secure execution"
fi

if [ "$security_fixes" -gt 0 ]; then
    print_status "PASS" "Security fixes implemented"
else
    print_status "WARN" "No security fixes documented"
fi

if [ "$validation_functions" -gt 0 ]; then
    print_status "PASS" "Input validation functions found"
else
    print_status "FAIL" "No input validation functions found"
fi

echo ""
echo "=== Security Recommendations ==="
echo "============================="

if [ "$system_calls" -gt 0 ]; then
    echo "1. Replace remaining system() calls with execve() for better security"
fi

if [ "$execve_calls" -eq 0 ]; then
    echo "2. Implement execve() based command execution"
fi

echo "3. Add comprehensive security testing"
echo "4. Implement continuous security monitoring"
echo "5. Consider using native Nitrokey API instead of CLI calls"

echo ""
echo "Security Code Review Complete!"
echo "============================="
