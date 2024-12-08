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
#include <Preferences.h>
#include "drivers/displays/display.h"
#include "version.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_err.h"
#include "wifiManager.h"

// Global instances
WebServer webServer(80);
DNSServer dnsServer;
Preferences preferences;
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
    
    js += "function scanWiFi() {";
    js += "  const statusText = document.getElementById('wifiScanStatus');";
    js += "  const select = document.getElementById('wifiSelect');";
    js += "  const currentOption = select.querySelector('option[selected]');";
    js += "  statusText.textContent = 'Scanning...';";
    js += "  fetch('/scan-wifi')";
    js += "    .then(response => response.json())";
    js += "    .then(data => {";
    js += "      select.innerHTML = currentOption ? currentOption.outerHTML : '<option value=\"\">Select Network</option>';";
    js += "      data.networks.forEach(network => {";
    js += "        const option = document.createElement('option');";
    js += "        option.value = network.ssid;";
    js += "        option.text = `${network.ssid} (${network.rssi} dBm)`;";
    js += "        select.appendChild(option);";
    js += "      });";
    js += "      const customOption = document.createElement('option');";
    js += "      customOption.value = 'custom';";
    js += "      customOption.text = 'Enter Custom Network';";
    js += "      select.appendChild(customOption);";
    js += "      statusText.textContent = data.networks.length + ' networks found';";
    js += "    })";
    js += "    .catch(error => {";
    js += "      console.error('Scan WiFi Error:', error);";
    js += "      statusText.textContent = 'Scan failed: ' + error.message;";
    js += "    });";
    js += "}";
    
    js += "function toggleCustomNetwork() {";
    js += "  const select = document.getElementById('wifiSelect');";
    js += "  const customNetworkDiv = document.getElementById('customNetworkDiv');";
    js += "  customNetworkDiv.style.display = select.value === 'custom' ? 'block' : 'none';";
    js += "}";
    
    js += "function factoryReset() {";
    js += "  if (confirm('Are you sure you want to reset to factory defaults? This will erase all settings and restart the device.')) {";
    js += "    document.getElementById('loadingOverlay').style.display = 'flex';";
    js += "    document.getElementById('statusText').innerText = 'Resetting to factory defaults...';";
    js += "    fetch('/factory-reset', { method: 'POST' })";
    js += "      .then(response => {";
    js += "        if (!response.ok) throw new Error('Reset failed');";
    js += "        alert('Factory reset successful. Device will restart now. Please wait 30 seconds then reconnect to the device\\'s AP to reconfigure.');";
    js += "        setTimeout(() => { window.location.href = '/'; }, 5000);";
    js += "      })";
    js += "      .catch(error => {";
    js += "        console.error('Error:', error);";
    js += "        alert('Error during factory reset. Device will restart. Please reconnect to the AP mode to reconfigure.');";
    js += "        document.getElementById('loadingOverlay').style.display = 'none';";
    js += "      });";
    js += "  }";
    js += "}";
    
    js += "document.addEventListener('DOMContentLoaded', function() {";
    js += "  console.log('DOMContentLoaded event triggered');";
    js += "  const wifiSelect = document.getElementById('wifiSelect');";
    js += "  if (wifiSelect) {";
    js += "    wifiSelect.addEventListener('change', toggleCustomNetwork);";
    js += "  }";
    js += "  loadData();";
    js += "});";
    
    js += "window.onload = loadData;";
    
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
    css += ".factory-reset {";
    css += "  background-color: #dc3545;";
    css += "  color: white;";
    css += "  padding: 10px 20px;";
    css += "  border: none;";
    css += "  border-radius: 4px;";
    css += "  cursor: pointer;";
    css += "  margin-top: 20px;";
    css += "  width: 100%;";
    css += "}";
    css += ".factory-reset:hover {";
    css += "  background-color: #c82333;";
    css += "}";
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
    // Root route handler
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
        
        // WiFi Configuration Section
        html += "<div class='section'>";
        html += "<h2 class='section-title'>WiFi Configuration</h2>";
        html += "<button type='button' onclick='scanWiFi()'>Scan WiFi Networks</button>";
        html += "<div id='wifiScanStatus' style='color: #666; margin-top: 10px;'></div>";
        html += "<label for='wifiSelect'>Select WiFi Network:</label>";
        html += "<select id='wifiSelect' name='wifiSelect'>";
        html += "<option value=''>Select Network</option>";

        // Add current saved WiFi network as a pre-selected option
        if (!Settings.WifiSSID.isEmpty()) {
            html += "<option value='" + Settings.WifiSSID + "' selected>Current: " + Settings.WifiSSID + "</option>";
        }

        html += "</select>";
        html += "<div id='customNetworkDiv' style='display: none;'>";
        html += "<label for='customNetworkInput'>Enter Custom Network Name:</label>";
        html += "<input type='text' id='customNetworkInput' name='customNetworkInput' placeholder='Custom Network SSID'>";
        html += "</div>";
        html += "<label for='wifiPassword'>WiFi Password:</label>";
        html += "<input type='password' id='wifiPassword' name='wifiPassword'>";
        html += "</div>";

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
        
        // Add factory reset button with confirmation dialog
        html += "<script>";
        html += "function factoryReset() {";
        html += "  if (confirm('Are you sure you want to reset to factory defaults? This will erase all settings and restart the device.')) {";
        html += "    document.getElementById('loadingOverlay').style.display = 'flex';";
        html += "    document.getElementById('statusText').innerText = 'Resetting to factory defaults...';";
        html += "    fetch('/factory-reset', { method: 'POST' })";
        html += "      .then(response => {";
        html += "        if (!response.ok) throw new Error('Reset failed');";
        html += "        alert('Factory reset successful. Device will restart now. Please wait 30 seconds then reconnect to the device\\'s AP to reconfigure.');";
        html += "        setTimeout(() => { window.location.href = '/'; }, 5000);";  // Redirect after 5 seconds
        html += "      })";
        html += "      .catch(error => {";
        html += "        console.error('Error:', error);";
        html += "        alert('Error during factory reset. Device will restart. Please reconnect to the AP mode to reconfigure.');";
        html += "        document.getElementById('loadingOverlay').style.display = 'none';";
        html += "      });";
        html += "  }";
        html += "}";
        html += "</script>";
        
        // Add factory reset button with more spacing and warning color
        html += "<button onclick='factoryReset()' style='margin-top: 20px; background-color: #ff4444;' class='factory-reset'>Factory Reset</button>";
        
        html += "</div></body></html>";
        
        webServer.sendHeader("Content-Type", "text/html; charset=UTF-8");
        webServer.send(200, "text/html", html);
    });

    // Captive portal handling
    webServer.on("/generate_204", HTTP_GET, []() {
        webServer.sendHeader("Location", "/");
        webServer.send(302, "text/plain", "");
    });

    webServer.on("/fwlink", HTTP_GET, []() {
        webServer.sendHeader("Location", "/");
        webServer.send(302, "text/plain", "");
    });

    // WiFi scan route
    webServer.on("/scan-wifi", HTTP_GET, []() {
        int n = WiFi.scanNetworks();
        String json = "{\"networks\":[";
        for (int i = 0; i < n; ++i) {
            if (i > 0) json += ",";
            json += "{";
            json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
            json += "\"rssi\":" + String(WiFi.RSSI(i));
            json += "}";
        }
        json += "]}";
        
        webServer.send(200, "application/json", json);
        WiFi.scanDelete(); // Clean up scan results
    });

    // Configuration save route
    webServer.on("/save", HTTP_POST, []() {
        nvMemory nvMem;
        TSettings savedSettings;
        
        // First, try to load existing configuration
        if (!nvMem.loadConfig(&savedSettings)) {
            // If no existing config, initialize with defaults
            savedSettings = Settings;
        }

        // Validate and update settings
        if (webServer.hasArg("wifiSelect")) {
            String selectedSSID = webServer.arg("wifiSelect");
            String customSSID = webServer.arg("customNetworkInput");
            String wifiPassword = webServer.arg("wifiPassword");

            if (selectedSSID == "custom" && !customSSID.isEmpty()) {
                savedSettings.WifiSSID = customSSID;
            } else if (!selectedSSID.isEmpty()) {
                savedSettings.WifiSSID = selectedSSID;
            }

            if (!wifiPassword.isEmpty()) {
                savedSettings.WifiPW = wifiPassword;
            }
        }

        // Pool Configuration
        if (webServer.hasArg("poolSelect")) {
            int poolIndex = webServer.arg("poolSelect").toInt();
            if (poolIndex >= 0 && poolIndex < POOL_COUNT) {
                if (poolIndex == 0) {  // Custom Pool
                    if (webServer.hasArg("poolUrl") && webServer.hasArg("poolPort")) {
                        savedSettings.PoolAddress = webServer.arg("poolUrl").c_str();
                        savedSettings.PoolPort = webServer.arg("poolPort").toInt();
                    }
                } else {
                    savedSettings.PoolAddress = POOLS[poolIndex].url;
                    savedSettings.PoolPort = POOLS[poolIndex].port;
                }
            }
        }

        // Wallet Configuration
        if (webServer.hasArg("wallet")) {
            strncpy(savedSettings.BtcWallet, webServer.arg("wallet").c_str(), sizeof(savedSettings.BtcWallet) - 1);
        }

        if (webServer.hasArg("password")) {
            strncpy(savedSettings.PoolPassword, webServer.arg("password").c_str(), sizeof(savedSettings.PoolPassword) - 1);
        }

        // Display Configuration
        if (webServer.hasArg("display_enabled")) {
            savedSettings.displayEnabled = webServer.arg("display_enabled").toInt() == 0;
        }

        // Timezone Configuration
        if (webServer.hasArg("timezone")) {
            savedSettings.Timezone = webServer.arg("timezone").toInt();
        }

        // Save Statistics Configuration
        if (webServer.hasArg("save_stats")) {
            savedSettings.saveStats = webServer.arg("save_stats").toInt() == 1;
        }

        // Create a detailed HTML response with saved parameters
        String htmlResponse = "<!DOCTYPE html><html><head><title>Configuration Saved</title>";
        htmlResponse += "<style>body { font-family: Arial, sans-serif; max-width: 600px; margin: 0 auto; padding: 20px; }";
        htmlResponse += "table { width: 100%; border-collapse: collapse; }";
        htmlResponse += "th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }";
        htmlResponse += "h1 { color: #4CAF50; }</style></head><body>";
        htmlResponse += "<h1>Configuration Saved Successfully</h1>";
        htmlResponse += "<table>";
        
        // WiFi Settings
        htmlResponse += "<tr><th colspan='2'>WiFi Configuration</th></tr>";
        htmlResponse += "<tr><td>Network SSID</td><td>" + savedSettings.WifiSSID + "</td></tr>";
        
        // Pool Settings
        htmlResponse += "<tr><th colspan='2'>Pool Configuration</th></tr>";
        htmlResponse += "<tr><td>Pool URL</td><td>" + String(savedSettings.PoolAddress) + "</td></tr>";
        htmlResponse += "<tr><td>Pool Port</td><td>" + String(savedSettings.PoolPort) + "</td></tr>";
        
        // Wallet Settings
        htmlResponse += "<tr><th colspan='2'>Wallet Configuration</th></tr>";
        htmlResponse += "<tr><td>BTC Wallet Address</td><td>" + String(savedSettings.BtcWallet) + "</td></tr>";
        
        // Additional Settings
        htmlResponse += "<tr><th colspan='2'>Additional Settings</th></tr>";
        htmlResponse += "<tr><td>Timezone</td><td>" + String(savedSettings.Timezone) + "</td></tr>";
        htmlResponse += "<tr><td>Save Statistics</td><td>" + String(savedSettings.saveStats ? "Enabled" : "Disabled") + "</td></tr>";
        htmlResponse += "<tr><td>Display Enabled</td><td>" + String(savedSettings.displayEnabled ? "Yes" : "No") + "</td></tr>";
        
        htmlResponse += "</table>";
        htmlResponse += "<p>Device will restart in 3 seconds...</p>";
        htmlResponse += "<script>setTimeout(function(){window.location.href='/';}, 3000);</script>";
        htmlResponse += "</body></html>";

        // Save the updated configuration
        if (nvMem.saveConfig(&savedSettings)) {
            webServer.send(200, "text/html", htmlResponse);
            delay(1000);
            ESP.restart();
        } else {
            webServer.send(500, "text/plain", "Failed to save configuration");
        }
    });

    // API routes for dynamic data
    webServer.on("/api/pools", HTTP_GET, []() {
        String jsonResponse = "[";
        for (size_t i = 0; i < POOL_COUNT; i++) {
            jsonResponse += "{";
            jsonResponse += "\"name\":\"" + String(POOLS[i].name) + "\",";
            jsonResponse += "\"url\":\"" + String(POOLS[i].url) + "\",";
            jsonResponse += "\"port\":" + String(POOLS[i].port) + ",";
            jsonResponse += "\"webUrl\":\"" + String(POOLS[i].webUrl ? POOLS[i].webUrl : "") + "\",";
            jsonResponse += "\"info\":\"" + String(POOLS[i].info) + "\"";
            jsonResponse += "}";
            if (i < POOL_COUNT - 1) jsonResponse += ",";
        }
        jsonResponse += "]";
        webServer.send(200, "application/json", jsonResponse);
    });

    webServer.on("/api/settings", HTTP_GET, []() {
        String jsonResponse = "{";
        jsonResponse += "\"wifiSSID\":\"" + Settings.WifiSSID + "\",";
        jsonResponse += "\"poolUrl\":\"" + String(Settings.PoolAddress) + "\",";
        jsonResponse += "\"poolPort\":" + String(Settings.PoolPort) + ",";
        jsonResponse += "\"btcWallet\":\"" + String(Settings.BtcWallet) + "\",";
        jsonResponse += "\"poolPassword\":\"" + String(Settings.PoolPassword) + "\",";
        jsonResponse += "\"timezone\":" + String(Settings.Timezone) + ",";
        jsonResponse += "\"saveStats\":" + String(Settings.saveStats ? "true" : "false") + ",";
        jsonResponse += "\"displayEnabled\":" + String(Settings.displayEnabled ? "true" : "false");
        jsonResponse += "}";
        webServer.send(200, "application/json", jsonResponse);
    });

    // Add factory reset endpoint
    webServer.on("/factory-reset", HTTP_POST, []() {
        String response = generateJsonResponse(true, "Factory reset initiated");
        webServer.send(200, "application/json", response);
        webServer.client().flush();
        delay(1000); // Give more time for the response to be sent
        reset_configuration();
    });

    // 404 handler
    webServer.onNotFound([]() {
        // Redirect all unhandled routes to root
        webServer.sendHeader("Location", "/");
        webServer.send(302, "text/plain", "");
    });

    // Start the web server
    webServer.begin();
    Serial.println("Web Server Started");
}

