#define ESP_DRD_USE_SPIFFS true

// Include Libraries
//#include ".h"

#include <WiFi.h>

#include <WiFiManager.h>

#include "wManager.h"
#include "monitor.h"
#include "drivers/displays/display.h"
#include "drivers/storage/SDCard.h"
#include "drivers/storage/nvMemory.h"
#include "drivers/storage/storage.h"
#include "mining.h"
#include "timeconst.h"


// Flag for saving data
bool shouldSaveConfig = false;

// Variables to hold data from custom textboxes
TSettings Settings;

// Define WiFiManager Object
WiFiManager wm;
extern monitor_data mMonitor;

nvMemory nvMem;

extern SDCard SDCrd;

// Store parameter references
WiFiManagerParameter* pool_param;
WiFiManagerParameter* port_param;
WiFiManagerParameter* wallet_param;
WiFiManagerParameter* timezone_param;
WiFiManagerParameter* save_stats_param;
WiFiManagerParameter* password_param;
#ifdef ESP32_2432S028R
WiFiManagerParameter* invert_display_param;
#endif

// Menu items
const char* MENU_ITEMS[] = {"wifi", "param", "info", "sep", "restart", "exit"};
const uint8_t MENU_SIZE = 6;

// Function prototypes
void setupParameters();
void setupMenu();
void saveNewConfig(WiFiManagerParameter* pool, WiFiManagerParameter* port, 
                  WiFiManagerParameter* password, WiFiManagerParameter* wallet,
                  WiFiManagerParameter* timezone, WiFiManagerParameter* save_stats
#ifdef ESP32_2432S028R
                  , WiFiManagerParameter* invert_colors
#endif
);

void setupMenu() {
    wm.setMenu((const char**)MENU_ITEMS, MENU_SIZE);
}

void saveConfigCallback()
// Callback notifying us of the need to save configuration
{
    Serial.println("Should save config");
    shouldSaveConfig = true;    
}

void saveParamsCallback() {
    Serial.println("Saving parameters");
    
    // Get the parameter values
    Settings.PoolAddress = String(pool_param->getValue());
    Settings.PoolPort = atoi(port_param->getValue());
    strncpy(Settings.BtcWallet, wallet_param->getValue(), sizeof(Settings.BtcWallet) - 1);
    Settings.Timezone = atoi(timezone_param->getValue());
    Settings.saveStats = (strncmp(save_stats_param->getValue(), "T", 1) == 0);
    strncpy(Settings.PoolPassword, password_param->getValue(), sizeof(Settings.PoolPassword) - 1);
    
#ifdef ESP32_2432S028R
    Settings.invertColors = (strncmp(invert_display_param->getValue(), "T", 1) == 0);
#endif

    // Save to non-volatile memory
    nvMem.saveConfig(&Settings);
    Serial.println("Parameters saved");
}

void saveNewConfig(WiFiManagerParameter* pool, WiFiManagerParameter* port, 
                  WiFiManagerParameter* password, WiFiManagerParameter* wallet,
                  WiFiManagerParameter* timezone, WiFiManagerParameter* save_stats
#ifdef ESP32_2432S028R
                  , WiFiManagerParameter* invert_colors
#endif
) {
    if (!pool || !port || !wallet || !timezone || !save_stats) {
        Serial.println("Error: Invalid parameter pointers");
        return;
    }

    Settings.PoolAddress = String(pool->getValue());
    Settings.PoolPort = atoi(port->getValue());
    strncpy(Settings.PoolPassword, password->getValue(), sizeof(Settings.PoolPassword));
    strncpy(Settings.BtcWallet, wallet->getValue(), sizeof(Settings.BtcWallet));
    Settings.Timezone = atoi(timezone->getValue());
    Settings.saveStats = (strncmp(save_stats->getValue(), "T", 1) == 0);
#ifdef ESP32_2432S028R
    Settings.invertColors = (strncmp(invert_colors->getValue(), "T", 1) == 0);
#endif

    // Debug output
    Serial.println("\nNew Configuration:");
    Serial.print("Pool URL: "); Serial.println(Settings.PoolAddress);
    Serial.print("Port: "); Serial.println(Settings.PoolPort);
    Serial.print("BTC Wallet: "); Serial.println(Settings.BtcWallet);
    Serial.print("Timezone: "); Serial.println(Settings.Timezone);
    Serial.print("Save Stats: "); Serial.println(Settings.saveStats);
#ifdef ESP32_2432S028R
    Serial.print("Invert Colors: "); Serial.println(Settings.invertColors);
#endif
}

void configModeCallback(WiFiManager* myWiFiManager)
// Called when config mode launched
{
    Serial.println("Entered Configuration Mode");
    drawSetupScreen();
    Serial.print("Config SSID: ");
    Serial.println(myWiFiManager->getConfigPortalSSID());

    Serial.print("Config IP Address: ");
    Serial.println(WiFi.softAPIP());
}

void reset_configuration()
{
    Serial.println("Erasing Config, restarting");
    nvMem.deleteConfig();
    resetStat();
    wm.resetSettings();
    ESP.restart();
}

