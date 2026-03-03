# =========================================================================
# lz-logger - Zero-allocation, Async-Signal-Safe Logging Library
# =========================================================================

cmake_minimum_required(VERSION 3.10)
project(lz_logger VERSION 1.0.0 LANGUAGES C)

set(LOG_SRCS 
    src/lz_log.c 
    src/lz_log_format.c
)

add_library(lz_logger STATIC ${LOG_SRCS})

target_include_directories(lz_logger PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_options(lz_logger PRIVATE 
    -Wall -Wextra -Wpedantic -Werror
    -fPIC
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Debug>:-O0 -g>
)