//--------------------------------------------------------------------
// M17 C library - crypto/nitrokey_openpgp.c
//
// Nitrokey OpenPGP integration implementation for M17 digital radio protocol
// Provides hardware-based OpenPGP operations using Nitrokey devices
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 21 October 2025
//--------------------------------------------------------------------

#include "nitrokey_openpgp.h"
#include "m17.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>

// Nitropy command
#define NITROPY_CMD "nitropy"

// Nitrokey OpenPGP integration state
static struct {
    bool is_initialized;
    bool nitrokey_available;
    char default_key_name[64];
} g_nitrokey_openpgp_state = {0};

// Execute nitropy command and capture output
static m17_nitrokey_openpgp_status_t execute_nitropy_command(const char* args, 
                                                           char* output, 
                                                           size_t output_size,
                                                           const char* input,
                                                           size_t input_size) {
    if (!g_nitrokey_openpgp_state.nitrokey_available) {
        return M17_NITROKEY_OPENPGP_ERROR_NITROPY_NOT_AVAILABLE;
    }
    
    // Build command
    char command[1024];
    snprintf(command, sizeof(command), "%s %s", NITROPY_CMD, args);
    
    // Create pipes for communication
    int pipe_in[2], pipe_out[2];
    if (pipe(pipe_in) == -1 || pipe(pipe_out) == -1) {
        return M17_NITROKEY_OPENPGP_ERROR_OPERATION_FAILED;
    }
    
    pid_t pid = fork();
    if (pid == -1) {
        close(pipe_in[0]);
        close(pipe_in[1]);
        close(pipe_out[0]);
        close(pipe_out[1]);
        return M17_NITROKEY_OPENPGP_ERROR_OPERATION_FAILED;
    }
    
    if (pid == 0) {
        // Child process
        close(pipe_in[1]);
        close(pipe_out[0]);
        
        dup2(pipe_in[0], STDIN_FILENO);
        dup2(pipe_out[1], STDOUT_FILENO);
        dup2(pipe_out[1], STDERR_FILENO);
        
        close(pipe_in[0]);
        close(pipe_out[1]);
        
        execl("/bin/sh", "sh", "-c", command, NULL);
        exit(1);
    } else {
        // Parent process
        close(pipe_in[0]);
        close(pipe_out[1]);
        
        // Write input if provided
        if (input && input_size > 0) {
            ssize_t bytes_written = write(pipe_in[1], input, input_size);
            if (bytes_written != (ssize_t)input_size) {
                close(pipe_in[1]);
                close(pipe_out[0]);
                return M17_NITROKEY_OPENPGP_ERROR_OPERATION_FAILED;
            }
        }
        close(pipe_in[1]);
        
        // Read output
        ssize_t bytes_read = read(pipe_out[0], output, output_size - 1);
        close(pipe_out[0]);
        
        int status;
        waitpid(pid, &status, 0);
        
        if (bytes_read > 0) {
            output[bytes_read] = '\0';
        } else {
            output[0] = '\0';
        }
        
        return (WEXITSTATUS(status) == 0) ? M17_NITROKEY_OPENPGP_SUCCESS : M17_NITROKEY_OPENPGP_ERROR_OPERATION_FAILED;
    }
}

