#include <Arduino.h>
#include "wManager.h"
#include "settings.h"
#include "drivers/storage/nvMemory.h"
#include "drivers/storage/storage.h"
#include <SPIFFS.h>
#include <FS.h>
#include <ArduinoJson.h>
#include "monitor.h"
#include "drivers/storage/SDCard.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include "drivers/displays/display.h"
#include "version.h"

// Global instances
WebServer webServer(80);
DNSServer dnsServer;
bool portalRunning = false;
unsigned long portalStartTime = 0;

extern monitor_data mMonitor;
extern nvMemory nvMem;
extern SDCard SDCrd;

// Memory tracking
static size_t initial_free_heap = 0;
static size_t min_free_heap = 0;

// Flag for saving data
bool shouldSaveConfig = false;

// Variables to hold data from custom textboxes
TSettings Settings;

// Pool definitions
const PoolConfig POOLS[] = {
    {"Custom", "", 0, "", "Enter custom pool details"},
    {"public-pool.io", "public-pool.io", 21496, "https://web.public-pool.io", "Open Source Solo Bitcoin Mining Pool supporting open source miners"},
    {"NerdMiners.org", "pool.nerdminers.org", 3333, "https://nerdminers.org", "The official Nerdminer pool site - Maintained by @golden-guy"},
    {"NerdMiner.io", "pool.nerdminer.io", 3333, "https://nerdminer.io", "Maintained by CHMEX"},
    {"PyBlock.xyz", "pool.pyblock.xyz", 3333, "https://pool.pyblock.xyz/", "Maintained by curly60e"},
    {"SethForPrivacy", "pool.sethforprivacy.com", 3333, "https://pool.sethforprivacy.com/", "Maintained by @sethforprivacy - public-pool fork"}
};

const size_t POOL_COUNT = sizeof(POOLS) / sizeof(POOLS[0]);

// Timezone definitions
const TimezoneInfo TIMEZONE_LIST[] = {
    {"(UTC-12:00) Baker Island", -12},
    {"(UTC-11:00) American Samoa", -11},
    {"(UTC-10:00) Hawaii", -10},
    {"(UTC-09:00) Alaska", -9},
    {"(UTC-08:00) Pacific Time (US)", -8},
    {"(UTC-07:00) Mountain Time (US)", -7},
    {"(UTC-06:00) Central Time (US)", -6},
    {"(UTC-05:00) Eastern Time (US)", -5},
    {"(UTC-04:00) Atlantic Time (Canada)", -4},
    {"(UTC-03:00) Buenos Aires", -3},
    {"(UTC-02:00) South Georgia", -2},
    {"(UTC-01:00) Azores", -1},
    {"(UTC+00:00) London, Dublin", 0},
    {"(UTC+01:00) Berlin, Paris, Rome", 1},
    {"(UTC+02:00) Cairo, Athens", 2},
    {"(UTC+03:00) Moscow, Istanbul", 3},
    {"(UTC+04:00) Dubai, Baku", 4},
    {"(UTC+05:00) Karachi, Tashkent", 5},
    {"(UTC+05:30) Mumbai, Colombo", 5},
    {"(UTC+06:00) Dhaka, Almaty", 6},
    {"(UTC+07:00) Bangkok, Jakarta", 7},
    {"(UTC+08:00) Beijing, Singapore", 8},
    {"(UTC+09:00) Tokyo, Seoul", 9},
    {"(UTC+10:00) Sydney, Brisbane", 10},
    {"(UTC+11:00) Solomon Islands", 11},
    {"(UTC+12:00) Auckland, Fiji", 12}
};

const int TIMEZONE_COUNT = sizeof(TIMEZONE_LIST) / sizeof(TIMEZONE_LIST[0]);

