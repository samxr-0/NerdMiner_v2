#include "nvMemory.h"

#ifdef NVMEM_SPIFFS

#include <SPIFFS.h>
#include <FS.h>
#include <ArduinoJson.h>

#include "../devices/device.h"
#include "storage.h"

nvMemory::nvMemory() : Initialized_(false){};

nvMemory::~nvMemory()
{
    if (Initialized_)
        SPIFFS.end();
};

/// @brief Save settings to config file on SPIFFS
/// @param TSettings* Settings to be saved.
/// @return true on success
bool nvMemory::saveConfig(TSettings* Settings)
{
    if (init())
    {
        Serial.println(F("SPIFS: Saving configuration."));

        StaticJsonDocument<512> json;
        
        // WiFi credentials
        json["wifiSSID"] = Settings->WifiSSID;
        json["wifiPassword"] = Settings->WifiPW;

        // Pool settings
        json["poolString"] = Settings->PoolAddress;
        json["portNumber"] = Settings->PoolPort;
        json["poolPassword"] = Settings->PoolPassword;
        json["btcString"] = Settings->BtcWallet;
        
        json["gmtZone"] = Settings->Timezone;
        json["saveStatsToNVS"] = Settings->saveStats;
        json["invertColors"] = Settings->invertColors;
        
        // Add log level to JSON configuration
        json["logLevel"] = static_cast<int>(Settings->LogLevel);

        File configFile = SPIFFS.open(JSON_CONFIG_FILE, "w");
        if (!configFile)
        {
            Serial.println("SPIFS: Failed to open config file for writing");
            return false;
        }

        serializeJsonPretty(json, Serial);
        Serial.print('\n');
        if (serializeJson(json, configFile) == 0)
        {
            Serial.println(F("SPIFS: Failed to write to file"));
            return false;
        }
        
        configFile.close();
        return true;
    }
    return false;
}

/// @brief Load settings from config file located in SPIFFS.
/// @param TSettings* Struct to update with new settings.
/// @return true on success
bool nvMemory::loadConfig(TSettings* Settings)
{
    // Uncomment if we need to format filesystem
    // SPIFFS.format();

    // Load existing configuration file
    // Read configuration from FS json

    if (init())
    {
        if (SPIFFS.exists(JSON_CONFIG_FILE))
        {
            File configFile = SPIFFS.open(JSON_CONFIG_FILE, "r");
            if (configFile)
            {
                Serial.println("SPIFS: Loading config file");
                StaticJsonDocument<512> json;
                DeserializationError error = deserializeJson(json, configFile);
                configFile.close();
                serializeJsonPretty(json, Serial);
                Serial.print('\n');
                if (!error)
                {
                    // WiFi credentials
                    Settings->WifiSSID = json["wifiSSID"] | Settings->WifiSSID;
                    Settings->WifiPW = json["wifiPassword"] | Settings->WifiPW;

                    // Pool settings
                    Settings->PoolAddress = json["poolString"] | Settings->PoolAddress;
                    if (json.containsKey("portNumber"))
                        Settings->PoolPort = json["portNumber"].as<int>();
                    
                    strcpy(Settings->PoolPassword, json["poolPassword"] | Settings->PoolPassword);
                    strcpy(Settings->BtcWallet, json["btcString"] | Settings->BtcWallet);
                    
                    if (json.containsKey("gmtZone"))
                        Settings->Timezone = json["gmtZone"].as<int>();
                    
                    if (json.containsKey("saveStatsToNVS"))
                        Settings->saveStats = json["saveStatsToNVS"].as<bool>();
                    
                    if (json.containsKey("invertColors")) {
                        Settings->invertColors = json["invertColors"].as<bool>();
                    } else {
                        Settings->invertColors = false;
                    }
                    
                    // Add log level configuration
                    if (json.containsKey("logLevel")) {
                        int logLevelValue = json["logLevel"].as<int>();
                        // Validate log level is within enum range
                        if (logLevelValue >= LOG_NONE && logLevelValue <= LOG_DEBUG) {
                            Settings->LogLevel = static_cast<LogVerbosity>(logLevelValue);
                        } else {
                            // Default to LOG_INFO if invalid
                            Settings->LogLevel = LOG_INFO;
                        }
                    } else {
                        // Default to LOG_INFO if not specified
                        Settings->LogLevel = LOG_INFO;
                    }
                    
                    return true;
                }
                else
                {
                    Serial.println("SPIFS: Error parsing config file!");
                }
            }
            else
            {
                Serial.println("SPIFS: Error opening config file!");
            }
        }
        else
        {
            Serial.println("SPIFS: No config file available!");
        }
    }
    return false;
}

/// @brief Delete config file from SPIFFS
/// @return true on successs
bool nvMemory::deleteConfig()
{
    Serial.println("SPIFS: Erasing config file..");
    return SPIFFS.remove(JSON_CONFIG_FILE); //Borramos fichero
}

/// @brief Prepare and mount SPIFFS
/// @return true on success
bool nvMemory::init()
{
    if (!Initialized_)
    {
        Serial.println("SPIFS: Mounting File System...");
        // May need to make it begin(true) first time you are using SPIFFS
        Initialized_ = SPIFFS.begin(false) || SPIFFS.begin(true);
        Initialized_ ? Serial.println("SPIFS: Mounted") : Serial.println("SPIFS: Mounting failed.");
    }
    else
    {
        Serial.println("SPIFS: Already Mounted");
    }
    return Initialized_;
};

#else

nvMemory::nvMemory() {}
nvMemory::~nvMemory() {}
bool nvMemory::saveConfig(TSettings* Settings) { return false; }
bool nvMemory::loadConfig(TSettings* Settings) { return false; }
bool nvMemory::deleteConfig() { return false; }
bool nvMemory::init() { return false; }


#endif //NVMEM_TYPE