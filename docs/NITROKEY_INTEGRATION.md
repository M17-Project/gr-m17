# Nitrokey Hardware Security Integration

## Overview

The M17 GNU Radio module includes comprehensive Nitrokey hardware security module (HSM) integration, providing enterprise-grade cryptographic key storage and operations. This integration ensures that private keys never leave the hardware security module, providing maximum security for M17 digital radio communications.

## Nitrokey Features

### Hardware Security Features

- **Tamper Resistant Design**: Physical protection against hardware attacks
- **Open Source Firmware**: Fully auditable firmware with community transparency
- **Hardware Random Number Generator**: True hardware entropy for cryptographic operations
- **Secure Key Storage**: Private keys stored in hardware, never exposed to host system
- **Cryptographic Operations**: All signing and key generation performed in hardware
- **Zero Key Exposure**: Private keys never leave the Nitrokey device

### Supported Nitrokey Devices

- **Nitrokey 3**: Latest generation with USB-C and NFC support
- **Nitrokey Start**: Open source hardware with FIDO2 support
- **Nitrokey Pro 2**: High-security model with smart card functionality
- **Nitrokey HSM**: Hardware security module for server environments

## Security Benefits

### Key Isolation
- Private keys are generated and stored exclusively on the Nitrokey
- Cryptographic operations performed in hardware
- No private key material ever transmitted to host system
- Protection against software-based key extraction attacks

### Tamper Resistance
- Physical protection against hardware tampering
- Automatic key destruction on tamper detection
- Secure element design prevents key extraction
- Open source firmware allows security auditing

### Open Source Advantages
- **Transparent Security**: All firmware code is publicly auditable
- **Community Verification**: Security researchers can review and improve code
- **No Backdoors**: Open source ensures no hidden vulnerabilities
- **Customizable**: Firmware can be modified for specific security requirements

## Usage Examples

### Basic Setup

```bash
# Install nitropy CLI tool
pip install pynitrokey

# Check Nitrokey connection
nitropy nk3 list

# Generate Ed25519 key on Nitrokey
nitropy nk3 secrets add-password --name "m17-radio-key" --algorithm ed25519
```

### M17 Integration

```python
# Python example for M17 with Nitrokey
import gnuradio.m17 as m17

# Initialize M17 coder with Nitrokey support
coder = m17.m17_coder(
    src_id="N0CALL",
    dst_id="N0CALL", 
    mode=0,  # Stream mode
    data=0,  # No data
    encr_type=2,  # AES encryption
    encr_subtype=0,
    aes_subtype=0,
    can=0,
    meta="",
    key="",  # Key will be loaded from Nitrokey
    priv_key="",  # Private key on Nitrokey
    debug=False,
    signed_str=True,  # Enable digital signatures
    seed=""
)

# Set Nitrokey key for signing
coder.set_nitrokey_key("m17-radio-key")
```

### C++ Integration

```cpp
#include <gnuradio/m17/m17_coder.h>

// Initialize M17 coder with Nitrokey
auto coder = gr::m17::m17_coder::make(
    "N0CALL",  // Source callsign
    "N0CALL",  // Destination callsign
    0,         // Stream mode
    0,         // No data
    2,         // AES encryption
    0,         // Encryption subtype
    0,         // AES subtype
    0,         // CAN
    "",        // Meta
    "",        // Key (from Nitrokey)
    "",        // Private key (on Nitrokey)
    false,     // Debug
    true,      // Signed stream
    ""         // Seed
);

// Configure Nitrokey integration
coder->set_nitrokey_key("m17-radio-key");
```

### Key Management

```bash
# List all keys on Nitrokey
nitropy nk3 secrets list

# Export public key for sharing
nitropy nk3 secrets get-public-key --name "m17-radio-key" --output public_key.pem

# Delete key from Nitrokey
nitropy nk3 secrets delete --name "m17-radio-key"

# Generate new key with specific parameters
nitropy nk3 secrets add-password --name "m17-backup-key" --algorithm ed25519
```

## Advanced Features

### Multi-Key Support

```python
# Support for multiple Nitrokey keys
coder.set_nitrokey_key("m17-primary-key")    # Primary signing key
coder.set_nitrokey_key("m17-backup-key")    # Backup key
coder.set_nitrokey_key("m17-emergency-key")  # Emergency key
```

### Key Rotation

