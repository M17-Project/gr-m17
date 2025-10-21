//--------------------------------------------------------------------
// M17 C library - crypto/openpgp_integration.c
//
// OpenPGP integration implementation for M17 digital radio protocol
// Provides GnuPG/OpenPGP integration for message and email signing
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 21 October 2025
//--------------------------------------------------------------------

#include "openpgp_integration.h"
#include "m17.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>

// Detect Linux distribution for better error messages
static const char* detect_linux_distribution(void) {
    FILE* fp = fopen("/etc/os-release", "r");
    if (fp == NULL) {
        return "Unknown";
    }
    
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "PRETTY_NAME=", 12) == 0) {
            fclose(fp);
            // Remove quotes and newline
            char* start = strchr(line, '"');
            if (start) {
                start++;
                char* end = strrchr(start, '"');
                if (end) {
                    *end = '\0';
                    // Return a static string to avoid memory management issues
                    static char distro_name[256];
                    strncpy(distro_name, start, sizeof(distro_name) - 1);
                    distro_name[sizeof(distro_name) - 1] = '\0';
                    return distro_name;
                }
            }
        }
    }
    fclose(fp);
    return "Unknown";
}

// Detect GNU Radio installation method and paths
static void detect_gnuradio_installation(char* gnuradio_path, size_t path_size) {
    // Check for PyBOMBS installation first
    const char* pybombs_prefix = getenv("PYBOMBS_PREFIX");
    if (pybombs_prefix != NULL) {
        snprintf(gnuradio_path, path_size, "%s/bin/gnuradio-companion", pybombs_prefix);
        if (access(gnuradio_path, X_OK) == 0) {
            return;
        }
    }
    
    // Check standard system paths
    const char* gnuradio_paths[] = {
        "/usr/bin/gnuradio-companion",           // Standard system installation
        "/usr/local/bin/gnuradio-companion",      // Local installation
        "/snap/gnuradio/current/bin/gnuradio-companion",  // Snap package
        "/var/lib/flatpak/app/org.gnuradio.GnuRadio/current/active/files/bin/gnuradio-companion",  // Flatpak
        "/opt/gnuradio/bin/gnuradio-companion",  // Custom installation
        NULL
    };
    
    for (int i = 0; gnuradio_paths[i] != NULL; i++) {
        if (access(gnuradio_paths[i], X_OK) == 0) {
            strncpy(gnuradio_path, gnuradio_paths[i], path_size - 1);
            gnuradio_path[path_size - 1] = '\0';
            return;
        }
    }
    
    // Try to find gnuradio-companion in PATH
    FILE* fp = popen("which gnuradio-companion 2>/dev/null", "r");
    if (fp != NULL) {
        char buffer[512];
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
            buffer[strcspn(buffer, "\n")] = '\0';
            if (strlen(buffer) > 0 && access(buffer, X_OK) == 0) {
                strncpy(gnuradio_path, buffer, path_size - 1);
                gnuradio_path[path_size - 1] = '\0';
                pclose(fp);
                return;
            }
        }
        pclose(fp);
    }
    
    // Default to not found
    gnuradio_path[0] = '\0';
}

// Detect GNU Radio Python module paths
static void detect_gnuradio_python_paths(char* python_path, size_t path_size) {
    // Check for PyBOMBS installation first
    const char* pybombs_prefix = getenv("PYBOMBS_PREFIX");
    if (pybombs_prefix != NULL) {
        snprintf(python_path, path_size, "%s/lib/python3.*/site-packages/gnuradio", pybombs_prefix);
        return;
    }
    
    // Check standard system paths
    const char* python_paths[] = {
        "/usr/lib/python3.*/site-packages/gnuradio",           // Standard system installation
        "/usr/local/lib/python3.*/site-packages/gnuradio",     // Local installation
        "/snap/gnuradio/current/lib/python3.*/site-packages/gnuradio",  // Snap package
        "/var/lib/flatpak/app/org.gnuradio.GnuRadio/current/active/files/lib/python3.*/site-packages/gnuradio",  // Flatpak
        "/opt/gnuradio/lib/python3.*/site-packages/gnuradio",  // Custom installation
        NULL
    };
    
    for (int i = 0; python_paths[i] != NULL; i++) {
        // Note: This is a simplified approach - in practice, you'd need to expand the wildcard
        strncpy(python_path, python_paths[i], path_size - 1);
        python_path[path_size - 1] = '\0';
        return;
    }
    
    // Default to not found
    python_path[0] = '\0';
}
#include <fcntl.h>
#include <sys/stat.h>

