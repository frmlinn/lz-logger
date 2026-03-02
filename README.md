# lzlogger

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Standard](https://img.shields.io/badge/C-11-blue.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS-lightgrey)](#)

A bare-metal, zero-allocation, and async-signal-safe logging library for C11 systems programming. Built specifically for environments where standard `libc` functions fail: custom memory allocators, signal handlers, and real-time kernel bypass applications.

## The Problem: Why not just use `printf` or `zlog`?

If you are writing a custom POSIX memory allocator (overriding `malloc`) or a critical `SIGSEGV` handler, using standard logging libraries is a death sentence:
1. **The `malloc` recursion loop:** `printf` and logging libraries like `zlog` allocate memory internally. If you call them from inside your own `malloc`, you cause an infinite recursion that blows up the stack.
2. **Async-Signal-Safety:** Standard I/O functions use internal locks and global buffers. If a thread crashes while holding the `libc` I/O lock, attempting to log the crash in the signal handler will cause an unrecoverable deadlock.



## Features

* **Zero-Allocation:** Strictly avoids `malloc`, `calloc`, or `free`. Formats everything entirely on the local thread's stack.
* **Async-Signal-Safe:** 100% safe to invoke from within signal handlers. It relies solely on the atomic POSIX `write()` syscall.
* **Lock-Free Assembly:** No shared buffers or mutexes. Threads never block each other while assembling log messages.
* **Rich Terminal UX (Color-Coded):** Automatically injects ANSI color codes (Red for FATAL, Yellow for WARN, Green for INFO) for lightning-fast visual grepping during multi-threaded debugging.
* **High-Precision Context:** * Injects the native OS Thread ID (TID) using a zero-syscall TLS Fast-Path.
  * Injects microsecond-level monotonic timestamps (`CLOCK_MONOTONIC`).
* **Zero-Cost Abstractions:** When compiled with `-DNDEBUG` (Release mode), `LZ_DEBUG` macros completely evaporate from the generated assembly. Zero CPU cycles wasted.
* **Custom Variadic Parser:** Safely handles length modifiers (`%ld`, `%zu`, `%llu`) with strict type promotion, bypassing `vsnprintf`.

## Integration

`lzlogger` is designed to be embedded as a static library or Git Submodule. It has **zero external dependencies**.

**1. Add it to your project:**
```bash
git submodule add [https://github.com/frmlinn/lzlogger.git](https://github.com/frmlinn/lzlogger.git) external/lzlogger
```

**2. Link it via CMake:**
```bash
# Add the subdirectory
add_subdirectory(external/lzlogger)

# Link it statically to your target
target_link_libraries(your_target PRIVATE lz_logger)
```

## Quick start
Simply include the header and use the macros. No initialization required.
```C
#include "lz_log.h"
#include <stddef.h>

int main() {
    size_t allocated_bytes = 4096;
    void* ptr = (void*)0x7fff51234abc;

    // Standard information
    LZ_INFO("Successfully mapped %zu bytes at %p", allocated_bytes, ptr);

    // Debug logs (Automatically stripped in Release builds)
    LZ_DEBUG("Thread acquired internal spinlock");

    // Warnings
    LZ_WARN("Memory fragmentation is above %d%%", 85);

    // Fatal errors (Logs the message, then traps/aborts the process)
     LZ_FATAL("Heap corruption detected near chunk %x", 0xFF);

    return 0;
}
```
## How it works under the hood
* **Stack Buffering**: When `LZ_INFO` is called, a fixed 512-byte buffer is allocated on the current thread's stack.

* **Reverse Formatting**: Integers and pointers are converted to ASCII using highly optimized look-up tables (LUTs) and reverse-filling algorithms, avoiding expensive division instructions where possible.

* **Atomic Dispatch**: Once the message is assembled with its colors, file name, line number, TID, and timestamp, it is dispatched to `STDERR_FILENO` using a single write() system call. Because the buffer is smaller than `PIPE_BUF` (usually 4KB), the Linux kernel guarantees the message will be printed atomically, without interleaving with other threads.