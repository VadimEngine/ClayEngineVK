#pragma once
// standard lib
#include <stdarg.h>
#include <stdio.h>
#include <string>

// Logs are on unless the build defines CLAY_NO_LOGS (e.g. a game's Release config:
// target_compile_definitions(ClayEngineVK PUBLIC $<$<CONFIG:Release>:CLAY_NO_LOGS>)).
#ifndef CLAY_NO_LOGS
#define PRINT_GLOBAL
#endif

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
// If PRINT_GLOBAL is not defined, disable logs (the arguments are not evaluated).
#define LOG_I(...) ((void)0)
#define LOG_IV(...) ((void)0)
#define LOG_W(...) ((void)0)
#define LOG_E(...) ((void)0)
#endif // PRINT_GLOBAL

#endif // #ifdef CLAY_PLATFORM_XR