// GnuPG command paths
#define GPG_CMD "gpg"
#define GPG2_CMD "gpg2"
#define GPG_CONF_CMD "gpgconf"

// OpenPGP integration state
static struct {
    bool is_initialized;
    bool gpg_available;
    char gpg_command[256];
    char gpg_home[512];
    char default_key_id[17];
} g_openpgp_state = {0};

// Find GnuPG executable
static m17_openpgp_status_t find_gpg_executable(char* gpg_path, size_t path_size) {
    // Common GnuPG executable paths across different Linux distributions
    const char* gpg_paths[] = {
        // Standard system paths
        "/usr/bin/gpg",           // Most modern Linux distributions
        "/usr/bin/gpg2",          // Some distributions use gpg2
        "/usr/local/bin/gpg",     // Local installations
        "/usr/local/bin/gpg2",    // Local installations
        "/opt/gnupg/bin/gpg",     // Some package managers
        "/opt/gnupg/bin/gpg2",    // Some package managers
        "/snap/bin/gpg",          // Snap packages
        "/snap/bin/gpg2",         // Snap packages
        "/flatpak/bin/gpg",       // Flatpak packages
        "/flatpak/bin/gpg2",      // Flatpak packages
        // AppImage paths
        "/usr/local/bin/gpg.appimage",
        "/usr/local/bin/gpg2.appimage",
        // Home directory installations (will be handled by which command)
        // Alternative locations
        "/bin/gpg",               // Some minimal systems
        "/bin/gpg2",              // Some minimal systems
        NULL
    };
    
    // Try each path
    for (int i = 0; gpg_paths[i] != NULL; i++) {
        if (access(gpg_paths[i], X_OK) == 0) {
            strncpy(gpg_path, gpg_paths[i], path_size - 1);
            gpg_path[path_size - 1] = '\0';
            return M17_OPENPGP_SUCCESS;
        }
    }
    
    // Try to find gpg in PATH using which command
    FILE* fp = popen("which gpg 2>/dev/null", "r");
    if (fp != NULL) {
        char buffer[512];
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
            // Remove newline
            buffer[strcspn(buffer, "\n")] = '\0';
            if (strlen(buffer) > 0 && access(buffer, X_OK) == 0) {
                strncpy(gpg_path, buffer, path_size - 1);
                gpg_path[path_size - 1] = '\0';
                pclose(fp);
                return M17_OPENPGP_SUCCESS;
            }
        }
        pclose(fp);
    }
    
    // Try to find gpg2 in PATH using which command
    fp = popen("which gpg2 2>/dev/null", "r");
    if (fp != NULL) {
        char buffer[512];
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
            // Remove newline
            buffer[strcspn(buffer, "\n")] = '\0';
            if (strlen(buffer) > 0 && access(buffer, X_OK) == 0) {
                strncpy(gpg_path, buffer, path_size - 1);
                gpg_path[path_size - 1] = '\0';
                pclose(fp);
                return M17_OPENPGP_SUCCESS;
            }
        }
        pclose(fp);
    }
    
    // Try gpg in PATH (fallback)
    if (access("gpg", X_OK) == 0) {
        strncpy(gpg_path, "gpg", path_size - 1);
        gpg_path[path_size - 1] = '\0';
        return M17_OPENPGP_SUCCESS;
    }
    
    // Try gpg2 in PATH (fallback)
    if (access("gpg2", X_OK) == 0) {
        strncpy(gpg_path, "gpg2", path_size - 1);
        gpg_path[path_size - 1] = '\0';
        return M17_OPENPGP_SUCCESS;
    }
    
    // Provide helpful error message with distribution-specific instructions
    const char* distro = detect_linux_distribution();
    fprintf(stderr, "M17_OPENPGP_ERROR: GnuPG not found on %s. Please install GnuPG:\n", distro);
    fprintf(stderr, "  Ubuntu/Debian: sudo apt install gnupg\n");
    fprintf(stderr, "  Fedora/RHEL: sudo dnf install gnupg\n");
    fprintf(stderr, "  Arch Linux: sudo pacman -S gnupg\n");
    fprintf(stderr, "  openSUSE: sudo zypper install gpg2\n");
    fprintf(stderr, "  Alpine: sudo apk add gnupg\n");
    fprintf(stderr, "  Gentoo: sudo emerge app-crypt/gnupg\n");
    return M17_OPENPGP_ERROR_GPG_NOT_FOUND;
}