// Helper function to generate client-side JavaScript
String generateClientScript() {
    String js = "";
    js += "let pools = [];";
    js += "let currentSettings = {};";
    
    js += "async function loadData() {";
    js += "  try {";
    js += "    pools = await fetch('/api/pools').then(r => r.json());";
    js += "    currentSettings = await fetch('/api/settings').then(r => r.json());";
    js += "    populatePoolSelect();";
    js += "    updatePoolFields();";
    js += "  } catch (error) {";
    js += "    console.error('Error loading data:', error);";
    js += "    alert('Error loading configuration data. Please refresh the page.');";
    js += "  }";
    js += "}";
    
    js += "function populatePoolSelect() {";
    js += "  const select = document.getElementById('poolSelect');";
    js += "  select.innerHTML = '';";
    js += "  pools.forEach((pool, index) => {";
    js += "    const option = document.createElement('option');";
    js += "    option.value = index;";
    js += "    option.text = pool.name;";
    js += "    if (pool.url === currentSettings.poolUrl && pool.port === currentSettings.poolPort) {";
    js += "      option.selected = true;";
    js += "    }";
    js += "    select.appendChild(option);";
    js += "  });";
    js += "}";
    
    js += "function updatePoolFields() {";
    js += "  const select = document.getElementById('poolSelect');";
    js += "  const poolUrl = document.getElementById('poolUrl');";
    js += "  const poolPort = document.getElementById('poolPort');";
    js += "  const poolInfo = document.getElementById('poolInfo');";
    js += "  const poolWebUrl = document.getElementById('poolWebUrl');";
    js += "  const customFields = document.getElementById('customPoolFields');";
    js += "  const selectedPool = pools[select.value];";
    
    js += "  if (select.value === '0') {";
    js += "    poolUrl.value = currentSettings.poolUrl || '';";
    js += "    poolPort.value = currentSettings.poolPort || '';";
    js += "    poolUrl.disabled = false;";
    js += "    poolPort.disabled = false;";
    js += "    poolInfo.innerHTML = selectedPool.info;";
    js += "    poolWebUrl.innerHTML = '';";
    js += "    customFields.style.display = 'block';";
    js += "  } else {";
    js += "    poolUrl.value = selectedPool.url;";
    js += "    poolPort.value = selectedPool.port;";
    js += "    poolUrl.disabled = true;";
    js += "    poolPort.disabled = true;";
    js += "    poolInfo.innerHTML = selectedPool.info;";
    js += "    customFields.style.display = 'none';";
    js += "    if (selectedPool.webUrl) {";
    js += "      poolWebUrl.innerHTML = `<a href=\"${selectedPool.webUrl}\" target=\"_blank\">Visit Pool Website</a>`;";
    js += "    } else {";
    js += "      poolWebUrl.innerHTML = '';";
    js += "    }";
    js += "  }";
    js += "}";
    
    js += "document.addEventListener('DOMContentLoaded', loadData);";
    return js;
}

// Helper function to generate CSS styles
String generateStyles() {
    String css = "";
    css += "body { font-family: 'Courier New', monospace; margin: 0; padding: 20px; background: #1a1a1a; color: #00ff00; }";
    css += ".container { max-width: 800px; margin: 0 auto; background: #2d2d2d; padding: 30px; border-radius: 10px; box-shadow: 0 0 20px rgba(0, 255, 0, 0.1); }";
    css += "h1 { text-align: center; color: #00ff00; text-shadow: 0 0 10px rgba(0, 255, 0, 0.5); margin-bottom: 30px; }";
    
    // Animation keyframes for left and right axes
    css += "@keyframes swingLeft { ";
    css += "0% { transform: rotate(0deg); }";
    css += "25% { transform: rotate(-30deg); }";
    css += "50% { transform: rotate(0deg); }";
    css += "100% { transform: rotate(0deg); }";
    css += "}";
    
    css += "@keyframes swingRight { ";
    css += "0% { transform: rotate(0deg); }";
    css += "25% { transform: rotate(30deg); }";
    css += "50% { transform: rotate(0deg); }";
    css += "100% { transform: rotate(0deg); }";
    css += "}";
    
    css += ".axe-left { display: inline-block; animation: swingLeft 2s ease-in-out infinite; transform-origin: 50% 50%; }";
    css += ".axe-right { display: inline-block; animation: swingRight 2s ease-in-out infinite; transform-origin: 50% 50%; animation-delay: 1s; }";
    
    css += "select, input { width: 100%; padding: 12px; margin: 8px 0 20px 0; background: #1a1a1a; border: 1px solid #00ff00; color: #00ff00; border-radius: 5px; }";
    css += "select:focus, input:focus { outline: none; box-shadow: 0 0 10px rgba(0, 255, 0, 0.5); }";
    css += ".pool-info { margin: 10px 0; padding: 10px; border-left: 3px solid #00ff00; background: #1a1a1a; }";
    css += ".pool-web-url { margin: 10px 0; }";
    css += ".pool-web-url a { color: #00ff00; text-decoration: none; }";
    css += ".pool-web-url a:hover { text-decoration: underline; }";
    css += "label { display: block; color: #00ff00; margin-bottom: 5px; }";
    css += "input[type='submit'] { background: #00ff00; color: #1a1a1a; font-weight: bold; cursor: pointer; transition: all 0.3s; }";
    css += "input[type='submit']:hover { background: #1a1a1a; color: #00ff00; }";
    css += ".section { margin-bottom: 30px; padding: 20px; border: 1px solid #00ff00; border-radius: 5px; }";
    css += ".section-title { color: #00ff00; margin-bottom: 20px; border-bottom: 1px solid #00ff00; padding-bottom: 10px; }";
    css += "@keyframes spin { 0% { transform: rotate(0deg); } 100% { transform: rotate(360deg); } }";
    css += ".spinner { width: 50px; height: 50px; border: 5px solid #1a1a1a; border-top: 5px solid #00ff00; border-radius: 50%; animation: spin 1s linear infinite; margin: 0 auto 20px; }";
    css += ".toggle-btn { background: #00ff00; color: #1a1a1a; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; margin-bottom: 20px; font-weight: bold; width: 100%; }";
    css += ".toggle-btn:hover { background: #1a1a1a; color: #00ff00; border: 1px solid #00ff00; }";
    return css;
}