// Initialize Nitrokey OpenPGP integration
m17_nitrokey_openpgp_status_t m17_nitrokey_openpgp_init(void) {
    if (g_nitrokey_openpgp_state.is_initialized) {
        return M17_NITROKEY_OPENPGP_SUCCESS;
    }
    
    // Common nitropy executable paths across different Linux distributions
    const char* nitropy_paths[] = {
        // Standard system paths
        "/usr/bin/nitropy",           // Most Linux distributions
        "/usr/local/bin/nitropy",     // Local installations
        "/opt/nitrokey/bin/nitropy",  // Some package managers
        "/snap/bin/nitropy",          // Snap packages
        "/flatpak/bin/nitropy",       // Flatpak packages
        // AppImage paths
        "/usr/local/bin/nitropy.appimage",
        // Home directory installations (will be handled by which command)
        // Alternative locations
        "/bin/nitropy",               // Some minimal systems
        NULL
    };
    
    bool nitropy_found = false;
    
    // Try each path
    for (int i = 0; nitropy_paths[i] != NULL; i++) {
        if (access(nitropy_paths[i], X_OK) == 0) {
            nitropy_found = true;
            break;
        }
    }
    
    // Try to find nitropy in PATH using which command
    if (!nitropy_found) {
        FILE* fp = popen("which nitropy 2>/dev/null", "r");
        if (fp != NULL) {
            char buffer[512];
            if (fgets(buffer, sizeof(buffer), fp) != NULL) {
                // Remove newline
                buffer[strcspn(buffer, "\n")] = '\0';
                if (strlen(buffer) > 0 && access(buffer, X_OK) == 0) {
                    nitropy_found = true;
                }
            }
            pclose(fp);
        }
    }
    
    // Try nitropy in PATH (fallback)
    if (!nitropy_found && access("nitropy", X_OK) == 0) {
        nitropy_found = true;
    }
    
    if (!nitropy_found) {
        fprintf(stderr, "M17_NITROKEY_OPENPGP_ERROR: nitropy not found. Please install Nitrokey CLI:\n");
        fprintf(stderr, "  Ubuntu/Debian: sudo apt install nitrokey-app\n");
        fprintf(stderr, "  Fedora/RHEL: sudo dnf install nitrokey-app\n");
        fprintf(stderr, "  Arch Linux: sudo pacman -S nitrokey-app\n");
        fprintf(stderr, "  Or download from: https://github.com/Nitrokey/nitropy\n");
        return M17_NITROKEY_OPENPGP_ERROR_NITROPY_NOT_AVAILABLE;
    }
    
    // Test nitropy connectivity
    char test_output[256];
    m17_nitrokey_openpgp_status_t status = execute_nitropy_command("nk3 list", test_output, sizeof(test_output), NULL, 0);
    if (status != M17_NITROKEY_OPENPGP_SUCCESS) {
        return M17_NITROKEY_OPENPGP_ERROR_DEVICE_NOT_FOUND;
    }
    
    g_nitrokey_openpgp_state.nitrokey_available = true;
    g_nitrokey_openpgp_state.is_initialized = true;
    
    return M17_NITROKEY_OPENPGP_SUCCESS;
}

// Check if Nitrokey is available and accessible
m17_nitrokey_openpgp_status_t m17_nitrokey_openpgp_check_device(void) {
    if (!g_nitrokey_openpgp_state.is_initialized) {
        m17_nitrokey_openpgp_status_t status = m17_nitrokey_openpgp_init();
        if (status != M17_NITROKEY_OPENPGP_SUCCESS) {
            return status;
        }
    }
    
    return g_nitrokey_openpgp_state.nitrokey_available ? M17_NITROKEY_OPENPGP_SUCCESS : M17_NITROKEY_OPENPGP_ERROR_DEVICE_NOT_FOUND;
}

// List OpenPGP keys stored on Nitrokey
m17_nitrokey_openpgp_status_t m17_nitrokey_openpgp_list_keys(m17_nitrokey_openpgp_key_t* keys,
                                                            size_t max_keys,
                                                            size_t* key_count) {
    if (!keys || !key_count) {
        return M17_NITROKEY_OPENPGP_ERROR_INVALID_PARAM;
    }
    
    char output[4096];
    m17_nitrokey_openpgp_status_t status = execute_nitropy_command("nk3 secrets list", 
                                                                 output, sizeof(output), NULL, 0);
    if (status != M17_NITROKEY_OPENPGP_SUCCESS) {
        return status;
    }
    
    *key_count = 0;
    char* line = strtok(output, "\n");
    
    while (line && *key_count < max_keys) {
        // Parse nitropy output to extract key information
        if (strstr(line, "OpenPGP") || strstr(line, "Ed25519") || strstr(line, "RSA")) {
            m17_nitrokey_openpgp_key_t* key = &keys[*key_count];
            
            // Extract key name (first field)
            char* key_name = strtok(line, " \t");
            if (key_name) {
                strncpy(key->key_name, key_name, sizeof(key->key_name) - 1);
                key->key_name[sizeof(key->key_name) - 1] = '\0';
            }
            
            // Determine key type
            key->is_ed25519 = strstr(line, "Ed25519") != NULL;
            key->is_rsa = strstr(line, "RSA") != NULL;
            
            // Set default key size
            key->key_size = key->is_ed25519 ? 256 : (key->is_rsa ? 2048 : 0);
            
            // Set creation time
            key->creation_time = (uint32_t)time(NULL);
            
            // Initialize other fields
            strcpy(key->key_id, "UNKNOWN");
            strcpy(key->fingerprint, "UNKNOWN");
            strcpy(key->user_id, "UNKNOWN");
            
            (*key_count)++;
        }
        line = strtok(NULL, "\n");
    }
    
    return M17_NITROKEY_OPENPGP_SUCCESS;
}