// Execute GnuPG command and capture output
static m17_openpgp_status_t execute_gpg_command(const char* args, 
                                              char* output, 
                                              size_t output_size,
                                              const char* input,
                                              size_t input_size) {
    if (!g_openpgp_state.gpg_available) {
        return M17_OPENPGP_ERROR_GPG_NOT_FOUND;
    }
    
    // Build command
    char command[1024];
    snprintf(command, sizeof(command), "%s --homedir %s %s", 
             g_openpgp_state.gpg_command, g_openpgp_state.gpg_home, args);
    
    // Create pipes for communication
    int pipe_in[2], pipe_out[2];
    if (pipe(pipe_in) == -1 || pipe(pipe_out) == -1) {
        return M17_OPENPGP_ERROR_OPERATION_FAILED;
    }
    
    pid_t pid = fork();
    if (pid == -1) {
        close(pipe_in[0]);
        close(pipe_in[1]);
        close(pipe_out[0]);
        close(pipe_out[1]);
        return M17_OPENPGP_ERROR_OPERATION_FAILED;
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
                return M17_OPENPGP_ERROR_OPERATION_FAILED;
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
        
        return (WEXITSTATUS(status) == 0) ? M17_OPENPGP_SUCCESS : M17_OPENPGP_ERROR_OPERATION_FAILED;
    }
}

// Initialize OpenPGP integration
m17_openpgp_status_t m17_openpgp_init(void) {
    if (g_openpgp_state.is_initialized) {
        return M17_OPENPGP_SUCCESS;
    }
    
    // Find GnuPG executable
    if (find_gpg_executable(g_openpgp_state.gpg_command, sizeof(g_openpgp_state.gpg_command)) != M17_OPENPGP_SUCCESS) {
        return M17_OPENPGP_ERROR_GPG_NOT_FOUND;
    }
    
    // Set GnuPG home directory
    const char* home = getenv("HOME");
    if (!home) {
        home = "/tmp";
    }
    snprintf(g_openpgp_state.gpg_home, sizeof(g_openpgp_state.gpg_home), "%s/.gnupg", home);
    
    // Check if GnuPG is properly configured
    char test_output[256];
    m17_openpgp_status_t status = execute_gpg_command("--version", test_output, sizeof(test_output), NULL, 0);
    if (status != M17_OPENPGP_SUCCESS) {
        return M17_OPENPGP_ERROR_GPG_NOT_FOUND;
    }
    
    g_openpgp_state.gpg_available = true;
    g_openpgp_state.is_initialized = true;
    
    return M17_OPENPGP_SUCCESS;
}

// Check GnuPG availability
m17_openpgp_status_t m17_openpgp_check_gpg_availability(void) {
    if (!g_openpgp_state.is_initialized) {
        m17_openpgp_status_t status = m17_openpgp_init();
        if (status != M17_OPENPGP_SUCCESS) {
            return status;
        }
    }
    
    return g_openpgp_state.gpg_available ? M17_OPENPGP_SUCCESS : M17_OPENPGP_ERROR_GPG_NOT_FOUND;
}

