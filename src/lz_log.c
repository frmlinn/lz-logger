/**
 * @file lz_log.c
 * @brief Implementation of the lock-free, context-aware message dispatcher.
 */

#define _GNU_SOURCE
#include "lz_log.h"
#include "lz_log_format.h"
#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

#ifdef __linux__
#include <sys/syscall.h>
#endif

/* CPU Relax for spin-locks to avoid burning CPU and memory bus */
#if defined(__x86_64__) || defined(__i386__)
    #define LZ_CPU_RELAX() __asm__ volatile("pause" ::: "memory")
#elif defined(__aarch64__) || defined(__arm__)
    #define LZ_CPU_RELAX() __asm__ volatile("yield" ::: "memory")
#else
    #define LZ_CPU_RELAX() do {} while(0)
#endif

#define LZ_LOG_MAX_MSG_SIZE 512

/* ANSI Color Codes */
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_GRAY    "\x1b[90m"
#define ANSI_COLOR_RESET   "\x1b[0m"

/* * Globals aligned to 64 bytes to prevent False Sharing with hot allocator paths.
 */
_Atomic int g_lz_log_level __attribute__((aligned(64))) = LZ_LOG_LEVEL_INFO;
static _Atomic bool g_crash_gate __attribute__((aligned(64))) = false;

/* Thread-local storage for Native TID */
static __thread uint64_t tls_cached_tid __attribute__((tls_model("initial-exec"))) = 0;

/**
 * @brief Ensures all bytes are written to the file descriptor, resilient to signals.
 */
static void safe_write_all(int fd, const char* buf, size_t count) {
    size_t written = 0;
    while (written < count) {
        ssize_t res = write(fd, buf + written, count - written);
        if (LZ_LOG_UNLIKELY(res < 0)) {
            if (errno == EINTR || errno == EAGAIN) {
                continue; /* Interrupted by signal, retry */
            }
            break; /* Unrecoverable I/O error */
        }
        written += (size_t)res;
    }
}

/**
 * @brief Callback executed only in the child process immediately after a fork().
 */
static void lz_log_reset_tid_cache_atfork(void) {
    tls_cached_tid = 0;
}

/**
 * @brief Library constructor.
 */
__attribute__((constructor)) static void lz_log_init(void) {
    pthread_atfork(NULL, NULL, lz_log_reset_tid_cache_atfork);
}

/**
 * @brief Appends a string to the buffer using block-copy for SIMD optimization.
 * Protected against buffer overruns.
 */
static inline void append_string(char* buf, size_t* pos, const char* str) {
    if (LZ_LOG_UNLIKELY(!str)) str = "(null)";
    
    if (LZ_LOG_UNLIKELY(*pos >= (LZ_LOG_MAX_MSG_SIZE - 2))) return;

    size_t len = lz_log_strlen(str);
    size_t max_avail = (LZ_LOG_MAX_MSG_SIZE - 2) - *pos;
    
    if (LZ_LOG_UNLIKELY(len > max_avail)) {
        len = max_avail;
    }
    
    if (LZ_LOG_LIKELY(len > 0)) {
        __builtin_memcpy(buf + *pos, str, len);
        *pos += len;
    }
}

/**
 * @brief Safely appends a single character.
 */
static inline void append_char(char* buf, size_t* pos, char c) {
    if (LZ_LOG_LIKELY(*pos < (LZ_LOG_MAX_MSG_SIZE - 2))) {
        buf[(*pos)++] = c;
    }
}

/**
 * @brief Gets the native OS Thread ID with a Zero-Syscall Fast-Path.
 */
LZ_LOG_ALWAYS_INLINE uint64_t get_native_tid(void) {
    if (LZ_LOG_LIKELY(tls_cached_tid != 0)) return tls_cached_tid;

#ifdef __linux__
    tls_cached_tid = (uint64_t)syscall(SYS_gettid);
#elif defined(__APPLE__)
    pthread_threadid_np(NULL, &tls_cached_tid);
#else
    tls_cached_tid = 1; // Fallback
#endif

    return tls_cached_tid;
}

/**
 * @brief Gets a fast, monotonic timestamp (microseconds).
 */
static inline uint64_t get_timestamp_us(void) {
    struct timespec ts;
#if defined(__linux__) && defined(CLOCK_MONOTONIC_COARSE)
    clock_gettime(CLOCK_MONOTONIC_COARSE, &ts);
#else
    clock_gettime(CLOCK_MONOTONIC, &ts);
#endif
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
    append_char(stack_buf, &pos, ':');
    append_string(stack_buf, &pos, lz_log_itoa(line, num_buf));
    append_string(stack_buf, &pos, " - ");

    /* 4. Format parsing */
    va_list args;
    va_start(args, format);
    while (*format && pos < (LZ_LOG_MAX_MSG_SIZE - 2)) {
        if (*format == '%') {
            format++;
            
            if (LZ_LOG_UNLIKELY(*format == '\0')) {
                append_char(stack_buf, &pos, '%');
                break;
            }
            
            int is_long = 0;
            int is_size_t = 0;
            
            while (*format == 'l' || *format == 'z') {
                if (*format == 'l') is_long++;
                if (*format == 'z') is_size_t = 1;
                format++;
            }

            if (LZ_LOG_UNLIKELY(*format == '\0')) {
                break;
            }

            switch (*format) {
                case 's': 
                    append_string(stack_buf, &pos, va_arg(args, const char*)); 
                    break;
                case 'd': {
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
                case 'c': 
                    append_char(stack_buf, &pos, (char)va_arg(args, int));
                    break;
                case '%': 
                    append_char(stack_buf, &pos, '%');
                    break;
                default:  
                    append_char(stack_buf, &pos, '%');
                    append_char(stack_buf, &pos, *format);
                    break;
            }
        } else {
            append_char(stack_buf, &pos, *format);
        }
        format++;
    }
    va_end(args);

    /* 5. Dispatch */
    stack_buf[pos++] = '\n';
    stack_buf[pos] = '\0';

    if (LZ_LOG_UNLIKELY(level == LZ_LOG_LEVEL_FATAL)) {
        bool expected = false;
        if (atomic_compare_exchange_strong_explicit(&g_crash_gate, &expected, true, 
                                                    memory_order_seq_cst, 
                                                    memory_order_relaxed)) {
            safe_write_all(STDERR_FILENO, stack_buf, pos);
            __builtin_trap(); 
        } else {
            for (;;) {
                LZ_CPU_RELAX();
            }
        }
    } else {
        safe_write_all(STDERR_FILENO, stack_buf, pos);
    }
}