void init_WifiManager()
{
#ifdef MONITOR_SPEED
    Serial.begin(MONITOR_SPEED);
#else
    Serial.begin(115200);
#endif //MONITOR_SPEED

#ifdef PIN_ENABLE5V
    pinMode(PIN_ENABLE5V, OUTPUT);
    digitalWrite(PIN_ENABLE5V, HIGH);
#endif

    bool forceConfig = false;

#if defined(PIN_BUTTON_2)
    if (!digitalRead(PIN_BUTTON_2)) {
        Serial.println(F("Button pressed to force start config mode"));
        forceConfig = true;
    }
#endif

    WiFi.mode(WIFI_AP_STA);

    // Load configuration from non-volatile memory or SD card
    if (!nvMem.loadConfig(&Settings))
    {
        if (SDCrd.loadConfigFile(&Settings))
        {
            SDCrd.SD2nvMemory(&nvMem, &Settings);          
        }
        else
        {
            forceConfig = true;
        }
    };
    
    SDCrd.terminate();

    // Set dark theme and other WiFiManager settings
    wm.setClass("invert");
    wm.setDarkMode(true);
    wm.setShowPassword(false);
    wm.setHostname("NerdMiner");
    
    // Setup menu items
    setupMenu();
    setupParameters();
    
    // Configure portal settings
    wm.setShowInfoErase(false);  // Hide erase button
    wm.setShowInfoUpdate(false); // Hide update button
    wm.setParamsPage(true);      // Enable parameters page
    wm.setBreakAfterConfig(true);
    wm.setConfigPortalTimeout(0); // Disable timeout
    wm.setConnectTimeout(30);     // 30 seconds to attempt connection
    
    // Set callbacks
    wm.setSaveConfigCallback(saveConfigCallback);
    wm.setSaveParamsCallback(saveParamsCallback);
    wm.setAPCallback(configModeCallback);

    if (forceConfig)    
    {
        // Run if we need a configuration
        wm.setConfigPortalBlocking(true);
        drawSetupScreen();
        mMonitor.NerdStatus = NM_Connecting;
        if (!wm.startConfigPortal(DEFAULT_SSID, DEFAULT_WIFIPW))
        {
            Serial.println("Failed to connect or hit timeout");
            saveNewConfig(pool_param, port_param, password_param, 
                         wallet_param, timezone_param, save_stats_param
#ifdef ESP32_2432S028R
                         , invert_display_param
#endif
            );
            delay(3000);
            ESP.restart();
        }
    }
    else
    {
        mMonitor.NerdStatus = NM_Connecting;
        wm.setCaptivePortalEnable(true);
        wm.setConfigPortalBlocking(false);
        wm.setEnableConfigPortal(true);
        
        if (!wm.autoConnect(DEFAULT_SSID, DEFAULT_WIFIPW))
        {
            Serial.println("Failed to connect to configured WIFI");
            if (shouldSaveConfig) {
                saveNewConfig(pool_param, port_param, password_param, 
                            wallet_param, timezone_param, save_stats_param
#ifdef ESP32_2432S028R
                            , invert_display_param
#endif
                );
            }
            ESP.restart();
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi connected");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());

        // Start the web portal in non-blocking mode with all parameters
        wm.startWebPortal();
        Serial.println("Configuration portal available at: " + WiFi.localIP().toString());

        // Save current configuration
        saveNewConfig(pool_param, port_param, password_param, 
                     wallet_param, timezone_param, save_stats_param
#ifdef ESP32_2432S028R
                     , invert_display_param
#endif
        );

        if (shouldSaveConfig) {
            nvMem.saveConfig(&Settings);
#ifdef ESP32_2432S028R
            if (Settings.invertColors) ESP.restart();
#endif
        }
    }
}

void setupParameters()
{
    // Create parameters with current values
    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%d", Settings.PoolPort);
    
    char timezone_str[4];
    snprintf(timezone_str, sizeof(timezone_str), "%d", Settings.Timezone);
    
    // Add mining parameters
    pool_param = new WiFiManagerParameter("poolString", "Pool URL", Settings.PoolAddress.c_str(), 80);
    port_param = new WiFiManagerParameter("portNumber", "Pool Port", port_str, 7);
    wallet_param = new WiFiManagerParameter("btcString", "BTC Wallet", Settings.BtcWallet, 80);
    password_param = new WiFiManagerParameter("poolPassword", "Pool Password (Optional)", Settings.PoolPassword, 80);
    
    wm.addParameter(pool_param);
    wm.addParameter(port_param);
    wm.addParameter(wallet_param);
    wm.addParameter(password_param);
    
    // Add additional parameters
    timezone_param = new WiFiManagerParameter("gmtZone", "Time Zone", timezone_str, 3);
    wm.addParameter(timezone_param);
    
    char checkboxParams[24] = "type=\"checkbox\"";
    if (Settings.saveStats) {
        strcat(checkboxParams, " checked");
    }
    save_stats_param = new WiFiManagerParameter("saveStatsToNVS", "Save Statistics", "T", 2, checkboxParams, WFM_LABEL_AFTER);
    wm.addParameter(save_stats_param);

#ifdef ESP32_2432S028R
    char displayParams[24] = "type=\"checkbox\"";
    if (Settings.invertColors) {
        strcat(displayParams, " checked");
    }
    invert_display_param = new WiFiManagerParameter("invertColors", "Invert Display", "T", 2, displayParams, WFM_LABEL_AFTER);
    wm.addParameter(invert_display_param);
#endif
}

//----------------- MAIN PROCESS WIFI MANAGER --------------
void wifiManagerProcess() {
    static wl_status_t oldStatus = WL_IDLE_STATUS;
    wl_status_t newStatus = WiFi.status();
    
    if (newStatus != oldStatus) {
        if (newStatus == WL_CONNECTED) {
            Serial.println("CONNECTED - Current ip: " + WiFi.localIP().toString());
            // Ensure web portal is running after reconnection
            if (!wm.getWebPortalActive()) {
                wm.startWebPortal();
                Serial.println("Configuration portal available at: " + WiFi.localIP().toString());
            }
        } else {
            Serial.print("[WIFI] Disconnected - status: ");
            Serial.println(newStatus);
            mMonitor.NerdStatus = NM_waitingConfig;
        }
        oldStatus = newStatus;
    }

    // Process WiFiManager portal
    wm.process();
}
