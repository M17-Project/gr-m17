/* -*- c++ -*- */
/*
 * Copyright 2023 jmfriedt.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_M17_M17_CODER_IMPL_H
#define INCLUDED_M17_M17_CODER_IMPL_H

#define AES
#define ECC

#include <gnuradio/m17/m17_coder.h>
#include "m17.h"		// lsf_t declaration

#ifdef AES
#include "aes.h"
#endif

#ifdef ECC
#include "uECC.h"
#endif

// SECURITY FIX: Add OpenSSL includes for secure hashing
#include <openssl/evp.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/err.h>

// SECURITY FIX: Add key management includes
#include <map>
#include <string>
#include <vector>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

// SECURITY FIX: Add constants to replace magic numbers
namespace m17_constants {
    constexpr int MAX_CALLSIGN_LENGTH = 9;
    constexpr int CALLSIGN_BUFFER_SIZE = 10;
    constexpr int FRAME_SIZE = 192;
    constexpr int SYMBOLS_PER_FRAME = 192;
    constexpr int AES_KEY_SIZE = 32;
    constexpr int AES_IV_SIZE = 16;
    
    // Frame number constants
    constexpr uint16_t FRAME_NUMBER_MAX = 0x7FFF;
    constexpr uint16_t SIGNATURE_START_FN = 0x7FFC;
    constexpr uint16_t SIGNATURE_END_FN = 0x7FFF;
    constexpr uint16_t EOT_MARKER = 0x8000;
    constexpr int ECC_PRIVATE_KEY_SIZE = 32;
    constexpr int ECC_PUBLIC_KEY_SIZE = 64;
    constexpr int SEED_SIZE = 3;
    constexpr int HEX_KEY_LENGTH = 64;
    constexpr int HEX_SEED_LENGTH = 6;
    
    // Additional constants for magic numbers
    constexpr int FRAME_NUMBER_BITS = 16;
    constexpr int META_FIELD_SIZE = 14;
    constexpr int LSF_CRC_SIZE = 2;
    constexpr int SCRAMBLER_BYTES_SIZE = 16;
    constexpr int MAX_META_LENGTH = 14;
    constexpr int IV_FRAME_NUMBER_OFFSET = 14;
    constexpr int IV_FRAME_NUMBER_MASK = 0x7F;
    constexpr int FRAME_NUMBER_MASK = 0xFF;
    
    // Key management constants
    constexpr uint32_t PUBLIC_KEY_DB_MAGIC = 0x4D313750;  // "M17P"
    constexpr uint32_t PUBLIC_KEY_DB_VERSION = 1;
    constexpr int ED25519_PUBLIC_KEY_SIZE = 32;
    constexpr int ED25519_SIGNATURE_SIZE = 64;
    constexpr int MAX_CALLSIGN_LEN = 9;
    constexpr int PUBLIC_KEY_RECORD_SIZE = 63;  // callsign(10) + key(32) + valid(1) + imported_date(8) + expiry_date(8)
    
    // ChaCha20-Poly1305 constants
    constexpr int CHACHA20_KEY_SIZE = 32;        // 256-bit key
    constexpr int CHACHA20_IV_SIZE = 12;         // 96-bit IV (recommended for ChaCha20)
    constexpr int CHACHA20_POLY1305_TAG_SIZE = 16; // 128-bit authentication tag
    constexpr int CHACHA20_BLOCK_SIZE = 64;      // ChaCha20 block size
    constexpr int CHACHA20_COUNTER_SIZE = 4;     // 32-bit counter
    
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
    
    // ChaCha20-Poly1305 encryption (extended crypto mode)
    constexpr uint8_t CUSTOM_ENCR_CHACHA20_POLY1305 = 4;  // ChaCha20-Poly1305 (non-standard)
    
    // Use reserved bit in TYPE field for extended crypto signaling
    constexpr uint16_t TYPE_EXTENDED_CRYPTO_BIT = 0x0008;  // Bit 3 (reserved)
    
    // SECURITY FIX: Strict M17 mode constants (P-256 + AES-CTR only)
    constexpr size_t P256_PRIVATE_KEY_SIZE = 32;
    constexpr size_t P256_PUBLIC_KEY_SIZE = 64;
    constexpr size_t P256_SIGNATURE_SIZE = 64;
    constexpr size_t AES_CTR_KEY_SIZE = 32;
    constexpr size_t AES_CTR_IV_SIZE = 16;
} // namespace m17_constants

// SECURITY FIX: PublicKey data structure for key management
struct PublicKey {
    uint8_t key[32];                    // Ed25519 public key
    char callsign[10];                  // Null-terminated callsign
    bool valid;                         // Key validity status
    time_t imported_date;               // Import timestamp
    time_t expiry_date;                 // Expiry timestamp (0 = no expiry)
    
    PublicKey() : valid(false), imported_date(0), expiry_date(0) {
        memset(key, 0, sizeof(key));
        memset(callsign, 0, sizeof(callsign));
    }
    
    ~PublicKey() {
        explicit_bzero(key, sizeof(key));
        explicit_bzero(callsign, sizeof(callsign));
    }
};

// SECURITY FIX: Key management storage paths
struct KeyStoragePaths {
    std::string primary_path;           // /data/m17/
    std::string secondary_path;         // /var/lib/m17/
    std::string fallback_path;         // /tmp/
    std::string db_filename;           // public_keys.db
};

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
 * m17_coder_impl encoder(
 *     false,  // debug_data
 *     false,  // debug_ctrl
 *     0.9,    // threshold
 *     true,   // callsign display
 *     false,  // signed_str (sign in general_work)
 *     0,      // ENCR_NONE (set later if needed)
 *     0,      // encr_subtype
 *     0,      // aes_subtype
 *     0,      // can (Channel Access Number)
 *     "",     // meta
 *     "",     // key (set later if needed)
 *     "",     // priv_key (set later if needed)
 *     false,  // debug
 *     false,  // signed_str
 *     ""      // seed (set later if needed)
 * );
 * 
 * // Default to M17 strict mode
 * encoder.set_m17_strict_mode(true);
 * 
 * // Only enable extended crypto if explicitly requested
 * // encoder.set_extended_crypto(true);  // COMMENTED OUT - requires coordination
 */