// Helper function to generate JSON response
String generateJsonResponse(bool success, const char* message = nullptr) {
    String json = "{\"success\":" + String(success ? "true" : "false");
    if (message) {
        json += ",\"message\":\"" + String(message) + "\"";
    }
    json += "}";
    return json;
}

void setupWebServer() {
    webServer.on("/", HTTP_GET, []() {
        String html = "<!DOCTYPE html>";
        html += "<html><head>";
        html += "<meta charset='UTF-8'>";
        html += "<meta http-equiv='Content-Type' content='text/html; charset=UTF-8'>";
        html += "<title>NerdMiner Configuration</title>";
        html += "<style>" + generateStyles() + "</style>";
        html += "<script>" + generateClientScript() + "</script>";
        html += "</head><body><div class='container'>";
        html += "<h1><span class='axe-left'>&#x26CF;</span> NerdMiner Configuration <span class='axe-right'>&#x26CF;</span></h1>";
        
        // Form sections
        html += "<form method='POST' action='/save'>";
        
        // Pool Configuration Section
        html += "<div class='section'>";
        html += "<h2 class='section-title'>Pool Configuration</h2>";
        html += "<label for='poolSelect'>Select Pool:</label>";
        html += "<select id='poolSelect' name='poolSelect' onchange='updatePoolFields()'></select>";
        html += "<div id='poolInfo' class='pool-info'></div>";
        html += "<div id='poolWebUrl' class='pool-web-url'></div>";
        html += "<div id='customPoolFields' class='custom-pool-fields' style='display: none;'>";
        html += "<label for='poolUrl'>Pool URL:</label>";
        html += "<input type='text' id='poolUrl' name='poolUrl'>";
        html += "<label for='poolPort'>Pool Port:</label>";
        html += "<input type='number' id='poolPort' name='poolPort'>";
        html += "</div>";
        html += "</div>";

        // Wallet Configuration Section
        html += "<div class='section'>";
        html += "<h2 class='section-title'>Wallet Configuration</h2>";
        html += "<label for='wallet'>BTC Wallet Address:</label>";
        html += "<input type='text' id='wallet' name='wallet' value='" + String(Settings.BtcWallet) + "'>";
        html += "<label for='password'>Pool Password (optional):</label>";
        html += "<input type='text' id='password' name='password' value='" + String(Settings.PoolPassword) + "'>";
        html += "</div>";

        // Display Configuration Section
        html += "<div class='section'>";
        html += "<h2 class='section-title'>Display Configuration</h2>";
        html += "<label for='display_enabled'>Display Enabled:</label>";
        html += "<select name='display_enabled' id='display_enabled'>";
        html += "<option value='1'" + String(Settings.displayEnabled ? " selected" : "") + ">No</option>";
        html += "<option value='0'" + String(!Settings.displayEnabled ? " selected" : "") + ">Yes</option>";
        html += "</select>";

        html += "<h2 class='section-title'>Stats Configuration</h2>";

        html += "<label for='timezone'>Timezone:</label><br>";
        html += "<select name='timezone'>";
        for (int i = 0; i < TIMEZONE_COUNT; i++) {
            html += "<option value='" + String(TIMEZONE_LIST[i].offset) + "'";
            if (TIMEZONE_LIST[i].offset == Settings.Timezone) {
                html += " selected";
            }
            html += ">" + String(TIMEZONE_LIST[i].name) + "</option>";
        }
        html += "</select><br>";

        // Save Statistics
        html += "<label for='save_stats'>Save Statistics:</label><br>";
        html += "<select name='save_stats'>";
        html += "<option value='1'" + String(Settings.saveStats ? " selected" : "") + ">Yes</option>";
        html += "<option value='0'" + String(!Settings.saveStats ? " selected" : "") + ">No</option>";
        html += "</select><br>";

        // Add loading animation styles
        html += "<style>";
        html += ".loading { display: none; position: fixed; top: 0; left: 0; width: 100%; height: 100%; background: rgba(0,0,0,0.9); z-index: 1000; }";
        html += ".loading-content { position: absolute; top: 50%; left: 50%; transform: translate(-50%, -50%); text-align: center; color: #00ff00; }";
        html += ".spinner { width: 50px; height: 50px; border: 5px solid #1a1a1a; border-top: 5px solid #00ff00; border-radius: 50%; animation: spin 1s linear infinite; margin: 0 auto 20px; }";
        html += "@keyframes spin { 0% { transform: rotate(0deg); } 100% { transform: rotate(360deg); } }";
        html += ".container { text-align: center; }";
        html += "</style>";
        
        // Add loading overlay
        html += "<div id='loadingOverlay' class='loading'>";
        html += "<div class='loading-content'>";
        html += "<div class='spinner'></div>";
        html += "<div id='statusText'>Saving configuration...</div>";
        html += "</div>";
        html += "</div>";

        // Submit Button
        html += "<input type='submit' value='Save Configuration' class='submit-btn'>";
        html += "</form>";

        html += "<div id='overlay' class='overlay' style='display: none;'>";
        html += "<div class='overlay-content'>";
        html += "<div class='spinner'></div>";
        html += "<p>Saving configuration...</p>";
        html += "</div>";
        html += "</div>";

        html += "</div></body></html>";
        
        webServer.sendHeader("Content-Type", "text/html; charset=UTF-8");
        webServer.send(200, "text/html", html);
    });

    // Add API endpoints for data
    webServer.on("/api/pools", HTTP_GET, []() {
        String json = "[";
        for (size_t i = 0; i < POOL_COUNT; i++) {
            if (i > 0) json += ",";
            json += "{";
            json += "\"name\":\"" + String(POOLS[i].name) + "\",";
            json += "\"url\":\"" + String(POOLS[i].url) + "\",";
            json += "\"port\":" + String(POOLS[i].port) + ",";
            json += "\"webUrl\":\"" + String(POOLS[i].webUrl) + "\",";
            json += "\"info\":\"" + String(POOLS[i].info) + "\"";
            json += "}";
        }
        json += "]";
        webServer.send(200, "application/json", json);
    });

    webServer.on("/api/settings", HTTP_GET, []() {
        String json = "{";
        json += "\"poolUrl\":\"" + String(Settings.PoolAddress) + "\",";
        json += "\"poolPort\":" + String(Settings.PoolPort) + ",";
        json += "\"wallet\":\"" + String(Settings.BtcWallet) + "\",";
        json += "\"password\":\"" + String(Settings.PoolPassword) + "\",";
        json += "\"timezone\":" + String(Settings.Timezone) + ",";
        json += "\"saveStats\":" + String(Settings.saveStats ? "true" : "false") + ",";
        json += "\"displayEnabled\":" + String(Settings.displayEnabled ? "true" : "false");
        json += "}";
        webServer.send(200, "application/json", json);
    });

    // Update save endpoint handler
    webServer.on("/save", HTTP_POST, []() {
        Serial.println("Save endpoint called");

        // Get form data
        String poolSelect = webServer.arg("poolSelect");
        String poolUrl = webServer.arg("poolUrl");
        String poolPort = webServer.arg("poolPort");
        String wallet = webServer.arg("wallet");
        String password = webServer.arg("password");
        String timezone = webServer.arg("timezone");
        String save_stats = webServer.arg("save_stats");
        String display_enabled = webServer.arg("display_enabled");

        // Validate required fields
        if (wallet.length() == 0) {
            webServer.send(400, "application/json", generateJsonResponse(false, "Wallet address is required"));
            return;
        }

        // Update pool settings based on selection
        if (poolSelect != "0") { // Not custom pool
            int poolIndex = poolSelect.toInt();
            if (poolIndex >= 0 && poolIndex < POOL_COUNT) {
                Settings.PoolAddress = POOLS[poolIndex].url;
                Settings.PoolPort = POOLS[poolIndex].port;
            } else {
                webServer.send(400, "application/json", generateJsonResponse(false, "Invalid pool selection"));
                return;
            }
        } else {
            // Validate custom pool settings
            if (poolUrl.length() == 0 || poolPort.length() == 0) {
                webServer.send(400, "application/json", generateJsonResponse(false, "Pool URL and Port are required for custom pool"));
                return;
            }
            Settings.PoolAddress = poolUrl;
            Settings.PoolPort = poolPort.toInt();
        }

        // Update other settings
        strncpy(Settings.BtcWallet, wallet.c_str(), sizeof(Settings.BtcWallet) - 1);
        Settings.BtcWallet[sizeof(Settings.BtcWallet) - 1] = '\0';

        if (password.length() > 0) {
            strncpy(Settings.PoolPassword, password.c_str(), sizeof(Settings.PoolPassword) - 1);
            Settings.PoolPassword[sizeof(Settings.PoolPassword) - 1] = '\0';
        }

        if (timezone.length() > 0) {
            Settings.Timezone = timezone.toInt();
        }

        Settings.saveStats = (save_stats == "1");
        Settings.displayEnabled = (display_enabled == "1");

        // Create response page
        String response = "<html><head>";
        response += "<meta http-equiv='refresh' content='2;url=/'>";
        response += "<style>";
        response += generateStyles();
        response += "</style>";
        response += "</head><body><div class='container'>";
        response += "<div class='spinner'></div>";

        try {
            saveNewConfig();
            response += "<h2>Configuration saved!</h2>";
            response += "<p>Device is restarting...</p>";
            webServer.sendHeader("Content-Type", "text/html; charset=UTF-8");
            webServer.send(200, "text/html", response);
            delay(500);  // Give time for the response to be sent
            ESP.restart();
        } catch (const std::exception& e) {
            Serial.println("Failed to save configuration");
            response += "<h2>Failed to save configuration</h2>";
            response += "<p>Please try again</p>";
            webServer.sendHeader("Content-Type", "text/html; charset=UTF-8");
            webServer.send(200, "text/html", response);
        }
    });

    // Add ping and restart endpoints
    webServer.on("/ping", HTTP_GET, []() {
        webServer.send(200, "text/plain", "pong");
    });

    webServer.on("/restart", HTTP_GET, []() {
        webServer.send(200, "application/json", "{\"success\":true}");
        delay(100);  // Small delay to ensure response is sent
        ESP.restart();
    });

    // Handle not found
    webServer.onNotFound([]() {
        webServer.sendHeader("Location", "/", true);
        webServer.send(302, "text/plain", "");
    });
}