void saveNewConfig() {
    Serial.println("Starting saveNewConfig");

    // Save settings to NVS
    nvMemory nvMem;
    if (!nvMem.saveConfig(&Settings)) {
        throw std::runtime_error("Failed to save settings to NVS");
    }

    Serial.println("saveNewConfig completed successfully");
}

void init_WifiManager() {
    wifiManager.init();
}

void wifiManagerProcess() {
    // Process WiFi state
    wifiManager.process();

    // Ensure web server is always running in AP or STA mode
    if (!portalRunning) {
        Serial.println("Starting Web Server");
        setupWebServer();
        portalRunning = true;
        portalStartTime = millis();
    }

    // Enhanced DNS and Captive Portal Handling
    if (WiFi.getMode() == WIFI_AP_STA || WiFi.getMode() == WIFI_AP) {
        static unsigned long lastDNSStartTime = 0;
        static bool dnsServerStarted = false;

        // Retry DNS server start with exponential backoff
        if (!dnsServerStarted || (millis() - lastDNSStartTime > 5000)) {
            Serial.println("Configuring Captive Portal DNS Server");
            
            // Stop any existing DNS server to prevent conflicts
            dnsServer.stop();
            
            // Set up DNS server with specific IP
            IPAddress apIP(192, 168, 4, 1);
            WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
            
            // Start DNS server
            dnsServer.start(DNS_PORT, "*", apIP);
            
            // Update tracking variables
            lastDNSStartTime = millis();
            dnsServerStarted = true;
            
            Serial.println("DNS Server started for Captive Portal");
        }

        // Always process DNS requests
        dnsServer.processNextRequest();
    }

    // Always handle web server client requests
    webServer.handleClient();

    // Connection state monitoring
    static bool wasConnected = false;
    bool isConnected = wifiManager.isConnected();
    
    if (isConnected && !wasConnected) {
        Serial.println("\nWiFi Connected!");
        Serial.print("IP Address: ");
        Serial.println(wifiManager.getLocalIP());
        
        // Explicitly restart web server when WiFi connects
        Serial.println("Restarting Web Server after WiFi Connection");
        setupWebServer();
        
        mMonitor.NerdStatus = NM_Connecting;
        wasConnected = true;
    } else if (!isConnected && wasConnected) {
        Serial.println("\nWiFi Disconnected!");
        mMonitor.NerdStatus = NM_waitingConfig;
        wasConnected = false;
    }

    // Optional: Add a timeout for configuration portal
    if (portalRunning && (millis() - portalStartTime > CONFIG_PORTAL_TIMEOUT)) {
        Serial.println("Configuration Portal Timeout, Restarting...");
        ESP.restart();
    }
}