namespace gr
{
  namespace m17
  {

    class m17_coder_impl:public m17_coder
    {
    private:
      unsigned char _src_id[10], _dst_id[10];	// 9 character callsign
      int _mode, _data;
      uint16_t _type;
      uint16_t _send_preamble;
      int _encr_subtype;
//encryption
#ifdef AES
//encryption
      int8_t _aes_subtype = -1;
      const struct uECC_Curve_t *_curve = uECC_secp256r1 ();
      encr_t _encr_type= ENCR_NONE;
//AES
      typedef enum
      {
	AES128,
	AES192,
	AES256
      } aes_t;
      uint8_t _key[32];
      uint8_t _iv[16];
      time_t epoch = 1577836800L;	//Jan 1, 2020, 00:00:00 UTC
#endif
      // _can moved to compatibility section below
      lsf_t _lsf, _next_lsf;
        std::string _meta;
      int _got_lsf = 0;
      uint16_t _fn = 0;		//16-bit Frame Number (for the stream mode)
      uint8_t _lich_cnt = 0;	//0..5 LICH counter, derived from the Frame Number
      bool _debug = 0;
      bool _signed_str = false, _finished = false;

      uint8_t _digest[32] = { 0 };	//32-byte field for the SHA-256 digest
      bool _priv_key_loaded = false;	//do we have a sig key loaded?
      uint8_t _priv_key[32] = { 0 };	//private key
      uint8_t _sig[64] = { 0 };	//ECDSA signature
      EVP_MD_CTX *_sha_ctx;		//EVP context for secure hashing
      bool _init_frame;

#ifdef ECC
//Scrambler
      uint8_t _seed[3]; //24-bit is the largest seed value
      uint8_t _scr_bytes[16];
      uint8_t _scrambler_pn[128];
      uint32_t _scrambler_seed = 0;
      int8_t _scrambler_subtype = -1;
#endif

