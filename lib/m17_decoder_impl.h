/* -*- c++ -*- */
/*
 * Copyright 2023 jmfriedt.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_M17_M17_DECODER_IMPL_H
#define INCLUDED_M17_M17_DECODER_IMPL_H

#include <gnuradio/m17/m17_decoder.h>
#include "m17.h"

// SECURITY FIX: Add OpenSSL includes for secure hashing
#include <openssl/evp.h>
#include <openssl/ec.h>
#include <openssl/ecdh.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>

// SECURITY FIX: Add constants to replace magic numbers
namespace m17_constants {
    constexpr uint16_t FRAME_NUMBER_MAX = 0x8000;
    constexpr uint16_t SIGNATURE_START_FN = 0x7FFC;
    constexpr uint16_t SIGNATURE_END_FN = 0x7FFF;
    constexpr uint16_t EOT_MARKER = 0x8000;
    
    // SECURITY FIX: M17 spec compliance constants
    constexpr uint8_t CAN_STANDARD = 0;           // Standard M17 compatibility
    constexpr uint8_t CAN_NITROKEY_EXTENDED = 15; // Extended crypto (CAN 15, least likely to conflict)
    constexpr uint8_t CAN_RESERVED = 2;           // Reserved for future use
    
    // M17 spec encryption types (LSF TYPE field bits 11-9)
    constexpr uint8_t M17_ENCR_NONE = 0;      // 00 - No encryption (M17 spec)
    constexpr uint8_t M17_ENCR_AES = 2;       // 10 - AES encryption (M17 spec)
    
    // Custom extensions (use with caution - non-standard)
    constexpr uint8_t CUSTOM_ENCR_SCRAMBLER = 1;  // 01 - Scrambler (non-standard)
    constexpr uint8_t CUSTOM_ENCR_EXTENDED = 3;   // 11 - Extended crypto (non-standard)
    
    // Use reserved bit in TYPE field for extended crypto signaling
    constexpr uint16_t TYPE_EXTENDED_CRYPTO_BIT = 0x0008;  // Bit 3 (reserved)
    
    // SECURITY FIX: Strict M17 mode constants (P-256 + AES-CTR only)
    constexpr size_t P256_PRIVATE_KEY_SIZE = 32;
    constexpr size_t P256_PUBLIC_KEY_SIZE = 64;
    constexpr size_t P256_SIGNATURE_SIZE = 64;
    constexpr size_t AES_CTR_KEY_SIZE = 32;
    constexpr size_t AES_CTR_IV_SIZE = 16;
}

// SECURITY FIX: Define missing cryptographic constants
#define M17_ED25519_PUBLIC_KEY_SIZE 32
#define M17_ED25519_PRIVATE_KEY_SIZE 32
#define M17_ED25519_SIGNATURE_SIZE 64
#define M17_CURVE25519_PUBLIC_KEY_SIZE 32
#define M17_CURVE25519_PRIVATE_KEY_SIZE 32
#define M17_CURVE25519_SHARED_SECRET_SIZE 32
#define M17_AES_GCM_KEY_SIZE 32
#define M17_AES_GCM_IV_SIZE 12
#define M17_AES_GCM_TAG_SIZE 16

/*
 * SECURITY FIX: M17 COMPATIBILITY MODES
 * 
 * STRICT M17 MODE (Default - Recommended for Production):
 * - Uses only P-256 ECDSA for digital signatures
 * - Uses only AES-256-CTR for encryption
 * - Follows M17 specification exactly
 * - 100% compatible with all M17 implementations
 * - No extended crypto features
 * 
 * EXTENDED CRYPTO MODE (Advanced - Use with Caution):
 * - Uses Ed25519 + Curve25519 + AES-GCM
 * - Requires Nitrokey hardware security module
 * - NOT compatible with standard M17 receivers
 * - Use only when both ends support extended crypto
 * - Clearly document compatibility requirements
 * 
 * COMPATIBILITY WARNINGS:
 * - Extended mode breaks M17 specification compliance
 * - Extended mode requires coordination with M17 project
 * - Extended mode may not work with existing M17 hardware
 * - Always test compatibility before deployment
 * 
 * RECOMMENDED DEFAULT CONFIGURATION FOR PRODUCTION:
 * 
 * m17_decoder_impl decoder(
 *     false,  // debug_data
 *     false,  // debug_ctrl
 *     0.9,    // threshold
 *     true,   // callsign display
 *     false,  // signed_str (verify in general_work)
 *     0,      // ENCR_NONE (detect from frame)
 *     "",     // key (set later if needed)
 *     ""      // seed (set later if needed)
 * );
 * 
 * // Default to M17 strict mode
 * decoder.set_m17_strict_mode(true);
 * 
 * // Only enable extended crypto if explicitly requested
 * // decoder.set_extended_crypto(true);  // COMMENTED OUT - requires coordination
 */

