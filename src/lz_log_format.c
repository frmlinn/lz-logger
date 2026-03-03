/**
 * @file lz_log_format.c
 * @brief Implementation of bare-metal formatting primitives.
 */

#include "lz_log_format.h"

/** @brief Digits LUT for hexadecimal conversion. */
static const char HEX_DIGITS[] = "0123456789abcdef";

size_t lz_log_strlen(const char* s) {
    const char* p = s;
    while (*p) p++;
    return (size_t)(p - s);
}

char* lz_log_utoa(uint64_t value, char* buf) {
    char* p = buf + 20; 
    *p = '\0';

    if (value == 0) {
        *--p = '0';
        return p;
    }

    while (value > 0) {
        *--p = (char)((value % 10) + '0');
        value /= 10;
    }

    return p;
}

char* lz_log_itoa(int64_t value, char* buf) {
    uint64_t uval;
    int negative = 0;

    if (value < 0) {
        negative = 1;
        // Handle LONG_MIN correctly by casting to unsigned before negating
        uval = (uint64_t)-(value + 1) + 1;
    } else {
        uval = (uint64_t)value;
    }

    char* p = lz_log_utoa(uval, buf + 1); // Leave space for '-'

    if (negative) {
        *--p = '-';
    }

    return p;
}

char* lz_log_ptr_to_hex(uintptr_t ptr, char* buf) {
    buf[18] = '\0';
    uintptr_t val = ptr;

    for (int i = 17; i >= 2; --i) {
        buf[i] = HEX_DIGITS[val & 0xf];
        val >>= 4;
    }

    buf[1] = 'x';
    buf[0] = '0';

    return buf;
}

char* lz_log_xtoa(uint64_t value, char* buf) {
    char* p = buf + 18; 
    *p = '\0';

    if (value == 0) {
        *--p = '0';
        return p;
    }

    while (value > 0) {
        *--p = HEX_DIGITS[value & 0xf];
        value >>= 4;
    }

    return p;
}
