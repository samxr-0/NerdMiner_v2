#include "wifiManager.h"
#include "wManager.h"  // For mMonitor and TSettings
#include <WiFi.h>
#include "log.h"

// Declare external global variables
extern nvMemory nvMem;
extern TSettings Settings;
extern monitor_data mMonitor;

WiFiManagerClass wifiManager;

unsigned long configPortalStartTime = 0;

void WiFiManagerClass::resetConfigPortalTimeout() {
    configPortalStartTime = millis();
}

void WiFiManagerClass::init() {
    LOG(LOG_INFO, "WiFi Manager Initialization\n");
    
    // Load WiFi settings from non-volatile storage
    nvMem.loadConfig(&Settings);
    
    // Comprehensive WiFi reset
    WiFi.disconnect(true, true);  // Disconnect and clear credentials
    
    // Explicitly set WiFi mode and wait
    WiFi.mode(WIFI_STA);
    delay(200);
    
    // Print out current WiFi credentials for debugging
    LOG(LOG_DEBUG, "Loaded SSID: %s\n", Settings.WifiSSID.c_str());
    LOG(LOG_DEBUG, "Loaded Password Length: %d\n", Settings.WifiPW.length());
    
    // Check if WiFi credentials are valid
    bool hasValidSSID = Settings.WifiSSID.length() > 0 && 
                        Settings.WifiSSID != String(DEFAULT_SSID);
    bool hasValidPassword = Settings.WifiPW.length() > 0 && 
                            Settings.WifiPW != String(DEFAULT_WIFIPW);
    
    if (!hasValidSSID || !hasValidPassword) {
        LOG(LOG_WARN, "No valid WiFi credentials found, starting Access Point\n");
        startAccessPoint();
        return;
    }
    
    // Attempt to connect to saved network
    connectToSavedNetwork();
}