void saveNewConfig() {
    Serial.println("Starting saveNewConfig");

    // Save settings to NVS
    nvMemory nvMem;
    if (!nvMem.saveConfig(&Settings)) {
        throw std::runtime_error("Failed to save settings to NVS");
    }

    // Update display state to match settings
    toggleDisplay(Settings.displayEnabled);

    Serial.println("saveNewConfig completed successfully");
}

void init_WifiManager() {
    initial_free_heap = ESP.getFreeHeap();
    min_free_heap = initial_free_heap;

    // Load settings from NVS
    nvMemory nvMem;
    if (!nvMem.loadConfig(&Settings)) {
        Serial.println("Failed to load settings from NVS, using defaults");
    }

    // Initialize display state from settings
    toggleDisplay(Settings.displayEnabled);

    WiFi.mode(WIFI_STA);
    bool forceConfig = false;

#if defined(PIN_BUTTON_2)
    if (!digitalRead(PIN_BUTTON_2)) {
        Serial.println(F("Button pressed to force start config mode"));
        forceConfig = true;
    }
#endif

    if (!nvMem.loadConfig(&Settings)) {
        if (SDCrd.loadConfigFile(&Settings)) {
            SDCrd.SD2nvMemory(&nvMem, &Settings);          
        } else {
            forceConfig = true;
        }
    }
    
    SDCrd.terminate();

    mMonitor.NerdStatus = NM_Connecting;

    if (forceConfig) {
        drawSetupScreen();
        mMonitor.NerdStatus = NM_waitingConfig;
        
        // Start captive portal
        WiFi.mode(WIFI_AP);
        WiFi.softAP("NerdMiner", "nerdminer");
        delay(100);
        
        IPAddress apIP(192, 168, 4, 1);
        WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
        
        // Start DNS server
        dnsServer.start(DNS_PORT, "*", apIP);

        // Setup web server routes
        setupWebServer();
        webServer.begin();
        portalRunning = true;
        portalStartTime = millis();
        
        Serial.println("Configuration portal started");
        Serial.print("AP IP address: ");
        Serial.println(WiFi.softAPIP());
    } else {
        // Try to connect to saved WiFi
        WiFi.begin();
        
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("\nFailed to connect to saved WiFi");
            drawSetupScreen();
            mMonitor.NerdStatus = NM_waitingConfig;
            
            // Start captive portal
            WiFi.mode(WIFI_AP);
            WiFi.softAP("NerdMiner", "nerdminer");
            delay(100);
            
            IPAddress apIP(192, 168, 4, 1);
            WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
            
            dnsServer.start(DNS_PORT, "*", apIP);
            setupWebServer();
            webServer.begin();
            portalRunning = true;
            portalStartTime = millis();
            
            Serial.println("Configuration portal started");
            Serial.print("AP IP address: ");
            Serial.println(WiFi.softAPIP());
        } else {
            Serial.println("\nConnected to WiFi");
            Serial.print("IP address: ");
            Serial.println(WiFi.localIP());
            mMonitor.NerdStatus = NM_Connected;
            
            // Start web server in regular WiFi mode too
            setupWebServer();
            webServer.begin();
            portalRunning = true;  // Keep web server running
        }
    }
}

