#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include "settings.h"
#include "monitor.h"
#include "drivers/storage/nvMemory.h"

// Use the correct DNS_PORT from wManager.h
#define DNS_PORT 53

class WiFiManagerClass {
public:
    // Initialization and core management
    void init();
    void process();
    
    // Network operations
    int scanNetworks(bool logResults = true, bool includeHidden = false);
    void startAccessPoint();
    void connectToSavedNetwork();
    void disconnect();
    
    // State and connection queries
    NMState getState() const { return currentState; }
    bool isConnected() const;
    
    // Network information
    String getLocalIP() const;
    IPAddress getAccessPointIP() const;
    String getAccessPointSSID() const;
    
private:
    NMState currentState = NM_waitingConfig;
    unsigned long lastConnectionAttempt = 0;
    const unsigned long CONNECTION_TIMEOUT = 10000; // 10 seconds
    
    // Internal helper methods
    void setupAccessPoint(const IPAddress& apIP);
};

extern WiFiManagerClass wifiManager;

#endif // WIFI_MANAGER_H
