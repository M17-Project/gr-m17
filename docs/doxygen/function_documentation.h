/**
 * @file function_documentation.h
 * @brief M17 Function Documentation
 * 
 * This file contains detailed Doxygen documentation for M17 functions,
 * including security considerations, usage examples, and parameter descriptions.
 */

/**
 * @brief Set encryption key for M17 decoder
 * @param arg Hex-encoded encryption key (32 bytes = 64 hex characters)
 * 
 * @details This function sets the encryption key used for decrypting M17 frames.
 * The key must be provided as a hex-encoded string (64 characters for 32 bytes).
 * 
 * @section security Security Considerations
 * 
 * @warning SECURITY CRITICAL: Never log the key value or store it in plaintext
 * @warning SECURITY CRITICAL: Use secure memory clearing when done with the key
 * @note The key is validated for proper hex format and length
 * @note Invalid keys will be rejected with error logging
 * 
 * @section usage Usage Example
 * @code
 * // Set a 32-byte encryption key (64 hex characters)
 * decoder.set_key("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
 * @endcode
 * 
 * @param arg Hex-encoded encryption key (must be exactly 64 characters)
 * @return void
 * @throws std::invalid_argument if key format is invalid
 * 
 * @see set_seed() for setting the scrambler seed
 * @see set_ed25519_keys() for setting Ed25519 keys
 * @see set_curve25519_keys() for setting Curve25519 keys
 */
void set_key(std::string arg);

/**
 * @brief Set scrambler seed for M17 decoder
 * @param seed Hex-encoded scrambler seed (variable length)
 * 
 * @details This function sets the scrambler seed used for descrambling M17 frames.
 * The seed is used to initialize the scrambler sequence generator.
 * 
 * @section security Security Considerations
 * 
 * @warning SECURITY CRITICAL: Never log the seed value
 * @warning SECURITY CRITICAL: Use secure memory clearing for seed data
 * @note The seed is validated for proper hex format
 * @note Invalid seeds will be rejected with error logging
 * 
 * @section usage Usage Example
 * @code
 * // Set a scrambler seed
 * decoder.set_seed("deadbeefcafebabe");
 * @endcode
 * 
 * @param seed Hex-encoded scrambler seed (hex string)
 * @return void
 * @throws std::invalid_argument if seed format is invalid
 * 
 * @see set_key() for setting the encryption key
 * @see scrambler_sequence_generator() for scrambler implementation
 */
void set_seed(std::string seed);

/**
 * @brief Set Ed25519 cryptographic keys
 * @param public_key Ed25519 public key (32 bytes)
 * @param pub_key_size Size of public key (must be 32)
 * @param private_key Ed25519 private key (32 bytes)
 * @param priv_key_size Size of private key (must be 32)
 * 
 * @details This function sets the Ed25519 keys used for digital signatures
 * in extended crypto mode. Ed25519 provides modern elliptic curve cryptography
 * with strong security guarantees.
 * 
 * @section security Security Considerations
 * 
 * @warning SECURITY CRITICAL: Never log private keys
 * @warning SECURITY CRITICAL: Store private keys in hardware security modules
 * @warning SECURITY CRITICAL: Use secure memory clearing for key data
 * @note Keys are validated for proper size and format
 * @note Invalid keys will be rejected with error logging
 * 
 * @section usage Usage Example
 * @code
 * // Set Ed25519 keys for digital signatures
 * uint8_t ed25519_pub[32], ed25519_priv[32];
 * // ... load keys from secure storage ...
 * decoder.set_ed25519_keys(ed25519_pub, 32, ed25519_priv, 32);
 * @endcode
 * 
 * @param public_key Ed25519 public key (32 bytes)
 * @param pub_key_size Size of public key (must be 32)
 * @param private_key Ed25519 private key (32 bytes)
 * @param priv_key_size Size of private key (must be 32)
 * @return void
 * @throws std::invalid_argument if key sizes are invalid
 * 
 * @see set_curve25519_keys() for ECDH keys
 * @see verify_ed25519_signature() for signature verification
 * @see sign_ed25519_message() for message signing
 */
