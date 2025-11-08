#include "network.h"
#include "hardware.h"
#include "users.h"

// ================== Network Configuration ==================
const char* WIFI_SSID = "Hanu";
const char* WIFI_PASS = "12345678";
const char* SERVER_HOST = "192.168.137.1"; // Admin panel server IP
const int SERVER_PORT = 3000;

// ================== Server Objects ==================
WebServer server(80);
HTTPClient httpClient;

// ================== Network State ==================
bool wifiConnected = false;
bool serverConnected = false;
String deviceIP = "";
long lastHeartbeat = 0;

// ================== Network Initialization ==================
bool initializeNetwork() {
    Serial.println("Initializing network...");
    connectWiFi();
    if (wifiConnected) {
        setupWebServer();
        deviceIP = getDeviceIP();
        Serial.println("Network initialization complete");
        return true;
    } else {
        Serial.println("Network initialization failed - WiFi not connected");
        return false;
    }
}

void connectWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    
    Serial.printf("Connecting to WiFi '%s'", WIFI_SSID);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        deviceIP = WiFi.localIP().toString();
        Serial.printf("\nWiFi connected! IP: %s\n", deviceIP.c_str());
        Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
    } else {
        wifiConnected = false;
        Serial.println("\nWiFi connection failed!");
    }
}

bool checkWiFiConnection() {
    if (WiFi.status() != WL_CONNECTED) {
        wifiConnected = false;
        Serial.println("WiFi disconnected, attempting reconnection...");
        connectWiFi();
    }
    return wifiConnected;
}

void setupWebServer() {
    // API endpoints for admin panel communication
    server.on("/", HTTP_GET, []() {
        server.send(200, "text/html", "<h1>ESP32 Gate System v" + String(FW_VERSION) + "</h1><p>Device IP: " + deviceIP + "</p>");
    });
    
    server.on("/api/info", HTTP_GET, handleInfo);
    server.on("/api/state", HTTP_GET, handleState);
    server.on("/api/open", HTTP_POST, handleOpen);
    server.on("/api/led", HTTP_POST, handleLED);
    server.on("/api/input/mode", HTTP_POST, handleInputMode);
    server.on("/api/time/sync", HTTP_POST, handleTimeSync);
    server.on("/api/database/sync", HTTP_POST, handleDatabaseSync);
    server.on("/api/input/last", HTTP_GET, handleLastInput);
    server.on("/api/selftest", HTTP_GET, handleSelfTest);
    
    server.begin();
    Serial.println("Web server started on port 80");
}

void handleWebRequests() {
    server.handleClient();
}

// ================== RPC Communication Functions ==================
RPCResponse sendRPCRequest(const String& endpoint, const String& method, const String& payload) {
    RPCResponse response;
    
    if (!checkWiFiConnection()) {
        response.error = "WiFi not connected";
        return response;
    }
    
    String url = createURL(endpoint);
    httpClient.begin(url);
    httpClient.addHeader("Content-Type", "application/json");
    httpClient.setTimeout(5000);
    
    int httpCode;
    if (method == "POST") {
        httpCode = httpClient.POST(payload);
    } else if (method == "PUT") {
        httpCode = httpClient.PUT(payload);
    } else {
        httpCode = httpClient.GET();
    }
    
    if (httpCode > 0) {
        String responseBody = httpClient.getString();
        
        if (httpCode == 200) {
            DeserializationError error = deserializeJson(response.data, responseBody);
            if (error) {
                response.error = "JSON parse error: " + String(error.c_str());
            } else {
                response.success = true;
            }
        } else {
            response.error = "HTTP " + String(httpCode) + ": " + responseBody;
        }
    } else {
        response.error = "Connection error: " + httpClient.errorToString(httpCode);
    }
    
    httpClient.end();
    return response;
}

RPCResponse getUsersFromServer() {
    return sendRPCRequest("/api/database/users", "GET");
}

RPCResponse sendUserToServer(const String& uid, const String& name, long credit) {
    DynamicJsonDocument payload(512);
    payload["uid"] = uid;
    payload["name"] = name;
    payload["credit"] = credit;
    
    String payloadStr;
    serializeJson(payload, payloadStr);
    
    return sendRPCRequest("/api/database/users/add", "POST", payloadStr);
}

RPCResponse updateUserOnServer(const String& uid, const String& name, long credit, bool in) {
    DynamicJsonDocument payload(512);
    payload["uid"] = uid;
    payload["name"] = name;
    payload["credit"] = credit;
    payload["in"] = in;
    
    String payloadStr;
    serializeJson(payload, payloadStr);
    
    return sendRPCRequest("/api/database/users/update", "POST", payloadStr);
}

RPCResponse notifyNewUID(const String& uid, bool isNew) {
    DynamicJsonDocument payload(512);
    payload["uid"] = uid;
    payload["isNew"] = isNew;
    payload["timestamp"] = millis();
    payload["device_ip"] = deviceIP;
    
    String payloadStr;
    serializeJson(payload, payloadStr);
    
    return sendRPCRequest("/api/input/new-uid", "POST", payloadStr);
}