      // SECURITY FIX: Key management system
      std::map<std::string, PublicKey> _public_keys;
      KeyStoragePaths _storage_paths;
      std::string _current_storage_path;
      
      // SECURITY FIX: Nitrokey integration
      bool _nitrokey_available = false;
      std::string _nitrokey_label;
      
      // SECURITY FIX: ChaCha20-Poly1305 encryption support
      uint8_t _chacha20_key[m17_constants::CHACHA20_KEY_SIZE] = { 0 };
      uint8_t _chacha20_iv[m17_constants::CHACHA20_IV_SIZE] = { 0 };
      uint8_t _chacha20_tag[m17_constants::CHACHA20_POLY1305_TAG_SIZE] = { 0 };
      bool _chacha20_available = false;

    public:
      void parse_raw_key_string (uint8_t *, const char *);
      void scrambler_sequence_generator ();
      void set_src_id (std::string src_id);
      void set_dst_id (std::string dst_id);
      void set_key (std::string key);
      void set_priv_key (std::string key);
      void set_meta (std::string meta);
      void set_seed (std::string meta);
      void set_type (int mode, int data, encr_t encr_type, int encr_subtype,
		     int can);
      void set_mode (int mode);
      void set_data (int data);
      void set_encr_type (int encr_type);
      void set_encr_subtype (int encr_subtype);
      void set_aes_subtype (int aes_subtype, int encr_type);
      void set_can (int can);
      void set_debug (bool debug);
      void set_signed (bool signed_str);
      void end_of_transmission(const pmt::pmt_t& msg);

      m17_coder_impl (std::string src_id, std::string dst_id, int mode,
		      int data, int encr_type, int encr_subtype, int aes_subtype, int can,
		      std::string meta, std::string key, std::string priv_key,
		      bool debug, bool signed_str, std::string seed);
      ~m17_coder_impl ();

      // SECURITY FIX: M17 backwards compatibility
      bool _m17_strict_mode = true;  // Default to M17 spec compliance
      bool _extended_crypto_enabled = false;  // Extended crypto features
      uint8_t _can = 0;  // Channel Access Number for compatibility
      
      // SECURITY FIX: M17 backwards compatibility methods
      void set_m17_strict_mode (bool strict_mode);
      void set_extended_crypto (bool enable);
      void set_channel_access_number (uint8_t can);
      bool is_m17_compatible () const;
      bool is_extended_crypto_enabled () const;
      
      // SECURITY FIX: Compatibility warning system
      void print_compatibility_warning () const;
      void print_extended_crypto_warning () const;
      
      // SECURITY FIX: Strict M17 mode implementation (P-256 + AES-CTR only)
      int sign_p256_message (const uint8_t* data, size_t data_len, uint8_t* signature);
      int perform_p256_ecdh (const uint8_t* peer_public_key, size_t pub_key_size);
      int encrypt_aes_ctr (const uint8_t* plaintext, size_t plaintext_len,
                          const uint8_t* key, size_t key_size,
                          const uint8_t* iv, size_t iv_size,
                          uint8_t* ciphertext, size_t ciphertext_size);
      
      // SECURITY FIX: ChaCha20-Poly1305 encryption support
      int encrypt_chacha20_poly1305 (const uint8_t* plaintext, size_t plaintext_len,
                                    const uint8_t* key, size_t key_size,
                                    const uint8_t* iv, size_t iv_size,
                                    const uint8_t* aad, size_t aad_len,
                                    uint8_t* ciphertext, size_t ciphertext_size,
                                    uint8_t* tag, size_t tag_size);
      int decrypt_chacha20_poly1305 (const uint8_t* ciphertext, size_t ciphertext_len,
                                    const uint8_t* key, size_t key_size,
                                    const uint8_t* iv, size_t iv_size,
                                    const uint8_t* aad, size_t aad_len,
                                    const uint8_t* tag, size_t tag_size,
                                    uint8_t* plaintext, size_t plaintext_size);
      bool set_chacha20_key (const std::string& hex_key);
      bool set_chacha20_iv (const std::string& hex_iv);
      void generate_chacha20_iv ();
      bool is_chacha20_available () const;

