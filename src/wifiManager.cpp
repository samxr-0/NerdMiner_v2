#include "wifiManager.h"
#include "wManager.h"  // For mMonitor and TSettings
#include <WiFi.h>

// Declare external global variables
extern nvMemory nvMem;
extern TSettings Settings;
extern monitor_data mMonitor;

WiFiManagerClass wifiManager;

void WiFiManagerClass::init() {
    Serial.println("WiFi Manager Initialization");
    
    // Load WiFi settings from non-volatile storage
    nvMem.loadConfig(&Settings);
    
    // Comprehensive WiFi reset
    WiFi.disconnect(true, true);  // Disconnect and clear credentials
    
    // Explicitly set WiFi mode and wait
    WiFi.mode(WIFI_STA);
    delay(200);
    
    // Print out current WiFi credentials for debugging
    Serial.print("Loaded SSID: ");
    Serial.println(Settings.WifiSSID);
    Serial.print("Loaded Password Length: ");
    Serial.println(Settings.WifiPW.length());
    
    // Check if WiFi credentials are valid
    bool hasValidSSID = Settings.WifiSSID.length() > 0 && 
                        Settings.WifiSSID != String(DEFAULT_SSID);
    bool hasValidPassword = Settings.WifiPW.length() > 0 && 
                            Settings.WifiPW != String(DEFAULT_WIFIPW);
    
    if (!hasValidSSID || !hasValidPassword) {
        Serial.println("No valid WiFi credentials found, starting Access Point");
        startAccessPoint();
        return;
    }
    
    // Attempt to connect to saved network
    connectToSavedNetwork();
}

void WiFiManagerClass::process() {
    // Detailed logging for configuration state
    if (mMonitor.NerdStatus == NM_waitingConfig) {
        // Verbose WiFi mode and AP configuration logging
        wifi_mode_t currentMode = WiFi.getMode();
        Serial.printf("Current WiFi Mode (Before): %d\n", currentMode);
        
        // Explicitly set WiFi mode to AP_STA only for configuration
        WiFi.mode(WIFI_AP_STA);
        delay(500);  // Longer delay for mode change
        
        currentMode = WiFi.getMode();
        Serial.printf("Current WiFi Mode (After): %d\n", currentMode);
        
        // Setup Access Point
        IPAddress apIP(192, 168, 4, 1);
        setupAccessPoint(apIP);
        
        return;
    }
    
    // Existing connection monitoring logic
    switch (currentState) {
        case NM_Connecting:
            {
                // Detailed connection status checking
                wl_status_t wifiStatus = WiFi.status();
                
                if (wifiStatus == WL_CONNECTED) {
                    Serial.println("WiFi Connected!");
                    Serial.print("IP Address: ");
                    Serial.println(WiFi.localIP());
                    
                    // Stop soft AP and switch to STA mode
                    WiFi.softAPdisconnect(true);  // Explicitly stop soft AP
                    WiFi.mode(WIFI_STA);
                    delay(200);  // Small delay to ensure mode change
                    
                    // Verify network configuration
                    IPAddress localIP = WiFi.localIP();
                    IPAddress subnetMask = WiFi.subnetMask();
                    IPAddress gatewayIP = WiFi.gatewayIP();
                    
                    Serial.println("Network Details:");
                    Serial.print("Local IP: ");
                    Serial.println(localIP);
                    Serial.print("Subnet Mask: ");
                    Serial.println(subnetMask);
                    Serial.print("Gateway IP: ");
                    Serial.println(gatewayIP);
                    
                    mMonitor.NerdStatus = NM_Connected;
                    currentState = NM_Connected;
                } 
                else if (millis() - lastConnectionAttempt > CONNECTION_TIMEOUT) {
                    Serial.println("Connection timeout, starting Access Point");
                    
                    // Log detailed connection failure reasons
                    switch (wifiStatus) {
                        case WL_NO_SSID_AVAIL:
                            Serial.println("No SSID Available");
                            break;
                        case WL_CONNECT_FAILED:
                            Serial.println("Connection Failed");
                            break;
                        case WL_IDLE_STATUS:
                            Serial.println("Idle Status");
                            break;
                        default:
                            Serial.printf("Connection Status: %d\n", wifiStatus);
                            break;
                    }
                    
                    startAccessPoint();
                }
            }
            break;
        
        case NM_Connected:
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("WiFi Disconnected, attempting to reconnect");
                connectToSavedNetwork();
            }
            break;
        
        default:
            currentState = NM_waitingConfig;
            mMonitor.NerdStatus = NM_waitingConfig;
            break;
    }
}

void WiFiManagerClass::setupAccessPoint(const IPAddress& apIP) {
    // Only configure soft AP if in configuration mode
    if (mMonitor.NerdStatus != NM_waitingConfig) {
        Serial.println("Not in configuration mode, skipping AP setup");
        return;
    }

    // Detailed Access Point configuration
    IPAddress subnet(255, 255, 255, 0);
    IPAddress gateway(192, 168, 4, 1);
    
    // Verbose soft AP configuration
    Serial.println("Configuring Soft AP:");
    
    // Ensure WiFi is in AP_STA mode for configuration
    WiFi.mode(WIFI_AP_STA);
    delay(200);
    
    // Configure soft AP with gateway and subnet
    bool apConfigResult = WiFi.softAPConfig(apIP, gateway, subnet);
    Serial.printf("AP Config Result: %s\n", apConfigResult ? "Success" : "Failed");
    
    // Start Soft AP with detailed parameters
    bool apStartResult = WiFi.softAP(
        "NerdMiner_Config",  // SSID
        "nerdminer123",      // Password
        1,                   // Channel
        false,               // Hidden
        4                    // Max connections
    );
    Serial.printf("Soft AP Start Result: %s\n", apStartResult ? "Success" : "Failed");
    
    // Verify AP details
    Serial.print("AP SSID: ");
    Serial.println(WiFi.softAPSSID());
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
    
    // Additional diagnostic information
    Serial.printf("Station IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Soft AP IP: %s\n", WiFi.softAPIP().toString().c_str());
}

