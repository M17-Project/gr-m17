#include <iostream>
#include <gnuradio/m17/m17_coder.h>

/**
 * Enhanced Nitrokey PIN Authentication Example
 * 
 * This example demonstrates the enhanced PIN authentication features
 * for Nitrokey devices in the M17 GNU Radio module.
 */

int main() {
    std::cout << "=== Enhanced Nitrokey PIN Authentication Example ===" << std::endl;
    
    // Initialize M17 coder
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
    
    std::cout << "\n1. Checking Nitrokey Status..." << std::endl;
    // This will now provide detailed status information
    coder->check_nitrokey_status();
    
    std::cout << "\n2. Reporting Detailed Nitrokey Status..." << std::endl;
    // This provides comprehensive status reporting
    coder->report_nitrokey_status();
    
    std::cout << "\n3. Attempting PIN Authentication..." << std::endl;
    // This will handle PIN authentication if required
    if (coder->attempt_nitrokey_pin_authentication()) {
        std::cout << "SUCCESS: PIN authentication successful" << std::endl;
    } else {
        std::cout << "FAILED: PIN authentication failed" << std::endl;
        std::cout << "Please check your PIN and try again" << std::endl;
        return 1;
    }
    
    std::cout << "\n4. Listing Available Keys..." << std::endl;
    if (coder->list_nitrokey_keys()) {
        std::cout << "SUCCESS: Keys listed successfully" << std::endl;
    } else {
        std::cout << "FAILED: Could not list keys" << std::endl;
        return 1;
    }
    
    std::cout << "\n5. Setting Active Key..." << std::endl;
    // This will now handle PIN authentication automatically
    if (coder->set_nitrokey_key("M17-Key")) {
        std::cout << "SUCCESS: Key set successfully" << std::endl;
    } else {
        std::cout << "FAILED: Could not set key" << std::endl;
        std::cout << "Please ensure the key exists and your PIN is correct" << std::endl;
        return 1;
    }
    
    std::cout << "\n6. Testing Signing Operation..." << std::endl;
    // Prepare test data
    const char* test_data = "Hello, M17 with Nitrokey!";
    size_t data_len = strlen(test_data);
    uint8_t signature[64];
    
    // This will now handle PIN authentication automatically
    if (coder->sign_with_nitrokey((const uint8_t*)test_data, data_len, signature)) {
        std::cout << "SUCCESS: Data signed with Nitrokey" << std::endl;
        std::cout << "Signature: ";
        for (int i = 0; i < 64; i++) {
            printf("%02x", signature[i]);
        }
        std::cout << std::endl;
    } else {
        std::cout << "FAILED: Could not sign data" << std::endl;
        std::cout << "Please check your Nitrokey configuration" << std::endl;
        return 1;
    }
    
    std::cout << "\n7. Final Status Report..." << std::endl;
    coder->report_nitrokey_status();
    
    std::cout << "\n=== Example Completed Successfully ===" << std::endl;
    return 0;
}
