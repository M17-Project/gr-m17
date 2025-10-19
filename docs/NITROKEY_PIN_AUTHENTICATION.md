# Enhanced Nitrokey PIN Authentication

## Overview

The M17 GNU Radio module now includes enhanced PIN authentication handling for Nitrokey devices. This provides better user experience, clearer error messages, and automatic PIN authentication management.

## Key Features

### Enhanced PIN Status Detection
- **Automatic Detection**: System automatically detects when PIN authentication is required
- **Status Classification**: Distinguishes between device not found, PIN required, and authenticated states
- **Real-time Monitoring**: Continuous monitoring of Nitrokey authentication status

### User Guidance and Error Handling
- **Clear Messages**: Detailed status messages explaining what action is needed
- **Error Classification**: Distinguishes between different types of errors
- **Action Guidance**: Specific instructions for resolving issues

### Session Management
- **Automatic Authentication**: Handles PIN authentication automatically when possible
- **Session Persistence**: Maintains authentication state during operations
- **Recovery Mechanisms**: Automatic recovery when device is reconnected

## API Functions

### PIN Status Detection

```cpp
// Check detailed PIN authentication status
NitrokeyStatus check_nitrokey_pin_status();

// Attempt PIN authentication
bool attempt_nitrokey_pin_authentication();

// Report comprehensive status
void report_nitrokey_status();

// Handle PIN authentication automatically
bool handle_nitrokey_pin_authentication();
```

### Status Enumeration

```cpp
enum class NitrokeyStatus {
    DEVICE_NOT_FOUND,    // Device not connected
    PIN_REQUIRED,        // Device connected but PIN authentication needed
    AUTHENTICATED,       // Device connected and PIN authenticated
    ERROR               // Other error condition
};
```

## Usage Examples

### Basic PIN Authentication

```cpp
#include <gnuradio/m17/m17_coder.h>

// Initialize M17 coder
auto coder = gr::m17::m17_coder::make(...);

// Check Nitrokey status with enhanced error handling
if (coder->check_nitrokey_status()) {
    std::cout << "Nitrokey is ready" << std::endl;
} else {
    // System will provide detailed error information
    coder->report_nitrokey_status();
}
```

### Automatic PIN Handling

```cpp
// Sign data with automatic PIN authentication
const char* data = "Hello, M17!";
uint8_t signature[64];

if (coder->sign_with_nitrokey((const uint8_t*)data, strlen(data), signature)) {
    std::cout << "Data signed successfully" << std::endl;
} else {
    // System will provide detailed error information
    std::cout << "Signing failed - check Nitrokey status" << std::endl;
}
```

### Manual PIN Authentication

```cpp
// Manually handle PIN authentication
if (coder->attempt_nitrokey_pin_authentication()) {
    std::cout << "PIN authentication successful" << std::endl;
} else {
    std::cout << "PIN authentication failed" << std::endl;
    std::cout << "Please check your PIN and try again" << std::endl;
}
```

## Status Messages

### Device Not Found
```
STATUS: Nitrokey device not connected
ACTION: Please connect your Nitrokey device
```

### PIN Required
```
STATUS: Nitrokey connected but PIN authentication required
ACTION: Please enter your PIN when prompted by nitropy
```

### Authenticated
```
STATUS: Nitrokey connected and authenticated with key 'M17-Key'
```

### Error Condition
```
STATUS: Nitrokey error condition
ACTION: Please check device connection and try again
```

## Error Handling

### Enhanced Error Classification

The system now distinguishes between different error conditions:

1. **Device Not Found**: Nitrokey not connected
2. **PIN Required**: Device connected but authentication needed
3. **Authentication Failed**: PIN entered incorrectly
4. **Key Not Found**: Specified key doesn't exist on device
5. **Permission Denied**: Insufficient permissions for device access

### Automatic Recovery

The system includes automatic recovery mechanisms:

- **Device Reconnection**: Automatically detects when device is reconnected
- **PIN Authentication**: Handles PIN authentication automatically when possible
- **Session Restoration**: Restores authentication state after device reconnection

## Security Considerations

### PIN Security
- **PIN Protection**: PINs are never stored or logged
- **Secure Input**: PIN entry is handled by nitropy securely
- **Session Management**: PIN sessions are properly managed
- **Timeout Handling**: Automatic timeout for PIN entry

### Device Security
- **Hardware Isolation**: Private keys remain on Nitrokey device
- **Tamper Resistance**: Hardware-level security features
- **Authentication Required**: All operations require PIN authentication
- **Session Termination**: Authentication terminates on device removal

## Troubleshooting

### Common Issues

#### PIN Authentication Timeout
```
ERROR: Nitrokey PIN authentication timed out
Please ensure you enter your PIN within 30 seconds
```

**Solution**: Enter PIN more quickly or increase timeout

#### Device Not Detected
```
STATUS: Nitrokey device not connected
ACTION: Please connect your Nitrokey device
```

**Solution**: Check USB connection and device status

#### Key Not Found
```
ERROR: Credential 'M17-Key' not found on Nitrokey
Please check that the key exists and your PIN is correct
```

**Solution**: Verify key exists using `list_nitrokey_keys()`

### Debug Information

Enable debug output for detailed information:

```cpp
// Enable debug mode
coder->set_debug(true);

// Check detailed status
coder->report_nitrokey_status();
```

## Performance Characteristics

### PIN Authentication
- **Timeout**: 30 seconds for PIN entry
- **Retry**: Automatic retry on authentication failure
- **Session**: PIN valid until device removal
- **Recovery**: Automatic detection of reconnection

### Status Checking
- **Device Detection**: ~1-2 seconds
- **PIN Status**: ~2-5 seconds
- **Authentication**: ~1-3 seconds
- **Key Operations**: ~1-2 seconds

## Best Practices

### PIN Management
- Use strong, unique PINs
- Never share PINs
- Change PINs regularly
- Keep backup PINs secure

### Device Handling
- Remove device when not in use
- Store device securely
- Check device status regularly
- Use device-specific keys

### Error Handling
- Always check return values
- Handle authentication failures gracefully
- Provide user guidance for errors
- Log security events appropriately

## Integration Examples

### Python Integration

```python
import gnuradio.m17 as m17

# Initialize with enhanced PIN handling
coder = m17.m17_coder(
    src_id="N0CALL",
    dst_id="N0CALL",
    # ... other parameters
)

# Check status with enhanced error handling
if coder.check_nitrokey_status():
    print("Nitrokey ready")
else:
    print("Check Nitrokey status")

# Sign with automatic PIN handling
data = b"Hello, M17!"
signature = coder.sign_with_nitrokey(data, len(data))
```

### C++ Integration

```cpp
#include <gnuradio/m17/m17_coder.h>

// Initialize with enhanced PIN handling
auto coder = gr::m17::m17_coder::make(...);

// Enhanced status checking
if (coder->check_nitrokey_status()) {
    // Nitrokey is ready
} else {
    // Handle error with detailed information
    coder->report_nitrokey_status();
}

// Sign with automatic PIN handling
const char* data = "Hello, M17!";
uint8_t signature[64];
if (coder->sign_with_nitrokey((const uint8_t*)data, strlen(data), signature)) {
    // Signing successful
}
```

## Conclusion

The enhanced PIN authentication features provide:

- **Better User Experience**: Clear status messages and automatic handling
- **Enhanced Security**: Proper PIN authentication management
- **Improved Reliability**: Better error handling and recovery
- **Comprehensive Monitoring**: Detailed status reporting

These features make Nitrokey integration more user-friendly while maintaining the highest security standards.