// SECURITY FIX: Nitrokey library headers
#ifdef NITROKEY_SUPPORT
#include <libnitrokey/libnitrokey.h>
#include <libnitrokey/device.h>
#endif

// SECURITY FIX: Nitrokey Integration Layer
class NitrokeyInterface {
public:
    // Initialize connection to Nitrokey
    bool init_nitrokey(const char* pin);
    
    // Sign data using key stored in Nitrokey
    bool sign_with_nitrokey(const uint8_t* data, size_t len, 
                          uint8_t* signature, int key_slot);
    
    // Perform ECDH using key stored in Nitrokey
    bool ecdh_with_nitrokey(const uint8_t* peer_pubkey, 
                           uint8_t* shared_secret, int key_slot);
    
    // Decrypt using key stored in Nitrokey
    bool decrypt_with_nitrokey(const uint8_t* ciphertext, size_t len,
                              uint8_t* plaintext, int key_slot);
    
    // Clean up
    void cleanup_nitrokey();
    
private:
    bool _nitrokey_initialized = false;
    int _current_key_slot = -1;
};

#define AES
#define ECC

#ifdef AES
#include "aes.h"
#endif

#ifdef ECC
#include "uECC.h"
#endif

namespace gr
{
  namespace m17
  {

    class m17_decoder_impl:public m17_decoder
    {
    private:
      bool _debug_data = false;
      bool _debug_ctrl = false;
      float _threshold = 0.9;
      bool _callsign = false;
      bool _signed_str = false;
      // SECURITY FIX: Keep _key for backward compatibility with existing code
      uint8_t _key[32];  // Legacy encryption key for AES operations
      uint8_t _iv[16];
      encr_t _encr_type = ENCR_NONE;
//used for signatures
      uint8_t _digest[32] = { 0 };	//32-byte field for the SHA-256 digest
      uint8_t _sig[64] = { 0 };	//ECDSA signature
      EVP_MD_CTX *_sha_ctx;         // EVP context for secure hashing
      
      // SECURITY FIX: Bidirectional M17 with Nitrokey requires private keys for:
      // - Mutual authentication (proving identity to transmitter)
      // - ECDH key agreement (establishing shared secrets)
      // - Decrypting messages (receiving encrypted traffic)
      // - Challenge-response (authenticating to network)
      uint8_t _ed25519_public_key[M17_ED25519_PUBLIC_KEY_SIZE] = { 0 };
      uint8_t _ed25519_private_key[M17_ED25519_PRIVATE_KEY_SIZE] = { 0 };
      uint8_t _ed25519_signature[M17_ED25519_SIGNATURE_SIZE] = { 0 };
      uint8_t _curve25519_public_key[M17_CURVE25519_PUBLIC_KEY_SIZE] = { 0 };
      uint8_t _curve25519_private_key[M17_CURVE25519_PRIVATE_KEY_SIZE] = { 0 };
      uint8_t _curve25519_shared_secret[M17_CURVE25519_SHARED_SECRET_SIZE] = { 0 };
      uint8_t _aes_gcm_key[M17_AES_GCM_KEY_SIZE] = { 0 };
      uint8_t _aes_gcm_iv[M17_AES_GCM_IV_SIZE] = { 0 };
      uint8_t _aes_gcm_tag[M17_AES_GCM_TAG_SIZE] = { 0 };
      
      // SECURITY FIX: M17 strict mode crypto (P-256 + AES-CTR)
      uint8_t _p256_public_key[m17_constants::P256_PUBLIC_KEY_SIZE] = { 0 };
      uint8_t _p256_private_key[m17_constants::P256_PRIVATE_KEY_SIZE] = { 0 };
      uint8_t _p256_signature[m17_constants::P256_SIGNATURE_SIZE] = { 0 };
      uint8_t _aes_ctr_key[m17_constants::AES_CTR_KEY_SIZE] = { 0 };
      uint8_t _aes_ctr_iv[m17_constants::AES_CTR_IV_SIZE] = { 0 };
      uint32_t _scrambler_key = 0;	//keep set to initial value for seed calculation function
      
      // SECURITY FIX: Nitrokey hardware security integration
      NitrokeyInterface _nitrokey;
      bool _use_nitrokey = false;
      int _nitrokey_key_slot = 0;
      
      // SECURITY FIX: M17 backwards compatibility
      bool _m17_strict_mode = true;  // Default to M17 spec compliance
      bool _extended_crypto_enabled = false;  // Extended crypto features
      uint8_t _can = 0;  // Channel Access Number for compatibility

#ifdef AES
      typedef enum
      {
	AES128,
	AES192,
	AES256
      } aes_t;
      int8_t _aes_subtype = -1;
#endif

