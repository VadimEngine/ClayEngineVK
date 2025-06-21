#pragma once
// standard lib
#include <stdarg.h>
#include <stdio.h>
#include <string>

// Define the global flag for enabling/disabling logs
#define PRINT_GLOBAL

#if defined(CLAY_PLATFORM_XR) || defined(CLAY_PLATFORM_ANDROID)
// Android platform logging
#include <android/log.h>

#define LOG_I(...) __android_log_print(ANDROID_LOG_INFO, "AppXR", __VA_ARGS__)
#define LOG_W(...) __android_log_print(ANDROID_LOG_WARN, "AppXR", __VA_ARGS__)
#define LOG_E(...) __android_log_print(ANDROID_LOG_ERROR, "AppXR", __VA_ARGS__)
#define LOG_IV(...) __android_log_print(ANDROID_LOG_INFO, "AppXR", "(%s:%s:%d) " __VA_ARGS__, __FILE__, __FUNCTION__, __LINE__)

#else // Non-Android platforms

// Standard logging macros
#ifdef PRINT_GLOBAL
#define LOG_I(...)                      \
{                                       \
    printf("Info: ");                   \
    printf(__VA_ARGS__);                \
    printf("\n");                       \
}

#define LOG_IV(...)                                                 \
{                                                                   \
    printf("Info (%s:%s:%d): ", __FILE__, __FUNCTION__, __LINE__);  \
    printf(__VA_ARGS__);                                            \
    printf("\n");                                                   \
}

#define LOG_W(...)          \
{                           \
    printf("Warning: ");    \
    printf(__VA_ARGS__);    \
    printf("\n");           \
}

#define LOG_E(...)          \
{                           \
    printf("Error: ");      \
    printf(__VA_ARGS__);    \
    printf("\n");           \
}

#else
// If PRINT_GLOBAL is not defined, disable logs
#define LOG_I(...) // No log
#define LOG_IV(...) // No log
#define LOG_W(...) // No log
#define LOG_E(...) // No log
#endif // PRINT_GLOBAL

#endif // #ifdef CLAY_PLATFORM_XR