// List available OpenPGP keys
m17_openpgp_status_t m17_openpgp_list_keys(m17_openpgp_key_info_t* keys, 
                                         size_t max_keys, 
                                         size_t* key_count) {
    if (!keys || !key_count) {
        return M17_OPENPGP_ERROR_INVALID_PARAM;
    }
    
    char output[8192];
    m17_openpgp_status_t status = execute_gpg_command("--list-secret-keys --with-colons", 
                                                     output, sizeof(output), NULL, 0);
    if (status != M17_OPENPGP_SUCCESS) {
        return status;
    }
    
    *key_count = 0;
    char* line = strtok(output, "\n");
    
    while (line && *key_count < max_keys) {
        if (strncmp(line, "sec:", 4) == 0) {
            // Parse secret key line
            char* fields[20];
            int field_count = 0;
            char* token = strtok(line, ":");
            while (token && field_count < 20) {
                fields[field_count++] = token;
                token = strtok(NULL, ":");
            }
            
            if (field_count >= 12) {
                m17_openpgp_key_info_t* key = &keys[*key_count];
                
                // Key ID (field 4)
                strncpy(key->key_id, fields[4], sizeof(key->key_id) - 1);
                key->key_id[sizeof(key->key_id) - 1] = '\0';
                
                // Creation time (field 5)
                key->creation_time = (uint32_t)strtoul(fields[5], NULL, 10);
                
                // Expiration time (field 6)
                key->expiration_time = (uint32_t)strtoul(fields[6], NULL, 10);
                
                // User ID (field 9)
                strncpy(key->user_id, fields[9], sizeof(key->user_id) - 1);
                key->user_id[sizeof(key->user_id) - 1] = '\0';
                
                key->is_secret = true;
                key->is_nitrokey = false; // TODO: Detect Nitrokey keys
                
                (*key_count)++;
            }
        }
        line = strtok(NULL, "\n");
    }
    
    return M17_OPENPGP_SUCCESS;
}

// Sign message with OpenPGP
m17_openpgp_status_t m17_openpgp_sign_message(const char* message,
                                             size_t message_len,
                                             const char* key_id,
                                             m17_openpgp_sig_type_t sig_type,
                                             m17_openpgp_signature_t* signature) {
    if (!message || !signature || !key_id) {
        return M17_OPENPGP_ERROR_INVALID_PARAM;
    }
    
    // Build GnuPG command for signing
    char args[512];
    const char* sig_type_str = (sig_type == M17_OPENPGP_SIG_TEXT) ? "--textmode" : "--binary";
    snprintf(args, sizeof(args), "--armor --detach-sign --local-user %s %s", key_id, sig_type_str);
    
    char output[8192];
    m17_openpgp_status_t status = execute_gpg_command(args, output, sizeof(output), message, message_len);
    if (status != M17_OPENPGP_SUCCESS) {
        return M17_OPENPGP_ERROR_SIGNATURE_FAILED;
    }
    
    // Copy signature data
    strncpy(signature->signature_armored, output, sizeof(signature->signature_armored) - 1);
    signature->signature_armored[sizeof(signature->signature_armored) - 1] = '\0';
    signature->signature_size = strlen(output);
    
    strncpy(signature->key_id, key_id, sizeof(signature->key_id) - 1);
    signature->key_id[sizeof(signature->key_id) - 1] = '\0';
    
    signature->creation_time = (uint32_t)time(NULL);
    signature->sig_type = sig_type;
    
    return M17_OPENPGP_SUCCESS;
}

// Sign email with OpenPGP
m17_openpgp_status_t m17_openpgp_sign_email(const char* email_content,
                                           size_t email_len,
                                           const char* key_id,
                                           m17_openpgp_sig_type_t sig_type,
                                           m17_openpgp_signature_t* signature) {
    // For email signing, we use text mode by default
    m17_openpgp_sig_type_t email_sig_type = (sig_type == M17_OPENPGP_SIG_BINARY) ? 
                                           M17_OPENPGP_SIG_TEXT : sig_type;
    
    return m17_openpgp_sign_message(email_content, email_len, key_id, email_sig_type, signature);
}

