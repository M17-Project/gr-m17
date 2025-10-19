//--------------------------------------------------------------------
// M17 C library - m17_safe.c
//
// Safety and error handling implementation for M17 library
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 12 March 2025
//--------------------------------------------------------------------
#include <string.h>
#include <stdlib.h>
#include "m17_safe.h"
#include "m17.h"

// Thread safety mutexes
M17_MUTEX_DECLARE(viterbi_mutex)
M17_MUTEX_DECLARE(golay_mutex)

int m17_safe_memcpy(void* dest, size_t dest_size, const void* src, size_t src_size)
{
    if (dest == NULL || src == NULL) {
        return -1;
    }
    
    if (src_size > dest_size) {
        return -1;
    }
    
    if (src_size == 0) {
        return -1;
    }
    
    memcpy(dest, src, src_size);
    return 0;
}

m17_error_t m17_safe_memset(void* dest, size_t dest_size, int value, size_t count)
{
    if (dest == NULL) {
        return M17_ERROR_NULL_POINTER;
    }
    
    if (count > dest_size) {
        return M17_ERROR_BUFFER_OVERFLOW;
    }
    
    memset(dest, value, count);
    return M17_SUCCESS;
}

m17_error_t m17_validate_callsign(const char* callsign)
{
    if (callsign == NULL) {
        return M17_ERROR_NULL_POINTER;
    }
    
    size_t len = strlen(callsign);
    if (len == 0 || len > 9) {
        return M17_ERROR_INVALID_CALLSIGN;
    }
    
    // Check for valid characters (A-Z, 0-9, space, /, -, .)
    for (size_t i = 0; i < len; i++) {
        char c = callsign[i];
        if (!((c >= 'A' && c <= 'Z') || 
              (c >= '0' && c <= '9') || 
              c == ' ' || c == '/' || c == '-' || c == '.')) {
            return M17_ERROR_INVALID_CALLSIGN;
        }
    }
    
    return M17_SUCCESS;
}

m17_error_t m17_validate_frame_type(uint8_t frame_type)
{
    // Valid frame types: 0=LSF, 1=Stream, 2=Packet, 3=BERT
    if (frame_type > 3) {
        return M17_ERROR_INVALID_FRAME_TYPE;
    }
    
    return M17_SUCCESS;
}

m17_error_t m17_validate_syncword(uint16_t syncword)
{
    // Check against known syncwords
    if (syncword != SYNC_LSF && 
        syncword != SYNC_STR && 
        syncword != SYNC_PKT && 
        syncword != SYNC_BER && 
        syncword != EOT_MRKR) {
        return M17_ERROR_INVALID_SYNCWORD;
    }
    
    return M17_SUCCESS;
}

const char* m17_error_string(m17_error_t error)
{
    switch (error) {
        case M17_SUCCESS:
            return "Success";
        case M17_ERROR_NULL_POINTER:
            return "Null pointer error";
        case M17_ERROR_INVALID_PARAM:
            return "Invalid parameter";
        case M17_ERROR_BUFFER_OVERFLOW:
            return "Buffer overflow";
        case M17_ERROR_INVALID_LENGTH:
            return "Invalid length";
        case M17_ERROR_INVALID_SYNCWORD:
            return "Invalid syncword";
        case M17_ERROR_DECODE_FAILED:
            return "Decode failed";
        case M17_ERROR_CRC_MISMATCH:
            return "CRC mismatch";
        case M17_ERROR_INVALID_CALLSIGN:
            return "Invalid callsign";
        case M17_ERROR_INVALID_FRAME_TYPE:
            return "Invalid frame type";
        case M17_ERROR_MEMORY_ALLOCATION:
            return "Memory allocation error";
        case M17_ERROR_THREAD_SAFETY:
            return "Thread safety error";
        case M17_ERROR_INTERNAL:
            return "Internal error";
        default:
            return "Unknown error";
    }
}
