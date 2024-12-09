#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#include <Arduino.h>

// Logging Verbosity Enum
enum LogVerbosity {
    LOG_NONE = 0,
    LOG_ERROR = 1,
    LOG_WARN = 2,
    LOG_INFO = 3,
    LOG_DEBUG = 4
};

// Comprehensive Settings Structure
struct TSettings {
    // WiFi Settings
    String WifiSSID = "";
    String WifiPW = "";

    // Pool Settings
    String PoolAddress = "public-pool.io";
    int PoolPort = 21496;
    char PoolPassword[64] = "x";
    char BtcWallet[64] = "";

    // Display and UI Settings
    bool displayEnabled = true;
    bool ledEnabled = true;
    bool invertColors = false;

    // System Settings
    int Timezone = 0;
    bool saveStats = false;

    // Logging Settings
    LogVerbosity LogLevel{ LOG_INFO };  // Default log level is INFO
};

// Logging Macro for conditional compilation and runtime verbosity
#define LOG(level, ...) \
    do { \
        extern TSettings Settings; \
        if (Settings.LogLevel >= level) { \
            Serial.printf(__VA_ARGS__); \
        } \
    } while(0)

#endif // _SETTINGS_H_