void WiFiManagerClass::process() {
    // Check configuration portal timeout
    if (mMonitor.NerdStatus == NM_waitingConfig) {
        if (configPortalStartTime == 0) {
            resetConfigPortalTimeout();
        }
        
        // Check if config portal has been running too long
        // But only if we're not actively using WiFi
        if (WiFi.status() != WL_CONNECTED && 
            (millis() - configPortalStartTime > CONFIG_PORTAL_TIMEOUT)) {
            
            LOG(LOG_WARN, "Configuration Portal Timeout Exceeded\n");
            
            // Attempt to recover or provide fallback
            if (Settings.WifiSSID.length() > 0 && Settings.WifiPW.length() > 0) {
                LOG(LOG_INFO, "Attempting to connect with saved credentials\n");
                
                // Soft reset WiFi
                WiFi.disconnect(true, true);
                delay(200);
                WiFi.mode(WIFI_STA);
                delay(200);
                
                // Attempt connection
                WiFi.begin(Settings.WifiSSID.c_str(), Settings.WifiPW.c_str());
                
                // Update connection state
                lastConnectionAttempt = millis();
                currentState = NM_Connecting;
                mMonitor.NerdStatus = NM_Connecting;
                
                // Reset config portal timeout
                configPortalStartTime = 0;
                
                return;
            } else {
                // If no saved credentials, extend configuration time
                LOG(LOG_WARN, "No saved credentials, extending configuration portal time\n");
                
                // Extend timeout and maintain AP mode
                configPortalStartTime = millis();
                WiFi.mode(WIFI_AP_STA);
                IPAddress apIP(192, 168, 4, 1);
                setupAccessPoint(apIP);
                
                return;
            }
        }
        
        // Existing AP setup logic
        wifi_mode_t currentMode = WiFi.getMode();
        LOG(LOG_DEBUG, "Current WiFi Mode (Before): %d\n", currentMode);
        
        WiFi.mode(WIFI_AP_STA);
        delay(500);  // Longer delay for mode change
        
        currentMode = WiFi.getMode();
        LOG(LOG_DEBUG, "Current WiFi Mode (After): %d\n", currentMode);
        
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
                    LOG(LOG_INFO, "WiFi Connected!\n");
                    LOG(LOG_INFO, "IP Address: %s\n", WiFi.localIP().toString().c_str());
                    
                    // Stop soft AP and switch to STA mode
                    WiFi.softAPdisconnect(true);  // Explicitly stop soft AP
                    WiFi.mode(WIFI_STA);
                    delay(200);  // Small delay to ensure mode change
                    
                    // Reset config portal timeout
                    configPortalStartTime = 0;
                    
                    // Verify network configuration
                    IPAddress localIP = WiFi.localIP();
                    IPAddress subnetMask = WiFi.subnetMask();
                    IPAddress gatewayIP = WiFi.gatewayIP();
                    
                    LOG(LOG_DEBUG, "Network Details:\n");
                    LOG(LOG_DEBUG, "Local IP: %s\n", localIP.toString().c_str());
                    LOG(LOG_DEBUG, "Subnet Mask: %s\n", subnetMask.toString().c_str());
                    LOG(LOG_DEBUG, "Gateway IP: %s\n", gatewayIP.toString().c_str());
                    
                    mMonitor.NerdStatus = NM_Connected;
                    currentState = NM_Connected;
                } 
                else if (millis() - lastConnectionAttempt > CONNECTION_TIMEOUT) {
                    LOG(LOG_WARN, "Connection timeout, starting Access Point\n");
                    
                    // Log detailed connection failure reasons
                    switch (wifiStatus) {
                        case WL_NO_SSID_AVAIL:
                            LOG(LOG_ERROR, "No SSID Available\n");
                            break;
                        case WL_CONNECT_FAILED:
                            LOG(LOG_ERROR, "Connection Failed\n");
                            break;
                        case WL_IDLE_STATUS:
                            LOG(LOG_WARN, "Idle Status\n");
                            break;
                        default:
                            LOG(LOG_ERROR, "Connection Status: %d\n", wifiStatus);
                            break;
                    }
                    
                    startAccessPoint();
                }
            }
            break;
        
        case NM_Connected:
            {
                // Enhanced connection monitoring
                static unsigned long lastConnectionCheck = 0;
                const unsigned long CONNECTION_CHECK_INTERVAL = 10000;  // 10 seconds
                
                if (WiFi.status() != WL_CONNECTED) {
                    LOG(LOG_WARN, "WiFi Disconnected, attempting to reconnect\n");
                    
                    // Comprehensive WiFi reset
                    WiFi.disconnect(true, true);
                    delay(200);
                    WiFi.mode(WIFI_OFF);
                    delay(200);
                    WiFi.mode(WIFI_STA);
                    WiFi.setAutoConnect(true);
                    WiFi.setAutoReconnect(true);
                    
                    // Attempt reconnection
                    WiFi.begin(Settings.WifiSSID.c_str(), Settings.WifiPW.c_str());
                    
                    // Update states
                    currentState = NM_Connecting;
                    mMonitor.NerdStatus = NM_Connecting;
                    lastConnectionAttempt = millis();
                }
                else if (millis() - lastConnectionCheck > CONNECTION_CHECK_INTERVAL) {
                    // Periodic connection health check
                    lastConnectionCheck = millis();
                    LOG(LOG_DEBUG, "Periodic WiFi Connection Check: %d\n", WiFi.status());
                }
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
        LOG(LOG_WARN, "Not in configuration mode, skipping AP setup\n");
        return;
    }

    // Detailed Access Point configuration
    IPAddress subnet(255, 255, 255, 0);
    IPAddress gateway(192, 168, 4, 1);
    
    // Verbose soft AP configuration
    LOG(LOG_INFO, "Configuring Soft AP:\n");
    
    // Ensure WiFi is in AP_STA mode for configuration
    WiFi.mode(WIFI_AP_STA);
    delay(200);
    
    // Configure soft AP with gateway and subnet
    bool apConfigResult = WiFi.softAPConfig(apIP, gateway, subnet);
    LOG(LOG_INFO, "AP Config Result: %s\n", apConfigResult ? "Success" : "Failed");
    
    // Start Soft AP with detailed parameters
    bool apStartResult = WiFi.softAP(
        "NerdMiner_Config",  // SSID
        "nerdminer123",      // Password
        1,                   // Channel
        false,               // Hidden
        4                    // Max connections
    );
    LOG(LOG_INFO, "Soft AP Start Result: %s\n", apStartResult ? "Success" : "Failed");
    
    // Verify AP details
    LOG(LOG_INFO, "AP SSID: %s\n", WiFi.softAPSSID().c_str());
    LOG(LOG_INFO, "AP IP: %s\n", WiFi.softAPIP().toString().c_str());
    
    // Additional diagnostic information
    LOG(LOG_DEBUG, "Station IP: %s\n", WiFi.localIP().toString().c_str());
    LOG(LOG_DEBUG, "Soft AP IP: %s\n", WiFi.softAPIP().toString().c_str());
}

