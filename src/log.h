#ifndef _LOG_H_
#define _LOG_H_

#include <Arduino.h>
#include "settings.h"  // Include settings.h for LogVerbosity and Settings

// Logging Macro for conditional compilation and runtime verbosity
#define LOG(level, ...) do { \
    extern TSettings Settings; \
    if (Settings.LogLevel >= level) { \
        Serial.printf(__VA_ARGS__); \
    } \
} while(0)

#endif // _LOG_H_