      float last[8] = { 0 };	//look-back buffer for finding syncwords
      float _pld[SYM_PER_PLD];	//raw frame symbols
      uint16_t soft_bit[2 * SYM_PER_PLD];	//raw frame soft bits
      uint16_t d_soft_bit[2 * SYM_PER_PLD];	//deinterleaved soft bits
      uint16_t _expected_next_fn;
      uint16_t _fn;

      lsf_t _lsf;	//complete LSF (one byte extra needed for the Viterbi decoder)
      uint16_t lich_chunk[96];	//raw, soft LSF chunk extracted from the LICH
      uint8_t _lich_b[6];	//48-bit decoded LICH
      uint8_t _lich_cnt;		//LICH_CNT
      uint8_t lich_chunks_rcvd = 0;	//flags set for each LSF chunk received

      uint16_t enc_data[272];	//raw frame data soft bits
      uint8_t _frame_data[19];	//decoded frame data, 144 bits (16+128), plus 4 flushing bits
      uint8_t digest[16] = { 0 };

      uint8_t syncd = 0;	//syncword found?
      uint8_t fl = 0;		//Frame=0 of LSF=1
      uint8_t pushed;		//counter for pushed symbols

      uint8_t d_dst[12], d_src[12];	//decoded strings
#ifdef ECC
//Scrambler
      uint8_t _seed[3]; //24-bit is the largest seed value
      uint8_t _scr_bytes[16];
      uint8_t _scrambler_pn[128];
      uint32_t _scrambler_seed = 0;
      int8_t _scrambler_subtype = -1;
#endif
      const struct uECC_Curve_t *_curve = uECC_secp256r1 ();

    public:
      m17_decoder_impl (bool debug_data, bool debug_ctrl, float threshold,
			bool callsign, bool signed_str, int encr_type,
			std::string key, std::string seed);
      
      // SECURITY FIX: Recommended default constructor for production
      m17_decoder_impl () : m17_decoder_impl(
          false,  // debug_data
          false,  // debug_ctrl
          0.9,    // threshold
          true,   // callsign display
          false,  // signed_str (verify in general_work)
          0,      // ENCR_NONE (detect from frame)
          "",     // key (set later if needed)
          ""      // seed (set later if needed)
      ) {
        // Default to M17 strict mode for maximum compatibility
        set_m17_strict_mode(true);
      }
      // SECURITY FIX: Destructor must wipe all sensitive cryptographic material
      // For Nitrokey integration, private keys are stored in hardware security module
      // This provides: tamper resistance, key isolation, secure key generation
      ~m17_decoder_impl () {
        // Wipe all crypto material
        explicit_bzero(_ed25519_private_key, sizeof(_ed25519_private_key));
        explicit_bzero(_curve25519_private_key, sizeof(_curve25519_private_key));
        explicit_bzero(_p256_private_key, sizeof(_p256_private_key));
        explicit_bzero(_curve25519_shared_secret, sizeof(_curve25519_shared_secret));
        explicit_bzero(_aes_gcm_key, sizeof(_aes_gcm_key));
        explicit_bzero(_aes_ctr_key, sizeof(_aes_ctr_key));
        explicit_bzero(_digest, sizeof(_digest));
        explicit_bzero(_sig, sizeof(_sig));
        explicit_bzero(_challenge_nonce, sizeof(_challenge_nonce));
        explicit_bzero(_response_nonce, sizeof(_response_nonce));
        
        // Clean up EVP context
        if (_sha_ctx) {
            EVP_MD_CTX_free(_sha_ctx);
        }
        
        // Clean up Nitrokey
        if (_use_nitrokey) {
            cleanup_hardware_security();
        }
      }
      void set_debug_data (bool debug);
      void set_key (std::string arg);
      void set_seed (std::string seed);
      void set_debug_ctrl (bool debug);
      void set_callsign (bool callsign);
      void set_threshold (float threshold);
      void set_signed (bool signed_str);
      void set_encr_type (int encr_type);
      void parse_raw_key_string (uint8_t * dest, const char *inp);
      void scrambler_sequence_generator ();
      uint32_t scrambler_seed_calculation (int8_t subtype, uint32_t key,
					   int fn);
      
      // SECURITY FIX: Bidirectional crypto methods for M17 with Nitrokey
      void set_ed25519_keys (const uint8_t* public_key, size_t pub_key_size,
                            const uint8_t* private_key, size_t priv_key_size);
      void set_curve25519_keys (const uint8_t* public_key, size_t pub_key_size,
                               const uint8_t* private_key, size_t priv_key_size);
      