void WiFiManagerClass::startAccessPoint() {
    LOG(LOG_INFO, "Starting Access Point Mode (Detailed)\n");
    
    // Reset config portal timeout
    resetConfigPortalTimeout();
    
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
    LOG(LOG_INFO, "Attempting to connect to saved WiFi network\n");
    
    // Comprehensive WiFi reset and configuration
    WiFi.disconnect(true, true);  // Disconnect and clear credentials
    WiFi.mode(WIFI_OFF);
    delay(200);
    
    // Set WiFi mode to STA with explicit configuration
    WiFi.mode(WIFI_STA);
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);
    
    // Print out connection details for debugging
    LOG(LOG_DEBUG, "Connecting to SSID: %s\n", Settings.WifiSSID.c_str());
    LOG(LOG_DEBUG, "Using Password: %s\n", Settings.WifiPW.length() > 0 ? "***" : "NONE");
    
    // Validate WiFi credentials before attempting connection
    if (Settings.WifiSSID.length() == 0 || 
        Settings.WifiSSID == String(DEFAULT_SSID) ||
        Settings.WifiPW.length() == 0 || 
        Settings.WifiPW == String(DEFAULT_WIFIPW)) {
        
        LOG(LOG_WARN, "Invalid WiFi credentials, starting Access Point\n");
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
            LOG(LOG_WARN, "WiFi Connection Timeout!\n");
            
            // Detailed WiFi connection status logging
            switch (WiFi.status()) {
                case WL_NO_SSID_AVAIL:
                    LOG(LOG_ERROR, "No SSID Available\n");
                    break;
                case WL_CONNECT_FAILED:
                    LOG(LOG_ERROR, "Connection Failed\n");
                    break;
                case WL_IDLE_STATUS:
                    LOG(LOG_WARN, "Idle Status\n");
                    break;
                default:
                    LOG(LOG_ERROR, "Connection Status: %d\n", WiFi.status());
                    break;
            }
            
            // Start Access Point if connection fails
            startAccessPoint();
            return;
        }
        
        // Periodic status update
        if (millis() % 1000 == 0) {
            LOG(LOG_DEBUG, ".\n");
        }
        
        delay(100);
    }
    
    // Connection successful
    LOG(LOG_INFO, "WiFi Connected Successfully!\n");
    LOG(LOG_INFO, "IP Address: %s\n", WiFi.localIP().toString().c_str());
    
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
    LOG(LOG_INFO, "Starting WiFi Network Scan...\n");
    
    // Configure scan parameters
    WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
    WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
    
    // Perform WiFi scan
    int networksFound = WiFi.scanNetworks(includeHidden);
    
    if (networksFound > 0 && logResults) {
        LOG(LOG_INFO, "Networks Found: %d\n", networksFound);
        
        for (int i = 0; i < networksFound; i++) {
            LOG(LOG_DEBUG, "Network %d: SSID: %s, RSSI: %d, Channel: %d, Encryption: %d\n", 
                i, 
                WiFi.SSID(i).c_str(), 
                WiFi.RSSI(i), 
                WiFi.channel(i), 
                WiFi.encryptionType(i)
            );
        }
    } else {
        LOG(LOG_WARN, "No networks found or scan failed.\n");
    }
    
    return networksFound;
}