// Generate Ed25519 OpenPGP key on Nitrokey
m17_nitrokey_openpgp_status_t m17_nitrokey_openpgp_generate_ed25519_key(const char* key_name,
                                                                       const char* user_id,
                                                                       const char* passphrase) {
    if (!key_name || !user_id) {
        return M17_NITROKEY_OPENPGP_ERROR_INVALID_PARAM;
    }
    
    char args[512];
    snprintf(args, sizeof(args), "nk3 secrets add-password --name \"%s\" --algorithm ed25519", key_name);
    
    char output[1024];
    m17_nitrokey_openpgp_status_t status = execute_nitropy_command(args, output, sizeof(output), NULL, 0);
    
    if (status == M17_NITROKEY_OPENPGP_SUCCESS) {
        // Set as default key if none is set
        if (strlen(g_nitrokey_openpgp_state.default_key_name) == 0) {
            strncpy(g_nitrokey_openpgp_state.default_key_name, key_name, 
                   sizeof(g_nitrokey_openpgp_state.default_key_name) - 1);
            g_nitrokey_openpgp_state.default_key_name[sizeof(g_nitrokey_openpgp_state.default_key_name) - 1] = '\0';
        }
    }
    
    return status;
}

// Generate RSA OpenPGP key on Nitrokey
m17_nitrokey_openpgp_status_t m17_nitrokey_openpgp_generate_rsa_key(const char* key_name,
                                                                  const char* user_id,
                                                                  const char* passphrase,
                                                                  uint32_t key_size) {
    if (!key_name || !user_id) {
        return M17_NITROKEY_OPENPGP_ERROR_INVALID_PARAM;
    }
    
    char args[512];
    snprintf(args, sizeof(args), "nk3 secrets add-password --name \"%s\" --algorithm rsa --key-size %u", 
             key_name, key_size);
    
    char output[1024];
    m17_nitrokey_openpgp_status_t status = execute_nitropy_command(args, output, sizeof(output), NULL, 0);
    
    if (status == M17_NITROKEY_OPENPGP_SUCCESS) {
        // Set as default key if none is set
        if (strlen(g_nitrokey_openpgp_state.default_key_name) == 0) {
            strncpy(g_nitrokey_openpgp_state.default_key_name, key_name, 
                   sizeof(g_nitrokey_openpgp_state.default_key_name) - 1);
            g_nitrokey_openpgp_state.default_key_name[sizeof(g_nitrokey_openpgp_state.default_key_name) - 1] = '\0';
        }
    }
    
    return status;
}

// Export OpenPGP public key from Nitrokey
m17_nitrokey_openpgp_status_t m17_nitrokey_openpgp_export_public_key(const char* key_name,
                                                                    char* armored_key,
                                                                    size_t armored_key_size) {
    if (!key_name || !armored_key) {
        return M17_NITROKEY_OPENPGP_ERROR_INVALID_PARAM;
    }
    
    char args[256];
    snprintf(args, sizeof(args), "nk3 secrets get-public-key --name \"%s\"", key_name);
    
    return execute_nitropy_command(args, armored_key, armored_key_size, NULL, 0);
}

// Import OpenPGP public key to Nitrokey
m17_nitrokey_openpgp_status_t m17_nitrokey_openpgp_import_public_key(const char* key_name,
                                                                    const char* armored_key,
                                                                    size_t armored_key_size) {
    if (!key_name || !armored_key) {
        return M17_NITROKEY_OPENPGP_ERROR_INVALID_PARAM;
    }
    
    // Create temporary file for public key
    char temp_file[] = "/tmp/m17_nitrokey_pubkey_XXXXXX";
    int temp_fd = mkstemp(temp_file);
    if (temp_fd == -1) {
        return M17_NITROKEY_OPENPGP_ERROR_OPERATION_FAILED;
    }
    
    ssize_t bytes_written = write(temp_fd, armored_key, armored_key_size);
    close(temp_fd);
    
    if (bytes_written != (ssize_t)armored_key_size) {
        unlink(temp_file);
        return M17_NITROKEY_OPENPGP_ERROR_OPERATION_FAILED;
    }
    
    char args[512];
    snprintf(args, sizeof(args), "nk3 secrets import-public-key --name \"%s\" --file %s", 
             key_name, temp_file);
    
    char output[1024];
    m17_nitrokey_openpgp_status_t status = execute_nitropy_command(args, output, sizeof(output), NULL, 0);
    
    // Clean up temporary file
    unlink(temp_file);
    
    return status;
}

