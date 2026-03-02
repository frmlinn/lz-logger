/**
 * @file lz_log_format.h
 * @brief High-performance, async-signal-safe string formatting utilities.
 * * This module provides primitives to convert binary data into ASCII strings
 * without using libc (no snprintf, no malloc). Optimized for zero-allocation.
 */

#ifndef LZ_LOG_FORMAT_H
#define LZ_LOG_FORMAT_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Converts an unsigned 64-bit integer to a decimal string.
 * @param value The number to convert.
 * @param buf The destination buffer (must be at least 21 bytes).
 * @return Pointer to the first character of the string in the buffer.
 */
char* lz_log_utoa(uint64_t value, char* buf);

/**
 * @brief Converts a signed 64-bit integer to a decimal string.
 * @param value The number to convert.
 * @param buf The destination buffer (must be at least 22 bytes).
 * @return Pointer to the first character of the string in the buffer.
 */
char* lz_log_itoa(int64_t value, char* buf);

/**
 * @brief Converts a pointer/uintptr_t to a hexadecimal string (0x...).
 * @param ptr The pointer or address to convert.
 * @param buf The destination buffer (must be at least 19 bytes).
 * @return Pointer to the first character of the string (start of '0x').
 */
char* lz_log_ptr_to_hex(uintptr_t ptr, char* buf);

/**
 * @brief Converts an unsigned 64-bit integer to a compressed hexadecimal string.
 * @param value The number to convert.
 * @param buf The destination buffer (must be at least 19 bytes).
 * @return Pointer to the first character of the string in the buffer.
 */
char* lz_log_xtoa(uint64_t value, char* buf);

/**
 * @brief Simple strlen implementation to avoid libc dependency.
 * @param s The null-terminated string.
 * @return Number of characters before the null terminator.
 */
size_t lz_log_strlen(const char* s);

#endif // LZ_LOG_FORMAT_H