      // SECURITY FIX: Key management system functions
      
      // Nitrokey Integration
      bool generate_key_on_nitrokey(const std::string& label);
      bool export_public_key_from_nitrokey(const std::string& file);
      bool sign_with_nitrokey(const uint8_t* data, size_t data_len, uint8_t* signature);
      bool list_nitrokey_keys();
      bool check_nitrokey_status();
      bool delete_nitrokey_key(const std::string& label);
      bool set_nitrokey_key(const std::string& label);
      
      // Enhanced Nitrokey PIN Authentication
      enum class NitrokeyStatus {
        DEVICE_NOT_FOUND,    // Device not connected
        PIN_REQUIRED,        // Device connected but PIN authentication needed
        AUTHENTICATED,       // Device connected and PIN authenticated
        ERROR               // Other error condition
      };
      
      NitrokeyStatus check_nitrokey_pin_status();
      bool attempt_nitrokey_pin_authentication();
      void report_nitrokey_status();
      bool handle_nitrokey_pin_authentication();
      
      // Public Key Import
      bool import_public_key(const std::string& callsign, const std::string& hex_key);
      bool import_public_key_from_file(const std::string& callsign, const std::string& pem_file);
      bool import_public_key_from_nitrokey(const std::string& callsign);
      
      // Verification
      bool verify_signature_from(const std::string& callsign, const uint8_t* data, 
                                size_t data_len, const uint8_t* signature);
      
      // Storage
      bool save_public_keys_to_disk();
      bool load_public_keys_from_disk();
      bool load_trusted_keys_from_file(const std::string& config_file);
      
      // Management
      void list_public_keys();
      bool remove_public_key(const std::string& callsign);
      void check_expired_keys();
      
      // Internal key management functions
      void init_key_management();
      bool detect_storage_path();
      bool validate_callsign(const std::string& callsign);
      bool validate_hex_key(const std::string& hex_key);
      std::string get_key_fingerprint(const uint8_t* key);
      void secure_wipe_key(PublicKey& key);
      
      // SECURITY FIX: Input validation functions
      bool validate_nitrokey_label(const std::string& label);
      std::string sanitize_shell_input(const std::string& input);
      
      // SECURITY FIX: Secure command execution
      int secure_execute_command(const std::vector<std::string>& args);
      bool execute_nitropy_command(const std::vector<std::string>& args, std::string& output);
      
      // SECURITY FIX: Memory protection and encryption
      class SecureKeyStorage {
      private:
          uint8_t* _encrypted_key;
          size_t _key_size;
          uint8_t _encryption_key[32];
          bool _is_encrypted;
          
      public:
          SecureKeyStorage();
          ~SecureKeyStorage();
          
          bool store_key(const uint8_t* key, size_t size);
          bool retrieve_key(uint8_t* key, size_t size);
          void clear_key();
          bool is_encrypted() const { return _is_encrypted; }
      };
      
      // SECURITY FIX: Process isolation for key operations
      class KeyIsolationManager {
      private:
          pid_t _key_process_pid;
          int _communication_pipe[2];
          bool _isolation_active;
          
      public:
          KeyIsolationManager();
          ~KeyIsolationManager();
          
          bool start_key_isolation();
          bool stop_key_isolation();
          bool execute_secure_key_operation(const std::string& operation, const uint8_t* data, size_t data_size, uint8_t* result, size_t result_size);
          bool is_isolation_active() const { return _isolation_active; }
      };
      
      // SECURITY FIX: Memory encryption functions
      bool encrypt_key_in_memory(uint8_t* key, size_t key_size, const uint8_t* encryption_key);
      bool decrypt_key_in_memory(uint8_t* key, size_t key_size, const uint8_t* encryption_key);
      bool generate_encryption_key(uint8_t* encryption_key, size_t key_size);
      void secure_wipe_memory(void* ptr, size_t size);
      
      // SECURITY FIX: Enhanced key management
      SecureKeyStorage _secure_private_key;
      SecureKeyStorage _secure_encryption_key;
      KeyIsolationManager _key_isolation;

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

#endif /* INCLUDED_M17_M17_CODER_IMPL_H */
