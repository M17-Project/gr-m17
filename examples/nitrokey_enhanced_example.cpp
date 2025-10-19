//--------------------------------------------------------------------
// M17 C++ library - examples/nitrokey_enhanced_example.cpp
//
// Enhanced example of Nitrokey integration with M17 key management
// Demonstrates key generation, signing, and public key management
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 19 April 2025
//--------------------------------------------------------------------

#include "m17_coder_impl.h"
#include <iostream>
#include <string>
#include <vector>

int main() {
    std::cout << "M17 Enhanced Nitrokey Integration Example\n";
    std::cout << "=========================================\n\n";
    
    try {
        // Create M17 coder instance
        gr::m17::m17_coder_impl encoder(
            "N0CALL",    // src_id
            "W1ABC",     // dst_id
            1,           // mode
            0,           // data
            0,           // encr_type
            0,           // encr_subtype
            0,           // aes_subtype
            0,           // can
            "",          // meta
            "",          // key
            "",          // priv_key
            false,       // debug
            false,       // signed_str
            ""           // seed
        );
        
        std::cout << "M17 Coder initialized successfully\n\n";
        
        // Check Nitrokey status
        std::cout << "Checking Nitrokey status...\n";
        if (!encoder.check_nitrokey_status()) {
            std::cout << "ERROR: Nitrokey not available. Please connect a Nitrokey 3 and install nitropy.\n";
            std::cout << "Installation: pip install pynitrokey\n";
            return 1;
        }
        std::cout << "SUCCESS: Nitrokey is connected and available\n\n";
        
        // List existing keys
        std::cout << "Listing existing Nitrokey keys...\n";
        encoder.list_nitrokey_keys();
        std::cout << "\n";
        
        // Generate a new Ed25519 key on the Nitrokey
        std::string key_label = "M17-Key-" + std::to_string(time(nullptr));
        std::cout << "Generating new Ed25519 key on Nitrokey with label: " << key_label << "\n";
        
        if (!encoder.generate_key_on_nitrokey(key_label)) {
            std::cout << "ERROR: Failed to generate key on Nitrokey\n";
            return 1;
        }
        std::cout << "SUCCESS: Generated Ed25519 key on Nitrokey (private key never left device)\n\n";
        
        // Export the public key
        std::string pubkey_file = "/tmp/m17_pubkey_" + key_label + ".pem";
        std::cout << "Exporting public key to: " << pubkey_file << "\n";
        
        if (!encoder.export_public_key_from_nitrokey(pubkey_file)) {
            std::cout << "ERROR: Failed to export public key\n";
            return 1;
        }
        std::cout << "SUCCESS: Exported public key (others can now verify/encrypt to you)\n\n";
        
        // Import the public key into our key management system
        std::cout << "Importing public key into M17 key management system...\n";
        if (!encoder.import_public_key_from_file("N0CALL", pubkey_file)) {
            std::cout << "ERROR: Failed to import public key\n";
            return 1;
        }
        std::cout << "SUCCESS: Public key imported for callsign N0CALL\n\n";
        
        // Demonstrate signing with Nitrokey
        std::string test_message = "Hello from M17! This message is signed with Nitrokey.";
        std::cout << "Signing message with Nitrokey: \"" << test_message << "\"\n";
        
        uint8_t signature[64];
        if (!encoder.sign_with_nitrokey(
            reinterpret_cast<const uint8_t*>(test_message.c_str()),
            test_message.length(),
            signature
        )) {
            std::cout << "ERROR: Failed to sign message with Nitrokey\n";
            return 1;
        }
        
        std::cout << "SUCCESS: Message signed with Nitrokey\n";
        std::cout << "Signature: ";
        for (int i = 0; i < 64; i++) {
            printf("%02x", signature[i]);
        }
        std::cout << "\n\n";
        
        // Verify the signature using our imported public key
        std::cout << "Verifying signature with imported public key...\n";
        if (encoder.verify_signature_from(
            "N0CALL",
            reinterpret_cast<const uint8_t*>(test_message.c_str()),
            test_message.length(),
            signature
        )) {
            std::cout << "SUCCESS: Signature verified successfully!\n";
        } else {
            std::cout << "ERROR: Signature verification failed\n";
            return 1;
        }
        
        // List all public keys in our system
        std::cout << "\nListing all public keys in M17 system:\n";
        encoder.list_public_keys();
        
        // Demonstrate key management
        std::cout << "\nDemonstrating key management...\n";
        
        // Set a different Nitrokey key (if it exists)
        std::string alt_key = "M17-Alt-Key";
        std::cout << "Attempting to set alternative key: " << alt_key << "\n";
        if (encoder.set_nitrokey_key(alt_key)) {
            std::cout << "SUCCESS: Set alternative key as active\n";
        } else {
            std::cout << "INFO: Alternative key not found, continuing with current key\n";
        }
        
        // Clean up temporary file
        std::string cleanup_cmd = "rm -f " + pubkey_file;
        system(cleanup_cmd.c_str());
        
        std::cout << "\nEnhanced Nitrokey integration example completed successfully!\n";
        std::cout << "\nKey Features Demonstrated:\n";
        std::cout << "- Nitrokey device status checking\n";
        std::cout << "- Ed25519 key generation on device (private key never leaves)\n";
        std::cout << "- Public key export for sharing\n";
        std::cout << "- Message signing with hardware key\n";
        std::cout << "- Signature verification with imported public key\n";
        std::cout << "- Key management and listing\n";
        std::cout << "- Multiple key support\n";
        
    } catch (const std::exception& e) {
        std::cout << "ERROR: Exception occurred: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