void WiFiManagerClass::startAccessPoint() {
    Serial.println("Starting Access Point Mode (Detailed)");
    
    // Comprehensive WiFi reset
    WiFi.disconnect(true, true);  // Disconnect and clear credentials
    
    // Explicitly set WiFi mode
    WiFi.mode(WIFI_OFF);
    delay(200);
    WiFi.mode(WIFI_AP_STA);  // Only use AP_STA for configuration
    delay(500);  // Longer delay for mode change
    
    // Setup Access Point
    IPAddress apIP(192, 168, 4, 1);
    setupAccessPoint(apIP);
    
    // Update system status
    currentState = NM_waitingConfig;
    mMonitor.NerdStatus = NM_waitingConfig;
}

void WiFiManagerClass::connectToSavedNetwork() {
    Serial.println("Attempting to connect to saved WiFi network");
    
    // Comprehensive WiFi reset and configuration
    WiFi.disconnect(true, true);  // Disconnect and clear credentials
    WiFi.mode(WIFI_OFF);
    delay(200);
    
    // Set WiFi mode to STA with explicit configuration
    WiFi.mode(WIFI_STA);
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);
    
    // Print out connection details for debugging
    Serial.print("Connecting to SSID: ");
    Serial.println(Settings.WifiSSID);
    Serial.print("Using Password: ");
    Serial.println(Settings.WifiPW.length() > 0 ? "***" : "NONE");
    
    // Validate WiFi credentials before attempting connection
    if (Settings.WifiSSID.length() == 0 || 
        Settings.WifiSSID == String(DEFAULT_SSID) ||
        Settings.WifiPW.length() == 0 || 
        Settings.WifiPW == String(DEFAULT_WIFIPW)) {
        
        Serial.println("Invalid WiFi credentials, starting Access Point");
        startAccessPoint();
        return;
    }
    
    // Begin connection with detailed configuration
    WiFi.begin(Settings.WifiSSID.c_str(), Settings.WifiPW.c_str());
    
    // Update connection state
    currentState = NM_Connecting;
    mMonitor.NerdStatus = NM_Connecting;
    lastConnectionAttempt = millis();
    
    // Extended connection timeout and logging
    unsigned long connectionStartTime = millis();
    const unsigned long CONNECTION_TIMEOUT = 15000;  // 15 seconds
    
    while (WiFi.status() != WL_CONNECTED) {
        // Print connection status periodically
        if (millis() - connectionStartTime > CONNECTION_TIMEOUT) {
            Serial.println("WiFi Connection Timeout!");
            
            // Detailed WiFi connection status logging
            switch (WiFi.status()) {
                case WL_NO_SSID_AVAIL:
                    Serial.println("No SSID Available");
                    break;
                case WL_CONNECT_FAILED:
                    Serial.println("Connection Failed");
                    break;
                case WL_IDLE_STATUS:
                    Serial.println("Idle Status");
                    break;
                default:
                    Serial.printf("Connection Status: %d\n", WiFi.status());
                    break;
            }
            
            // Start Access Point if connection fails
            startAccessPoint();
            return;
        }
        
        // Periodic status update
        if (millis() % 1000 == 0) {
            Serial.print(".");
        }
        
        delay(100);
    }
    
    // Connection successful
    Serial.println("\nWiFi Connected Successfully!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    
    // Update connection state
    currentState = NM_Connected;
    mMonitor.NerdStatus = NM_Connected;
}

void WiFiManagerClass::disconnect() {
    WiFi.disconnect(true);
    currentState = NM_waitingConfig;
    mMonitor.NerdStatus = NM_waitingConfig;
}

bool WiFiManagerClass::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

String WiFiManagerClass::getLocalIP() const {
    return WiFi.localIP().toString();
}

IPAddress WiFiManagerClass::getAccessPointIP() const {
    return WiFi.softAPIP();
}

String WiFiManagerClass::getAccessPointSSID() const {
    return WiFi.softAPSSID();
}

int WiFiManagerClass::scanNetworks(bool logResults, bool includeHidden) {
    Serial.println("Starting WiFi Network Scan...");
    
    // Configure scan parameters
    WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
    WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
    
    // Perform WiFi scan
    int networksFound = WiFi.scanNetworks(includeHidden);
    
    if (networksFound > 0 && logResults) {
        Serial.printf("Networks Found: %d\n", networksFound);
        
        for (int i = 0; i < networksFound; i++) {
            Serial.printf("Network %d: SSID: %s, RSSI: %d, Channel: %d, Encryption: %d\n", 
                i, 
                WiFi.SSID(i).c_str(), 
                WiFi.RSSI(i), 
                WiFi.channel(i), 
                WiFi.encryptionType(i)
            );
        }
    } else {
        Serial.println("No networks found or scan failed.");
    }
    
    return networksFound;
}