// Verify OpenPGP signature
m17_openpgp_status_t m17_openpgp_verify_signature(const char* message,
                                                  size_t message_len,
                                                  const char* signature,
                                                  size_t signature_len,
                                                  m17_openpgp_verification_t* verification) {
    if (!message || !signature || !verification) {
        return M17_OPENPGP_ERROR_INVALID_PARAM;
    }
    
    // Create temporary files for message and signature
    char temp_msg_path[] = "/tmp/m17_openpgp_msg_XXXXXX";
    char temp_sig_path[] = "/tmp/m17_openpgp_sig_XXXXXX";
    
    int msg_fd = mkstemp(temp_msg_path);
    int sig_fd = mkstemp(temp_sig_path);
    
    if (msg_fd == -1 || sig_fd == -1) {
        if (msg_fd != -1) close(msg_fd);
        if (sig_fd != -1) close(sig_fd);
        return M17_OPENPGP_ERROR_OPERATION_FAILED;
    }
    
    // Write message and signature to temporary files
    ssize_t msg_bytes = write(msg_fd, message, message_len);
    ssize_t sig_bytes = write(sig_fd, signature, signature_len);
    close(msg_fd);
    close(sig_fd);
    
    if (msg_bytes != (ssize_t)message_len || sig_bytes != (ssize_t)signature_len) {
        return M17_OPENPGP_ERROR_OPERATION_FAILED;
    }
    
    // Verify signature
    char args[512];
    snprintf(args, sizeof(args), "--verify %s %s", temp_sig_path, temp_msg_path);
    
    char output[1024];
    m17_openpgp_status_t status = execute_gpg_command(args, output, sizeof(output), NULL, 0);
    
    // Clean up temporary files
    unlink(temp_msg_path);
    unlink(temp_sig_path);
    
    if (status != M17_OPENPGP_SUCCESS) {
        verification->is_valid = false;
        strncpy(verification->error_message, "Signature verification failed", 
                sizeof(verification->error_message) - 1);
        verification->error_message[sizeof(verification->error_message) - 1] = '\0';
        return M17_OPENPGP_ERROR_VERIFICATION_FAILED;
    }
    
    // Parse verification output to extract key information
    verification->is_valid = true;
    verification->creation_time = (uint32_t)time(NULL);
    verification->sig_type = M17_OPENPGP_SIG_BINARY;
    
    // TODO: Parse key ID and fingerprint from output
    strcpy(verification->key_id, "UNKNOWN");
    strcpy(verification->fingerprint, "UNKNOWN");
    strcpy(verification->user_id, "UNKNOWN");
    
    return M17_OPENPGP_SUCCESS;
}

// Create detached signature for file
m17_openpgp_status_t m17_openpgp_create_detached_signature(const char* file_path,
                                                         const char* key_id,
                                                         const char* output_path) {
    if (!file_path || !key_id || !output_path) {
        return M17_OPENPGP_ERROR_INVALID_PARAM;
    }
    
    char args[512];
    snprintf(args, sizeof(args), "--armor --detach-sign --local-user %s --output %s %s", 
             key_id, output_path, file_path);
    
    char output[256];
    return execute_gpg_command(args, output, sizeof(output), NULL, 0);
}

// Verify detached signature
m17_openpgp_status_t m17_openpgp_verify_detached_signature(const char* file_path,
                                                        const char* signature_path,
                                                        m17_openpgp_verification_t* verification) {
    if (!file_path || !signature_path || !verification) {
        return M17_OPENPGP_ERROR_INVALID_PARAM;
    }
    
    char args[512];
    snprintf(args, sizeof(args), "--verify %s %s", signature_path, file_path);
    
    char output[1024];
    m17_openpgp_status_t status = execute_gpg_command(args, output, sizeof(output), NULL, 0);
    
    if (status != M17_OPENPGP_SUCCESS) {
        verification->is_valid = false;
        strncpy(verification->error_message, "Detached signature verification failed", 
                sizeof(verification->error_message) - 1);
        verification->error_message[sizeof(verification->error_message) - 1] = '\0';
        return M17_OPENPGP_ERROR_VERIFICATION_FAILED;
    }
    
    verification->is_valid = true;
    verification->creation_time = (uint32_t)time(NULL);
    verification->sig_type = M17_OPENPGP_SIG_BINARY;
    
    // TODO: Parse key information from output
    strcpy(verification->key_id, "UNKNOWN");
    strcpy(verification->fingerprint, "UNKNOWN");
    strcpy(verification->user_id, "UNKNOWN");
    
    return M17_OPENPGP_SUCCESS;
}