```bash
# Generate new key
nitropy nk3 secrets add-password --name "m17-key-v2" --algorithm ed25519

# Update M17 configuration
# (Update your M17 application to use new key)

# Archive old key
nitropy nk3 secrets delete --name "m17-key-v1"
```

### Backup and Recovery

```bash
# Export public key for backup
nitropy nk3 secrets get-public-key --name "m17-radio-key" --output backup_public_key.pem

# Import public key on another system
# (Use the exported public key to verify signatures from this Nitrokey)
```

## Security Best Practices

### Key Management
- Use unique key names for different purposes
- Regularly rotate cryptographic keys
- Keep backup copies of public keys
- Never share private keys (they never leave the Nitrokey)

### Physical Security
- Store Nitrokey in secure location when not in use
- Use strong PIN/password for Nitrokey access
- Enable tamper detection features
- Keep firmware updated

### Operational Security
- Verify public keys before trusting signatures
- Use multiple Nitrokey devices for redundancy
- Implement proper key escrow procedures
- Monitor for unauthorized key usage

## Where to Get Nitrokey

### Official Sources
- **Nitrokey Website**: https://www.nitrokey.com/
- **Nitrokey Shop**: https://shop.nitrokey.com/
- **Amazon**: Available through official Nitrokey store
- **Distributors**: Check website for local distributors

### Recommended Models for M17
- **Nitrokey 3**: Best overall choice for M17 integration
- **Nitrokey Start**: Budget-friendly option with good security
- **Nitrokey Pro 2**: High-security option for critical applications

### Pricing
- **Nitrokey Start**: ~€25-30 (budget option)
- **Nitrokey 3**: ~€50-60 (recommended)
- **Nitrokey Pro 2**: ~€80-100 (high security)
- **Nitrokey HSM**: ~€200+ (server/enterprise)

## Technical Specifications

### Cryptographic Support
- **Ed25519**: Digital signatures
- **Curve25519**: Key agreement
- **AES-256**: Symmetric encryption
- **SHA-256**: Hashing
- **Hardware RNG**: True random number generation

### Interface Support
- **USB-A**: Traditional USB connection
- **USB-C**: Modern USB-C connection (Nitrokey 3)
- **NFC**: Near-field communication (Nitrokey 3)
- **Smart Card**: ISO 7816 interface (Pro models)

### Operating System Support
- **Linux**: Full support with open source drivers
- **Windows**: Supported with official drivers
- **macOS**: Supported with official drivers
- **FreeBSD**: Community-supported drivers

## Troubleshooting

### Common Issues

**Nitrokey not detected:**
```bash
# Check USB connection
lsusb | grep Nitrokey

# Install udev rules
sudo apt install libnitrokey-dev

# Check permissions
sudo usermod -a -G plugdev $USER
```

**Key generation fails:**
```bash
# Check Nitrokey status
nitropy nk3 list

# Verify connection
nitropy nk3 status

# Reset if needed
nitropy nk3 reset
```

**M17 integration issues:**
```bash
# Verify nitropy installation
pip install --upgrade pynitrokey

# Check M17 build with Nitrokey support
cmake -DNITROKEY_SUPPORT=ON ..
make
```

### Support Resources
- **Nitrokey Documentation**: https://docs.nitrokey.com/
- **Community Forum**: https://github.com/Nitrokey/nitrokey-start-firmware
- **M17 Project**: https://github.com/M17-Project/gr-m17
- **Security Audits**: https://www.nitrokey.com/security

## Compliance and Standards

### Security Standards
- **FIPS 140-2**: Hardware security module standards
- **Common Criteria**: International security evaluation
- **Open Source**: Transparent and auditable code
- **Hardware Security**: Tamper-resistant design

### Certifications
- **CE Marking**: European conformity
- **FCC Certification**: US radio frequency compliance
- **Open Source**: GPL licensed firmware
- **Community Audited**: Public security reviews

## Conclusion

The Nitrokey integration with M17 provides enterprise-grade security for digital radio communications. The combination of open source firmware, tamper-resistant hardware, and zero key exposure makes it an ideal solution for secure M17 implementations.

For more information, visit:
- **Nitrokey Website**: https://www.nitrokey.com/
- **M17 Project**: https://github.com/M17-Project/gr-m17
- **Documentation**: https://docs.nitrokey.com/