void set_ed25519_keys(const uint8_t* public_key, size_t pub_key_size,
                      const uint8_t* private_key, size_t priv_key_size);

/**
 * @brief Set Curve25519 cryptographic keys
 * @param public_key Curve25519 public key (32 bytes)
 * @param pub_key_size Size of public key (must be 32)
 * @param private_key Curve25519 private key (32 bytes)
 * @param priv_key_size Size of private key (must be 32)
 * 
 * @details This function sets the Curve25519 keys used for ECDH key agreement
 * in extended crypto mode. Curve25519 provides secure key exchange for
 * establishing shared secrets.
 * 
 * @section security Security Considerations
 * 
 * @warning SECURITY CRITICAL: Never log private keys
 * @warning SECURITY CRITICAL: Store private keys in hardware security modules
 * @warning SECURITY CRITICAL: Use secure memory clearing for key data
 * @note Keys are validated for proper size and format
 * @note Invalid keys will be rejected with error logging
 * 
 * @section usage Usage Example
 * @code
 * // Set Curve25519 keys for ECDH
 * uint8_t curve25519_pub[32], curve25519_priv[32];
 * // ... load keys from secure storage ...
 * decoder.set_curve25519_keys(curve25519_pub, 32, curve25519_priv, 32);
 * @endcode
 * 
 * @param public_key Curve25519 public key (32 bytes)
 * @param pub_key_size Size of public key (must be 32)
 * @param private_key Curve25519 private key (32 bytes)
 * @param priv_key_size Size of private key (must be 32)
 * @return void
 * @throws std::invalid_argument if key sizes are invalid
 * 
 * @see set_ed25519_keys() for signature keys
 * @see perform_curve25519_ecdh() for key agreement
 * @see derive_encryption_key() for key derivation
 */
void set_curve25519_keys(const uint8_t* public_key, size_t pub_key_size,
                         const uint8_t* private_key, size_t priv_key_size);

/**
 * @brief Verify Ed25519 digital signature
 * @param data Data to verify (raw bytes)
 * @param data_len Length of data
 * @param signature Ed25519 signature (64 bytes)
 * @param sig_size Size of signature (must be 64)
 * @param public_key Ed25519 public key (32 bytes)
 * @param pub_key_size Size of public key (must be 32)
 * @return 0 on success, -1 on failure
 * 
 * @details This function verifies an Ed25519 digital signature against the
 * provided data and public key. Ed25519 provides strong security guarantees
 * and is resistant to side-channel attacks.
 * 
 * @section security Security Considerations
 * 
 * @warning SECURITY CRITICAL: Always validate return codes
 * @warning SECURITY CRITICAL: Use constant-time operations for signature verification
 * @note Signature verification is cryptographically secure
 * @note Invalid signatures will be rejected
 * 
 * @section usage Usage Example
 * @code
 * // Verify Ed25519 signature
 * if (decoder.verify_ed25519_signature(data, data_len, signature, 64, 
 *                                       public_key, 32) == 0) {
 *     printf("Signature verified successfully\n");
 * } else {
 *     printf("Signature verification failed\n");
 * }
 * @endcode
 * 
 * @param data Data to verify (raw bytes)
 * @param data_len Length of data
 * @param signature Ed25519 signature (64 bytes)
 * @param sig_size Size of signature (must be 64)
 * @param public_key Ed25519 public key (32 bytes)
 * @param pub_key_size Size of public key (must be 32)
 * @return 0 on success, -1 on failure
 * 
 * @see sign_ed25519_message() for message signing
 * @see set_ed25519_keys() for setting keys
 */
int verify_ed25519_signature(const uint8_t* data, size_t data_len,
                            const uint8_t* signature, size_t sig_size,
                            const uint8_t* public_key, size_t pub_key_size);