// Sign message using Nitrokey OpenPGP key
m17_nitrokey_openpgp_status_t m17_nitrokey_openpgp_sign_message(const char* message,
                                                              size_t message_len,
                                                              const char* key_name,
                                                              m17_openpgp_sig_type_t sig_type,
                                                              m17_openpgp_signature_t* signature) {
    if (!message || !signature || !key_name) {
        return M17_NITROKEY_OPENPGP_ERROR_INVALID_PARAM;
    }
    
    // Create temporary file for message
    char temp_msg[] = "/tmp/m17_nitrokey_msg_XXXXXX";
    int msg_fd = mkstemp(temp_msg);
    if (msg_fd == -1) {
        return M17_NITROKEY_OPENPGP_ERROR_OPERATION_FAILED;
    }
    
    ssize_t msg_bytes = write(msg_fd, message, message_len);
    close(msg_fd);
    
    if (msg_bytes != (ssize_t)message_len) {
        unlink(temp_msg);
        return M17_NITROKEY_OPENPGP_ERROR_OPERATION_FAILED;
    }
    
    // Create temporary file for signature
    char temp_sig[] = "/tmp/m17_nitrokey_sig_XXXXXX";
    int sig_fd = mkstemp(temp_sig);
    if (sig_fd == -1) {
        unlink(temp_msg);
        return M17_NITROKEY_OPENPGP_ERROR_OPERATION_FAILED;
    }
    close(sig_fd);
    
    // Sign message using Nitrokey
    char args[512];
    snprintf(args, sizeof(args), "nk3 secrets sign --name \"%s\" --input %s --output %s", 
             key_name, temp_msg, temp_sig);
    
    char output[1024];
    m17_nitrokey_openpgp_status_t status = execute_nitropy_command(args, output, sizeof(output), NULL, 0);
    
    if (status == M17_NITROKEY_OPENPGP_SUCCESS) {
        // Read signature from file
        FILE* sig_file = fopen(temp_sig, "r");
        if (sig_file) {
            size_t bytes_read = fread(signature->signature_armored, 1, 
                                    sizeof(signature->signature_armored) - 1, sig_file);
            signature->signature_armored[bytes_read] = '\0';
            signature->signature_size = bytes_read;
            fclose(sig_file);
        }
        
        strncpy(signature->key_id, key_name, sizeof(signature->key_id) - 1);
        signature->key_id[sizeof(signature->key_id) - 1] = '\0';
        signature->creation_time = (uint32_t)time(NULL);
        signature->sig_type = sig_type;
    }
    
    // Clean up temporary files
    unlink(temp_msg);
    unlink(temp_sig);
    
    return status;
}

// Sign email using Nitrokey OpenPGP key
m17_nitrokey_openpgp_status_t m17_nitrokey_openpgp_sign_email(const char* email_content,
                                                            size_t email_len,
                                                            const char* key_name,
                                                            m17_openpgp_sig_type_t sig_type,
                                                            m17_openpgp_signature_t* signature) {
    // For email signing, we use text mode by default
    m17_openpgp_sig_type_t email_sig_type = (sig_type == M17_OPENPGP_SIG_BINARY) ? 
                                           M17_OPENPGP_SIG_TEXT : sig_type;
    
    return m17_nitrokey_openpgp_sign_message(email_content, email_len, key_name, email_sig_type, signature);
}

// Create detached signature using Nitrokey
m17_nitrokey_openpgp_status_t m17_nitrokey_openpgp_create_detached_signature(const char* file_path,
                                                                           const char* key_name,
                                                                           const char* output_path) {
    if (!file_path || !key_name || !output_path) {
        return M17_NITROKEY_OPENPGP_ERROR_INVALID_PARAM;
    }
    
    char args[512];
    snprintf(args, sizeof(args), "nk3 secrets sign --name \"%s\" --input %s --output %s", 
             key_name, file_path, output_path);
    
    char output[1024];
    return execute_nitropy_command(args, output, sizeof(output), NULL, 0);
}

