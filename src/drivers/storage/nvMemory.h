#ifndef _NVMEMORY_H_
#define _NVMEMORY_H_

// Support both SPIFFS and NVS
#define NVMEM_SPIFFS
#define NVMEM_NVS

#include <Arduino.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <SPIFFS.h>
#include <FS.h>
#include <ArduinoJson.h>

#include "../devices/device.h"
#include "storage.h"

class nvMemory
{
public: 
    nvMemory();
    ~nvMemory();
    bool saveConfig(TSettings* Settings);
    bool loadConfig(TSettings* Settings);
    bool deleteConfig();

private:
    bool init();
    bool initNVS();
    bool initSPIFFS();
    bool Initialized_;
};

#if !defined(NVMEM_SPIFFS) && !defined(NVMEM_NVS)
#error We need some kind of permanent storage implementation!
#endif

#endif // _NVMEMORY_H_