// Generate OpenPGP keypair
m17_openpgp_status_t m17_openpgp_generate_keypair(const char* name,
                                                const char* email,
                                                const char* comment,
                                                const char* passphrase,
                                                uint32_t key_size,
                                                uint32_t expiration_days) {
    if (!name || !email) {
        return M17_OPENPGP_ERROR_INVALID_PARAM;
    }
    
    // Create batch file for key generation
    char batch_file[] = "/tmp/m17_openpgp_batch_XXXXXX";
    int batch_fd = mkstemp(batch_file);
    if (batch_fd == -1) {
        return M17_OPENPGP_ERROR_OPERATION_FAILED;
    }
    
    char batch_content[1024];
    snprintf(batch_content, sizeof(batch_content),
             "Key-Type: RSA\n"
             "Key-Length: %u\n"
             "Name-Real: %s\n"
             "Name-Email: %s\n"
             "%s%s\n"
             "Expire-Date: %u\n"
             "%s\n",
             key_size, name, email,
             comment ? "Name-Comment: " : "",
             comment ? comment : "",
             expiration_days,
             passphrase ? "Passphrase: " : "%no-protection\n");
    
    if (passphrase) {
        strcat(batch_content, passphrase);
        strcat(batch_content, "\n");
    }
    
    ssize_t batch_bytes = write(batch_fd, batch_content, strlen(batch_content));
    close(batch_fd);
    
    if (batch_bytes != (ssize_t)strlen(batch_content)) {
        return M17_OPENPGP_ERROR_OPERATION_FAILED;
    }
    
    // Generate key
    char args[512];
    snprintf(args, sizeof(args), "--batch --gen-key %s", batch_file);
    
    char output[1024];
    m17_openpgp_status_t status = execute_gpg_command(args, output, sizeof(output), NULL, 0);
    
    // Clean up batch file
    unlink(batch_file);
    
    return status;
}

// Export public key in ASCII-armored format
m17_openpgp_status_t m17_openpgp_export_public_key(const char* key_id,
                                                  char* armored_key,
                                                  size_t armored_key_size) {
    if (!key_id || !armored_key) {
        return M17_OPENPGP_ERROR_INVALID_PARAM;
    }
    
    char args[256];
    snprintf(args, sizeof(args), "--armor --export %s", key_id);
    
    return execute_gpg_command(args, armored_key, armored_key_size, NULL, 0);
}

// Import public key from ASCII-armored format
m17_openpgp_status_t m17_openpgp_import_public_key(const char* armored_key,
                                                  size_t armored_key_size) {
    if (!armored_key) {
        return M17_OPENPGP_ERROR_INVALID_PARAM;
    }
    
    return execute_gpg_command("--import", NULL, 0, armored_key, armored_key_size);
}

// Set default signing key
m17_openpgp_status_t m17_openpgp_set_default_key(const char* key_id) {
    if (!key_id) {
        return M17_OPENPGP_ERROR_INVALID_PARAM;
    }
    
    strncpy(g_openpgp_state.default_key_id, key_id, sizeof(g_openpgp_state.default_key_id) - 1);
    g_openpgp_state.default_key_id[sizeof(g_openpgp_state.default_key_id) - 1] = '\0';
    
    return M17_OPENPGP_SUCCESS;
}

// Get default signing key
m17_openpgp_status_t m17_openpgp_get_default_key(char* key_id, size_t key_id_size) {
    if (!key_id || key_id_size == 0) {
        return M17_OPENPGP_ERROR_INVALID_PARAM;
    }
    
    strncpy(key_id, g_openpgp_state.default_key_id, key_id_size - 1);
    key_id[key_id_size - 1] = '\0';
    
    return M17_OPENPGP_SUCCESS;
}

// Cleanup OpenPGP integration
void m17_openpgp_cleanup(void) {
    g_openpgp_state.is_initialized = false;
    g_openpgp_state.gpg_available = false;
    memset(g_openpgp_state.gpg_command, 0, sizeof(g_openpgp_state.gpg_command));
    memset(g_openpgp_state.gpg_home, 0, sizeof(g_openpgp_state.gpg_home));
    memset(g_openpgp_state.default_key_id, 0, sizeof(g_openpgp_state.default_key_id));
}
