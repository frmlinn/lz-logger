/**
 * @file lz_log.h
 * @brief Zero-allocation, async-signal-safe logging macro system.
 */

#ifndef LZ_LOG_H
#define LZ_LOG_H

#include <stddef.h>
#include <stdint.h>

/* ========================================================================= *
 * Compiler Built-ins (Total Independence)
 * ========================================================================= */

#define LZ_LOG_LIKELY(x)   __builtin_expect(!!(x), 1)
#define LZ_LOG_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define LZ_LOG_ALWAYS_INLINE inline __attribute__((always_inline))

/* ========================================================================= *
 * Log Levels
 * ========================================================================= */

#define LZ_LOG_LEVEL_DEBUG 0
#define LZ_LOG_LEVEL_INFO  1
#define LZ_LOG_LEVEL_WARN  2
#define LZ_LOG_LEVEL_ERROR 3
#define LZ_LOG_LEVEL_FATAL 4

/* ========================================================================= *
 * Global Configuration
 * ========================================================================= */

/** * @brief Current active log level. 
 * Can be modified at runtime to silence logs without recompiling.
 */
extern int g_lz_log_level;

/* ========================================================================= *
 * Public Macro API
 * ========================================================================= */

/**
 * @brief Internal dispatch function. Do not call directly.
 */
void lz_internal_log(int level, const char* file, int line, const char* format, ...);

#if defined(NDEBUG)
    /** @brief Debug log macro. Evaporates in Release builds. */
    #define LZ_DEBUG(fmt, ...) do {} while(0)
#else
    /** @brief Debug log macro. Prints only if current level allows it. */
    #define LZ_DEBUG(fmt, ...) \
        do { if (g_lz_log_level <= LZ_LOG_LEVEL_DEBUG) lz_internal_log(LZ_LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__); } while(0)
#endif

/** @brief Info log macro. */
#define LZ_INFO(fmt, ...) \
    do { if (g_lz_log_level <= LZ_LOG_LEVEL_INFO) lz_internal_log(LZ_LOG_LEVEL_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__); } while(0)

/** @brief Warning log macro. Evaluated as an unlikely cold path. */
#define LZ_WARN(fmt, ...) \
    do { if (LZ_LOG_UNLIKELY(g_lz_log_level <= LZ_LOG_LEVEL_WARN)) lz_internal_log(LZ_LOG_LEVEL_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__); } while(0)

/** @brief Error log macro. Evaluated as an unlikely cold path. */
#define LZ_ERROR(fmt, ...) \
    do { if (LZ_LOG_UNLIKELY(g_lz_log_level <= LZ_LOG_LEVEL_ERROR)) lz_internal_log(LZ_LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__); } while(0)

/** @brief Fatal log macro. Traps (crashes) the program after logging. */
#define LZ_FATAL(fmt, ...) \
    do { lz_internal_log(LZ_LOG_LEVEL_FATAL, __FILE__, __LINE__, fmt, ##__VA_ARGS__); __builtin_trap(); } while(0)

#endif // LZ_LOG_H