// Verify signature using Nitrokey public key
m17_nitrokey_openpgp_status_t m17_nitrokey_openpgp_verify_signature(const char* message,
                                                                   size_t message_len,
                                                                   const char* signature,
                                                                   size_t signature_len,
                                                                   const char* key_name,
                                                                   m17_openpgp_verification_t* verification) {
    if (!message || !signature || !verification || !key_name) {
        return M17_NITROKEY_OPENPGP_ERROR_INVALID_PARAM;
    }
    
    // Create temporary files
    char temp_msg[] = "/tmp/m17_nitrokey_verify_msg_XXXXXX";
    char temp_sig[] = "/tmp/m17_nitrokey_verify_sig_XXXXXX";
    
    int msg_fd = mkstemp(temp_msg);
    int sig_fd = mkstemp(temp_sig);
    
    if (msg_fd == -1 || sig_fd == -1) {
        if (msg_fd != -1) close(msg_fd);
        if (sig_fd != -1) close(sig_fd);
        return M17_NITROKEY_OPENPGP_ERROR_OPERATION_FAILED;
    }
    
    ssize_t msg_bytes = write(msg_fd, message, message_len);
    ssize_t sig_bytes = write(sig_fd, signature, signature_len);
    close(msg_fd);
    close(sig_fd);
    
    if (msg_bytes != (ssize_t)message_len || sig_bytes != (ssize_t)signature_len) {
        unlink(temp_msg);
        unlink(temp_sig);
        return M17_NITROKEY_OPENPGP_ERROR_OPERATION_FAILED;
    }
    
    // Verify signature
    char args[512];
    snprintf(args, sizeof(args), "nk3 secrets verify --name \"%s\" --input %s --signature %s", 
             key_name, temp_msg, temp_sig);
    
    char output[1024];
    m17_nitrokey_openpgp_status_t status = execute_nitropy_command(args, output, sizeof(output), NULL, 0);
    
    if (status == M17_NITROKEY_OPENPGP_SUCCESS) {
        verification->is_valid = true;
        strncpy(verification->key_id, key_name, sizeof(verification->key_id) - 1);
        verification->key_id[sizeof(verification->key_id) - 1] = '\0';
        verification->creation_time = (uint32_t)time(NULL);
        verification->sig_type = M17_OPENPGP_SIG_BINARY;
    } else {
        verification->is_valid = false;
        strncpy(verification->error_message, "Signature verification failed", 
                sizeof(verification->error_message) - 1);
        verification->error_message[sizeof(verification->error_message) - 1] = '\0';
    }
    
    // Clean up temporary files
    unlink(temp_msg);
    unlink(temp_sig);
    
    return status;
}

// Set default Nitrokey OpenPGP key for operations
m17_nitrokey_openpgp_status_t m17_nitrokey_openpgp_set_default_key(const char* key_name) {
    if (!key_name) {
        return M17_NITROKEY_OPENPGP_ERROR_INVALID_PARAM;
    }
    
    strncpy(g_nitrokey_openpgp_state.default_key_name, key_name, 
           sizeof(g_nitrokey_openpgp_state.default_key_name) - 1);
    g_nitrokey_openpgp_state.default_key_name[sizeof(g_nitrokey_openpgp_state.default_key_name) - 1] = '\0';
    
    return M17_NITROKEY_OPENPGP_SUCCESS;
}

// Get default Nitrokey OpenPGP key
m17_nitrokey_openpgp_status_t m17_nitrokey_openpgp_get_default_key(char* key_name, size_t key_name_size) {
    if (!key_name || key_name_size == 0) {
        return M17_NITROKEY_OPENPGP_ERROR_INVALID_PARAM;
    }
    
    strncpy(key_name, g_nitrokey_openpgp_state.default_key_name, key_name_size - 1);
    key_name[key_name_size - 1] = '\0';
    
    return M17_NITROKEY_OPENPGP_SUCCESS;
}

// Delete OpenPGP key from Nitrokey
m17_nitrokey_openpgp_status_t m17_nitrokey_openpgp_delete_key(const char* key_name) {
    if (!key_name) {
        return M17_NITROKEY_OPENPGP_ERROR_INVALID_PARAM;
    }
    
    char args[256];
    snprintf(args, sizeof(args), "nk3 secrets delete --name \"%s\"", key_name);
    
    char output[1024];
    return execute_nitropy_command(args, output, sizeof(output), NULL, 0);
}

// Get Nitrokey device information
m17_nitrokey_openpgp_status_t m17_nitrokey_openpgp_get_device_info(char* device_info, size_t info_size) {
    if (!device_info) {
        return M17_NITROKEY_OPENPGP_ERROR_INVALID_PARAM;
    }
    
    char args[] = "nk3 info";
    return execute_nitropy_command(args, device_info, info_size, NULL, 0);
}

// Cleanup Nitrokey OpenPGP integration
void m17_nitrokey_openpgp_cleanup(void) {
    g_nitrokey_openpgp_state.is_initialized = false;
    g_nitrokey_openpgp_state.nitrokey_available = false;
    memset(g_nitrokey_openpgp_state.default_key_name, 0, sizeof(g_nitrokey_openpgp_state.default_key_name));
}