void wifiManagerProcess() {
    if (portalRunning) {
        if (WiFi.getMode() == WIFI_AP) {
            dnsServer.processNextRequest();
        }
        webServer.handleClient();
        
        // Only check timeout in AP mode
        if (WiFi.getMode() == WIFI_AP && millis() - portalStartTime > CONFIG_PORTAL_TIMEOUT) {
            Serial.println("Portal timeout reached");
            WiFi.softAPdisconnect(true);
            webServer.stop();
            dnsServer.stop();
            portalRunning = false;
            ESP.restart();
            return;
        }
    }
    
    // Check if WiFi is still connected when not in AP mode
    if (WiFi.getMode() != WIFI_AP) {
        static bool wasConnected = false;
        bool isConnected = WiFi.status() == WL_CONNECTED;
        
        if (isConnected != wasConnected) {
            if (isConnected) {
                Serial.println("WiFi reconnected");
                mMonitor.NerdStatus = NM_Connected;
                // Restart web server after reconnection
                if (!portalRunning) {
                    setupWebServer();
                    webServer.begin();
                    portalRunning = true;
                }
            } else {
                Serial.println("WiFi connection lost");
                mMonitor.NerdStatus = NM_Connecting;
                // Stop web server when disconnected
                if (portalRunning) {
                    webServer.stop();
                    portalRunning = false;
                }
            }
            wasConnected = isConnected;
        }

#if defined(PIN_BUTTON_2)
        // Check for config portal trigger
        if (!digitalRead(PIN_BUTTON_2)) {
            Serial.println("Button pressed, starting config portal");
            drawSetupScreen();
            mMonitor.NerdStatus = NM_waitingConfig;
            
            WiFi.disconnect();
            WiFi.mode(WIFI_AP);
            WiFi.softAP("NerdMiner", "nerdminer");
            delay(100);
            
            IPAddress apIP(192, 168, 4, 1);
            WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
            
            dnsServer.start(DNS_PORT, "*", apIP);
            setupWebServer();
            webServer.begin();
            portalRunning = true;
            portalStartTime = millis();
        }
#endif
    }
}

void reset_configuration() {
    Serial.println("Resetting configuration...");
    nvMem.deleteConfig();
    ESP.restart();
}

size_t getFreeHeap() {
    size_t free_heap = ESP.getFreeHeap();
    if (free_heap < min_free_heap) {
        min_free_heap = free_heap;
    }
    return free_heap;
}

size_t getMinFreeHeap() {
    return min_free_heap;
}