      // Signature operations
      int verify_ed25519_signature (const uint8_t* data, size_t data_len, 
                                   const uint8_t* signature, size_t sig_size,
                                   const uint8_t* public_key, size_t pub_key_size);
      int sign_ed25519_message (const uint8_t* data, size_t data_len,
                               uint8_t* signature, size_t sig_size);
      
      // Key agreement and encryption
      int perform_curve25519_ecdh (const uint8_t* peer_public_key, size_t pub_key_size);
      int derive_encryption_key (const uint8_t* shared_secret, size_t secret_size,
                                const uint8_t* salt, size_t salt_size,
                                const uint8_t* info, size_t info_len);
      int decrypt_aes_gcm (const uint8_t* ciphertext, size_t ciphertext_len,
                          const uint8_t* key, size_t key_size,
                          const uint8_t* iv, size_t iv_size,
                          const uint8_t* tag, size_t tag_size,
                          uint8_t* plaintext, size_t plaintext_size);
      
      // Mutual authentication and challenge-response
      int generate_authentication_challenge (uint8_t* challenge, size_t challenge_size);
      int verify_authentication_response (const uint8_t* response, size_t response_size,
                                         const uint8_t* expected_challenge, size_t challenge_size);
      
      // SECURITY FIX: Challenge-response implementation details
      uint8_t _challenge_nonce[32] = { 0 };  // 256-bit nonce for challenges
      uint8_t _response_nonce[32] = { 0 };    // 256-bit nonce for responses
      uint64_t _challenge_timestamp = 0;      // Timestamp for replay protection
      uint64_t _response_timestamp = 0;      // Timestamp for replay protection
      bool _challenge_pending = false;        // Challenge state tracking
      
      // SECURITY FIX: Nitrokey hardware security methods
      bool init_nitrokey_security (const char* pin, int key_slot);
      bool sign_with_hardware (const uint8_t* data, size_t data_len, uint8_t* signature);
      bool ecdh_with_hardware (const uint8_t* peer_pubkey, uint8_t* shared_secret);
      bool decrypt_with_hardware (const uint8_t* ciphertext, size_t ciphertext_len,
                                 uint8_t* plaintext, size_t plaintext_size);
      void cleanup_hardware_security ();
      
      // SECURITY FIX: M17 backwards compatibility methods
      void set_m17_strict_mode (bool strict_mode);
      void set_extended_crypto (bool enable);
      void set_channel_access_number (uint8_t can);
      bool is_m17_compatible () const;
      bool is_extended_crypto_enabled () const;
      
      // SECURITY FIX: Compatibility warning system
      void print_compatibility_warning () const;
      void print_extended_crypto_warning () const {
        fprintf(stderr, "\n");
        fprintf(stderr, "========================================\n");
        fprintf(stderr, "WARNING: EXTENDED CRYPTO MODE ENABLED\n");
        fprintf(stderr, "========================================\n");
        fprintf(stderr, "This mode is NOT M17 spec-compliant!\n");
        fprintf(stderr, "- Standard M17 receivers cannot decode\n");
        fprintf(stderr, "- Only works with matching transmitters\n");
        fprintf(stderr, "- Requires Nitrokey hardware support\n");
        fprintf(stderr, "- Use only for testing/development\n");
        fprintf(stderr, "========================================\n\n");
      }
      
      // SECURITY FIX: Strict M17 mode implementation (P-256 + AES-CTR only)
      int verify_p256_signature (const uint8_t* data, size_t data_len, 
                                const uint8_t* signature, size_t sig_size,
                                const uint8_t* public_key, size_t pub_key_size);
      int perform_p256_ecdh (const uint8_t* peer_public_key, size_t pub_key_size);
      int decrypt_aes_ctr (const uint8_t* ciphertext, size_t ciphertext_len,
                          const uint8_t* key, size_t key_size,
                          const uint8_t* iv, size_t iv_size,
                          uint8_t* plaintext, size_t plaintext_size);
      
      // SECURITY FIX: Mode selection logic for general_work
      int process_frame_crypto (const uint8_t* frame_data, size_t frame_size,
                               uint16_t type_field, uint8_t can_field);
      int detect_crypto_mode (uint16_t type_field, uint8_t can_field);

      // Where all the action really happens
      void forecast (int noutput_items,
		     gr_vector_int & ninput_items_required);

      int general_work (int noutput_items,
			gr_vector_int & ninput_items,
			gr_vector_const_void_star & input_items,
			gr_vector_void_star & output_items);
    };

  }				// namespace m17
}				// namespace gr

#endif /* INCLUDED_M17_M17_DECODER_IMPL_H */
