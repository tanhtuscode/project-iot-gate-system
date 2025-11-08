#ifndef NETWORK_H
#define NETWORK_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <vector>

// ================== Network Configuration ==================
extern const char* WIFI_SSID;
extern const char* WIFI_PASS;
extern const char* SERVER_HOST;
extern const int SERVER_PORT;

// ================== Server Objects ==================
extern WebServer server;
extern HTTPClient httpClient;

// ================== Network State ==================
extern bool wifiConnected;
extern bool serverConnected;
extern String deviceIP;
extern long lastHeartbeat;

// ================== RPC Response Structure ==================
struct RPCResponse {
    bool success;
    String error;
    DynamicJsonDocument data;
    
    RPCResponse() : success(false), data(1024) {}
};

// ================== Network Functions ==================
bool initializeNetwork();
void connectWiFi();
bool checkWiFiConnection();
void setupWebServer();
void startWebServer();
void handleWebRequests();
void handleWebServerRequests();
void syncUsersWithServer();

// ================== RPC Communication Functions ==================
RPCResponse sendRPCRequest(const String& endpoint, const String& method = "GET", const String& payload = "");
RPCResponse getUsersFromServer();
RPCResponse sendUserToServer(const String& uid, const String& name, long credit);
RPCResponse updateUserOnServer(const String& uid, const String& name, long credit, bool in);
RPCResponse notifyNewUID(const String& uid, bool isNew);
RPCResponse syncTimeWithServer();
RPCResponse notifyServerEvent(const String& event, const String& details);
void sendAdminAlert(const String& event, const String& uid, const String& userName, long credit, const String& reason);

// ================== Server Response Handlers ==================
void handleInfo();
void handleState();
void handleOpen();
void handleLED();
void handleInputMode();
void handleTimeSync();
void handleDatabaseSync();
void handleLastInput();
void handleSelfTest();

// ================== Utility Functions ==================
String getDeviceIP();
long getWiFiRSSI();
void sendHeartbeat();
bool isServerReachable();
String createURL(const String& endpoint);

#endif // NETWORK_H