/**
 * @brief Perform Curve25519 ECDH key agreement
 * @param peer_public_key Peer's Curve25519 public key (32 bytes)
 * @param pub_key_size Size of peer's public key (must be 32)
 * @return 0 on success, -1 on failure
 * 
 * @details This function performs Curve25519 ECDH key agreement to establish
 * a shared secret with a peer. The shared secret can then be used for
 * symmetric encryption.
 * 
 * @section security Security Considerations
 * 
 * @warning SECURITY CRITICAL: Always validate return codes
 * @warning SECURITY CRITICAL: Use secure key derivation after ECDH
 * @note ECDH provides forward secrecy
 * @note Shared secrets should be derived using HKDF
 * 
 * @section usage Usage Example
 * @code
 * // Perform ECDH key agreement
 * if (decoder.perform_curve25519_ecdh(peer_pubkey, 32) == 0) {
 *     // Derive encryption key from shared secret
 *     uint8_t encryption_key[32];
 *     decoder.derive_encryption_key(shared_secret, 32, salt, salt_len, encryption_key, 32);
 * }
 * @endcode
 * 
 * @param peer_public_key Peer's Curve25519 public key (32 bytes)
 * @param pub_key_size Size of peer's public key (must be 32)
 * @return 0 on success, -1 on failure
 * 
 * @see derive_encryption_key() for key derivation
 * @see set_curve25519_keys() for setting keys
 */
int perform_curve25519_ecdh(const uint8_t* peer_public_key, size_t pub_key_size);

/**
 * @brief Set M17 strict mode
 * @param strict_mode Enable strict M17 compliance
 * 
 * @details This function enables or disables M17 strict mode. In strict mode,
 * only M17 specification compliant features are used (P-256 ECDSA, AES-256-CTR).
 * This ensures maximum compatibility with other M17 implementations.
 * 
 * @section compatibility Compatibility
 * 
 * @b Strict Mode (true):
 * - P-256 ECDSA for digital signatures
 * - AES-256-CTR for encryption
 * - Standard LSF/META field usage
 * - Compatible with all M17 implementations
 * 
 * @b Extended Mode (false):
 * - Ed25519 digital signatures
 * - Curve25519 ECDH key agreement
 * - AES-256-GCM authenticated encryption
 * - Nitrokey HSM integration
 * - NOT M17 spec compliant
 * 
 * @warning SECURITY WARNING: Extended mode is NOT M17 spec compliant
 * @warning SECURITY WARNING: Extended mode requires coordination with other stations
 * @note Use strict mode for maximum compatibility
 * @note Extended mode is for advanced users with Nitrokey hardware
 * 
 * @section usage Usage Example
 * @code
 * // Enable strict M17 mode (default, recommended)
 * decoder.set_m17_strict_mode(true);
 * 
 * // Enable extended mode (requires coordination)
 * decoder.set_m17_strict_mode(false);
 * decoder.set_extended_crypto(true);
 * @endcode
 * 
 * @param strict_mode Enable strict M17 compliance
 * @return void
 * 
 * @see set_extended_crypto() for extended mode
 * @see is_m17_compatible() for compatibility check
 */
void set_m17_strict_mode(bool strict_mode);

/**
 * @brief Initialize Nitrokey hardware security module
 * @param pin Nitrokey PIN for authentication
 * @return true on success, false on failure
 * 
 * @details This function initializes the Nitrokey hardware security module
 * for secure cryptographic operations. The Nitrokey provides hardware-backed
 * key storage and cryptographic operations.
 * 
 * @section security Security Considerations
 * 
 * @warning SECURITY CRITICAL: Never log the PIN
 * @warning SECURITY CRITICAL: Use strong PINs for production
 * @note Nitrokey provides hardware security for key storage
 * @note Cryptographic operations are performed in hardware
 * 
 * @section usage Usage Example
 * @code
 * // Initialize Nitrokey HSM
 * if (decoder.init_nitrokey_security("123456")) {
 *     printf("Nitrokey initialized successfully\n");
 *     // Use hardware-backed cryptography
 *     decoder.sign_with_hardware(data, len, signature, 0);
 * } else {
 *     printf("Nitrokey initialization failed\n");
 * }
 * @endcode
 * 
 * @param pin Nitrokey PIN for authentication
 * @return true on success, false on failure
 * 
 * @see sign_with_hardware() for hardware signing
 * @see ecdh_with_hardware() for hardware ECDH
 * @see decrypt_with_hardware() for hardware decryption
 */
bool init_nitrokey_security(const char* pin);
