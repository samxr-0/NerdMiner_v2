#ifndef _WMANAGER_H_
#define _WMANAGER_H_

#include <Arduino.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "drivers/storage/nvMemory.h"
#include "settings.h"
#include "monitor.h"
#include "drivers/storage/SDCard.h"

// Pool definitions
struct PoolConfig {
    const char* name;
    const char* url;
    int port;
    const char* webUrl;
    const char* info;
};

extern const PoolConfig POOLS[];
extern const size_t POOL_COUNT;

// Timezone definitions
struct TimezoneInfo {
    const char* name;
    int offset;
};

extern const TimezoneInfo TIMEZONE_LIST[];
extern const int TIMEZONE_COUNT;

// Configuration portal timeout (2 minutes)
#define CONFIG_PORTAL_TIMEOUT 120000

// DNS Server port for captive portal
#define DNS_PORT 53

// Function declarations
void init_WifiManager();
void wifiManagerProcess();
void setupParameters();
void reset_configuration();
void saveNewConfig();

// Memory monitoring helpers
size_t getFreeHeap();
size_t getMinFreeHeap();

#endif // _WMANAGER_H_
