/**
 * @file lz_log.c
 * @brief Implementation of the lock-free, context-aware message dispatcher.
 */

#define _GNU_SOURCE
#include "lz_log.h"
#include "lz_log_format.h"
#include <stdarg.h>
#include <unistd.h>
#include <time.h>

#ifdef __linux__
#include <sys/syscall.h>
#elif defined(__APPLE__)
#include <pthread.h>
#endif

int g_lz_log_level = LZ_LOG_LEVEL_INFO;

/* ANSI Color Codes */
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_GRAY    "\x1b[90m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define LZ_LOG_MAX_MSG_SIZE 512

/**
 * @brief Appends a string to the buffer safely.
 */
static inline void append_string(char* buf, size_t* pos, const char* str) {
    if (LZ_LOG_UNLIKELY(!str)) str = "(null)";
    while (*str && *pos < (LZ_LOG_MAX_MSG_SIZE - 2)) {
        buf[(*pos)++] = *str++;
    }
}

/**
 * @brief Gets the native OS Thread ID with a Zero-Syscall Fast-Path.
 */
static LZ_LOG_ALWAYS_INLINE uint64_t get_native_tid(void) {
#ifdef __linux__
    static __thread uint64_t cached_tid = 0;
    if (LZ_LOG_LIKELY(cached_tid != 0)) return cached_tid;
    cached_tid = (uint64_t)syscall(SYS_gettid);
    return cached_tid;
#elif defined(__APPLE__)
    static __thread uint64_t cached_tid = 0;
    if (LZ_LOG_LIKELY(cached_tid != 0)) return cached_tid;
    pthread_threadid_np(NULL, &cached_tid);
    return cached_tid;
#else
    return 0;
#endif
}

/**
 * @brief Gets a fast, monotonic timestamp (microseconds).
 */
static inline uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000ULL);
}

void lz_internal_log(int level, const char* file, int line, const char* format, ...) {
    char stack_buf[LZ_LOG_MAX_MSG_SIZE];
    size_t pos = 0;
    char num_buf[32];

    /* 1. Level & Color */
    switch (level) {
        case LZ_LOG_LEVEL_DEBUG: append_string(stack_buf, &pos, ANSI_COLOR_GRAY   "[DEBUG]" ANSI_COLOR_RESET " "); break;
        case LZ_LOG_LEVEL_INFO:  append_string(stack_buf, &pos, ANSI_COLOR_GREEN  "[INFO] " ANSI_COLOR_RESET " "); break;
        case LZ_LOG_LEVEL_WARN:  append_string(stack_buf, &pos, ANSI_COLOR_YELLOW "[WARN] " ANSI_COLOR_RESET " "); break;
        case LZ_LOG_LEVEL_ERROR: append_string(stack_buf, &pos, ANSI_COLOR_RED    "[ERROR]" ANSI_COLOR_RESET " "); break;
        case LZ_LOG_LEVEL_FATAL: append_string(stack_buf, &pos, ANSI_COLOR_RED    "[FATAL]" ANSI_COLOR_RESET " "); break;
        default:                 append_string(stack_buf, &pos, "[LOG]   "); break;
    }

    /* 2. Timestamp & TID */
    append_string(stack_buf, &pos, "[T+");
    append_string(stack_buf, &pos, lz_log_utoa(get_timestamp_us(), num_buf));
    append_string(stack_buf, &pos, "us] [TID:");
    append_string(stack_buf, &pos, lz_log_utoa(get_native_tid(), num_buf));
    append_string(stack_buf, &pos, "] ");

    /* 3. File & Line */
    append_string(stack_buf, &pos, file);
    append_string(stack_buf, &pos, ":");
    append_string(stack_buf, &pos, lz_log_itoa(line, num_buf));
    append_string(stack_buf, &pos, " - ");

    /* 4. Format parsing (Safe type extraction) */
    va_list args;
    va_start(args, format);
    while (*format && pos < (LZ_LOG_MAX_MSG_SIZE - 2)) {
        if (*format == '%') {
            format++;
            
            int is_long = 0;
            int is_size_t = 0;
            
            // Parse length modifiers (%ld, %lld, %zu)
            while (*format == 'l' || *format == 'z') {
                if (*format == 'l') is_long++;
                if (*format == 'z') is_size_t = 1;
                format++;
            }

            switch (*format) {
                case 's': 
                    append_string(stack_buf, &pos, va_arg(args, const char*)); 
                    break;
                case 'd': {
                    // Properly extract based on type promotion rules
                    int64_t val = is_size_t ? va_arg(args, ssize_t) :
                                  (is_long >= 2) ? va_arg(args, long long) :
                                  (is_long == 1) ? va_arg(args, long) :
                                  va_arg(args, int);
                    append_string(stack_buf, &pos, lz_log_itoa(val, num_buf)); 
                    break;
                }
                case 'u': {
                    uint64_t val = is_size_t ? va_arg(args, size_t) :
                                   (is_long >= 2) ? va_arg(args, unsigned long long) :
                                   (is_long == 1) ? va_arg(args, unsigned long) :
                                   va_arg(args, unsigned int);
                    append_string(stack_buf, &pos, lz_log_utoa(val, num_buf)); 
                    break;
                }
                case 'x': {
                    uint64_t val = is_size_t ? va_arg(args, size_t) :
                                   (is_long >= 2) ? va_arg(args, unsigned long long) :
                                   (is_long == 1) ? va_arg(args, unsigned long) :
                                   va_arg(args, unsigned int);
                    append_string(stack_buf, &pos, lz_log_xtoa(val, num_buf)); 
                    break;
                }
                case 'p': 
                    append_string(stack_buf, &pos, lz_log_ptr_to_hex((uintptr_t)va_arg(args, void*), num_buf)); 
                    break;
                case 'c': {
                    char c = (char)va_arg(args, int); // chars promote to int in variadic
                    if (pos < (LZ_LOG_MAX_MSG_SIZE - 2)) stack_buf[pos++] = c;
                    break;
                }
                case '%': 
                    stack_buf[pos++] = '%'; 
                    break;
                default:  
                    stack_buf[pos++] = '%'; 
                    if (pos < (LZ_LOG_MAX_MSG_SIZE - 2)) stack_buf[pos++] = *format; 
                    break;
            }
        } else {
            stack_buf[pos++] = *format;
        }
        format++;
    }
    va_end(args);

    /* 5. Dispatch */
    stack_buf[pos++] = '\n';
    stack_buf[pos] = '\0';

    #pragma GCC diagnostic ignored "-Wunused-result"
    write(STDERR_FILENO, stack_buf, pos);
}