void reset_configuration() {
    Serial.println("Resetting configuration to factory defaults...");

    // Disconnect WiFi completely
    WiFi.disconnect(true, true);  // Disconnect and clear stored credentials
    WiFi.mode(WIFI_OFF);
    delay(100);

    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Erase all of NVS
    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_ERROR_CHECK(nvs_flash_init());
    
    // Reset all settings to defaults
    Settings = {}; // Clear entire structure first
    Settings.WifiSSID = "";  // Explicitly clear WiFi settings
    Settings.WifiPW = "";
    Settings.PoolAddress = DEFAULT_POOLURL;
    strcpy(Settings.BtcWallet, DEFAULT_WALLETID);
    strcpy(Settings.PoolPassword, DEFAULT_POOLPASS);
    Settings.PoolPort = DEFAULT_POOLPORT;
    Settings.Timezone = DEFAULT_TIMEZONE;
    Settings.saveStats = DEFAULT_SAVESTATS;
    Settings.invertColors = DEFAULT_INVERTCOLORS;
    Settings.displayEnabled = DEFAULT_DISPLAY_ENABLED;
    Settings.ledEnabled = DEFAULT_LED_ENABLED;
    
    // Save the cleared settings
    nvMemory nvMem;
    nvMem.saveConfig(&Settings);
    
    Serial.println("Factory reset complete. Device will restart in AP mode...");
    delay(1000);
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