RPCResponse syncTimeWithServer() {
    return sendRPCRequest("/api/time/server", "GET");
}

RPCResponse notifyServerEvent(const String& event, const String& details) {
    DynamicJsonDocument payload(512);
    payload["event"] = event;
    payload["details"] = details;
    payload["timestamp"] = millis();
    payload["device_ip"] = deviceIP;
    
    String payloadStr;
    serializeJson(payload, payloadStr);
    
    return sendRPCRequest("/api/events/notify", "POST", payloadStr);
}

// ================== Server Response Handlers ==================
void handleInfo() {
    DynamicJsonDocument doc(1024);
    doc["fwVersion"] = FW_VERSION;
    doc["version"] = FW_VERSION;
    doc["ip"] = deviceIP;
    doc["ssid"] = WIFI_SSID;
    doc["rssi"] = getWiFiRSSI();
    doc["uptime"] = millis() / 1000;
    doc["uptime_s"] = millis() / 1000;
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["heap"] = ESP.getFreeHeap();
    doc["users"] = getTotalUserCount();
    doc["static"] = getStaticUserCount();
    doc["dynamic"] = getDynamicUserCount();
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleState() {
    DynamicJsonDocument doc(2048);
    doc["inputMode"] = isInputModeActive();
    doc["gateOpen"] = gateIsOpen;
    
    JsonArray users = doc.createNestedArray("users");
    populateUsersJson(users);
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleOpen() {
    gateOpen();
    server.send(200, "application/json", "{\"success\":true}");
}

void handleLED() {
    String color = server.hasArg("c") ? server.arg("c") : "OFF";
    color.toUpperCase();
    
    if (color == "RED") {
        setLED(true, false, false);
    } else if (color == "GREEN") {
        setLED(false, true, false);
    } else if (color == "BLUE") {
        setLED(false, false, true);
    } else {
        ledOff();
    }
    
    server.send(200, "application/json", "{\"success\":true}");
}

void handleInputMode() {
    if (server.hasArg("mode")) {
        String mode = server.arg("mode");
        mode.toLowerCase();
        
        if (mode == "on") {
            setInputModeActive(true);
        } else if (mode == "off") {
            setInputModeActive(false);
        }
    }
    
    DynamicJsonDocument doc(256);
    doc["active"] = isInputModeActive();
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleTimeSync() {
    if (server.hasArg("timestamp")) {
        long timestamp = server.arg("timestamp").toInt();
        
        if (timestamp > 0) {
            rtc.adjust(DateTime(timestamp));
            server.send(200, "application/json", "{\"success\":true}");
            return;
        }
    }
    
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid timestamp\"}");
}

void handleDatabaseSync() {
    String payload = server.arg("plain");
    DynamicJsonDocument doc(4096);
    
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
    }
    
    if (syncUsersFromJson(doc)) {
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(500, "application/json", "{\"success\":false,\"error\":\"Sync failed\"}");
    }
}

void handleLastInput() {
    DynamicJsonDocument doc(512);
    
    LastScanResult lastScan = getLastScan();
    doc["hasInput"] = !lastScan.uid.isEmpty();
    doc["uid"] = lastScan.uid;
    doc["timestamp"] = lastScan.timestamp;
    doc["isNew"] = lastScan.isNew;
    doc["inputMode"] = isInputModeActive();
    
    if (server.hasArg("clear") && server.arg("clear") == "true") {
        clearLastScan();
    }
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleSelfTest() {
    DynamicJsonDocument doc(512);
    doc["rc522"] = testRFID();
    doc["oled"] = testOLED();
    doc["led"] = testLED();
    doc["servo"] = testServo();
    doc["rtc"] = testRTC();
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

// ================== Utility Functions ==================
String getDeviceIP() {
    return WiFi.localIP().toString();
}

long getWiFiRSSI() {
    return WiFi.RSSI();
}

void syncUsersWithServer() {
    Serial.println("Syncing users with server...");
    
    RPCResponse response = getUsersFromServer();
    if (!response.success) {
        Serial.println("Failed to sync users: " + response.error);
        return;
    }
    
    if (response.data.containsKey("users")) {
        JsonArray users = response.data["users"];
        Serial.println("Received " + String(users.size()) + " users from server");
        
        // This would typically update the local user database
        // For now, just log the received data
        for (JsonVariant user : users) {
            String uid = user["uid"].as<String>();
            String name = user["name"].as<String>();
            long credit = user["credit"].as<long>();
            Serial.println("Server user: " + name + " (" + uid + ") - Credit: " + String(credit));
        }
    }
    
    Serial.println("User sync completed");
}

void sendHeartbeat() {
    if (millis() - lastHeartbeat > 30000) { // Every 30 seconds
        RPCResponse response = notifyServerEvent("HEARTBEAT", "Device alive");
        serverConnected = response.success;
        lastHeartbeat = millis();
    }
}

bool isServerReachable() {
    return serverConnected;
}

String createURL(const String& endpoint) {
    return "http://" + String(SERVER_HOST) + ":" + String(SERVER_PORT) + endpoint;
}