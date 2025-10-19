/**
 * @file m17_documentation.h
 * @brief M17 Digital Voice Protocol Documentation
 * 
 * This file contains comprehensive Doxygen documentation for the M17 digital voice
 * protocol implementation, including security considerations, usage examples, and
 * compatibility information.
 * 
 * @author M17 Development Team
 * @version 1.0
 * @date 2024
 * 
 * @section overview Overview
 * 
 * The M17 Digital Voice Protocol is a digital voice protocol designed for amateur
 * radio use. This implementation provides comprehensive security features including
 * hardware security module (HSM) support, cryptographic authentication, and
 * compatibility modes.
 * 
 * @section security Security Features
 * 
 * The M17 implementation includes several critical security features:
 * 
 * @subsection crypto Cryptographic Security
 * - **Ed25519 Digital Signatures**: Modern elliptic curve digital signatures
 * - **Curve25519 ECDH**: Secure key agreement for encrypted channels
 * - **P-256 ECDSA**: M17 specification compliant signatures
 * - **AES-256-GCM**: Authenticated encryption for extended mode
 * - **AES-256-CTR**: M17 specification compliant encryption
 * - **HKDF**: Secure key derivation from shared secrets
 * - **SHA-256**: Cryptographic hashing for signatures
 * 
 * @subsection memory Memory Security
 * - **Secure Memory Clearing**: Uses explicit_bzero for sensitive data
 * - **Memory Protection**: Bounds checking and overflow protection
 * - **Secure Random Number Generation**: Uses /dev/urandom and /dev/hwrng
 * - **Key Management**: Hardware security module integration
 * 
 * @subsection authentication Authentication
 * - **Digital Signatures**: Ed25519 and P-256 ECDSA support
 * - **Challenge-Response**: Secure authentication protocols
 * - **Replay Protection**: Timestamp and nonce validation
 * - **Hardware Security**: Nitrokey HSM integration
 * 
 * @section compatibility Compatibility Modes
 * 
 * The M17 implementation supports two compatibility modes:
 * 
 * @subsection strict_mode M17 Strict Mode (Default)
 * 
 * Full M17 specification compliance:
 * - P-256 ECDSA for digital signatures
 * - AES-256-CTR for encryption
 * - Standard LSF/META field usage
 * - Compatible with all M17 implementations
 * 
 * @subsection extended_mode Extended Mode (Nitrokey)
 * 
 * Enhanced features with hardware security:
 * - Ed25519 digital signatures
 * - Curve25519 ECDH key agreement
 * - AES-256-GCM authenticated encryption
 * - Nitrokey HSM integration
 * - Challenge-response authentication
 * 
 * @warning SECURITY WARNING: Extended mode is NOT M17 spec compliant
 * @warning SECURITY WARNING: Extended mode requires coordination with other stations
 * @note Use strict mode for maximum compatibility
 * @note Extended mode is for advanced users with Nitrokey hardware
 * 
 * @section usage Usage Examples
 * 
 * @subsection basic_usage Basic Usage
 * @code
 * // Create decoder with default settings (strict mode)
 * m17_decoder_impl decoder(
 *     false,  // debug_data
 *     false,  // debug_ctrl
 *     0.9,    // threshold
 *     true,   // callsign display
 *     false,  // signed_str
 *     0,      // ENCR_NONE
 *     "",     // key (set later)
 *     ""      // seed (set later)
 * );
 * 
 * // Set encryption key (32 bytes = 64 hex characters)
 * decoder.set_key("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
 * 
 * // Enable strict M17 mode (default)
 * decoder.set_m17_strict_mode(true);
 * @endcode
 * 
 * @subsection extended_usage Extended Mode Usage
 * @code
 * // Enable extended crypto mode (requires Nitrokey)
 * decoder.set_extended_crypto(true);
 * 
 * // Set Ed25519 keys for digital signatures
 * uint8_t ed25519_pub[32], ed25519_priv[32];
 * // ... load keys from secure storage ...
 * decoder.set_ed25519_keys(ed25519_pub, 32, ed25519_priv, 32);
 * 
 * // Set Curve25519 keys for ECDH
 * uint8_t curve25519_pub[32], curve25519_priv[32];
 * // ... load keys from secure storage ...
 * decoder.set_curve25519_keys(curve25519_pub, 32, curve25519_priv, 32);
 * @endcode
 * 
 * @subsection nitrokey_usage Nitrokey Integration
 * @code
 * // Initialize Nitrokey hardware security module
 * if (decoder.init_nitrokey_security("123456")) {
 *     // Use hardware-backed cryptography
 *     decoder.sign_with_hardware(data, len, signature, 0);
 *     decoder.ecdh_with_hardware(peer_pubkey, shared_secret, 0);
 * }
 * @endcode
 * 
 * @section security_warnings Security Warnings
 * 
 * @warning SECURITY CRITICAL: Never log cryptographic keys, private keys, or seeds
 * @warning SECURITY CRITICAL: Always use secure memory clearing for sensitive data
 * @warning SECURITY CRITICAL: Validate all cryptographic operations return codes
 * @warning SECURITY CRITICAL: Use hardware security modules for production systems
 * @warning SECURITY CRITICAL: Never use weak random number generators
 * @warning SECURITY CRITICAL: Always validate input parameters
 * 
 * @section best_practices Best Practices
 * 
 * @subsection key_management Key Management
 * - Store private keys in hardware security modules
 * - Use secure key derivation functions
 * - Implement proper key rotation
 * - Clear sensitive memory after use
 * 
 * @subsection authentication Authentication
 * - Use digital signatures for message authentication
 * - Implement challenge-response protocols
 * - Validate timestamps and nonces
 * - Protect against replay attacks
 * 
 * @subsection encryption Encryption
 * - Use authenticated encryption (AES-GCM)
 * - Generate random IVs for each message
 * - Use secure key agreement (ECDH)
 * - Implement proper key derivation
 * 
 * @section troubleshooting Troubleshooting
 * 
 * @subsection common_issues Common Issues
 * - **Invalid key format**: Ensure keys are hex-encoded (64 characters for 32 bytes)
 * - **Compatibility issues**: Use strict mode for maximum compatibility
 * - **Security warnings**: Review all security warnings and fix issues
 * - **Hardware issues**: Ensure Nitrokey is properly connected and initialized
 * 
 * @subsection debugging Debugging
 * - Enable debug output for troubleshooting
 * - Check cryptographic operation return codes
 * - Validate input parameters
 * - Review security audit reports
 * 
 * @section references References
 * 
 * - M17 Digital Voice Protocol Specification
 * - GNU Radio Documentation
 * - OpenSSL Documentation
 * - Nitrokey Documentation
 * - Security Best Practices Guide
 * 
 * @section changelog Changelog
 * 
 * @version 1.0
 * - Initial implementation with basic M17 support
 * - Added security features and hardware integration
 * - Implemented compatibility modes
 * - Added comprehensive documentation
 */
