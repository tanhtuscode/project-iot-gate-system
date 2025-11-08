#include "hardware.h"
#include "network.h" 
#include "display.h"
#include "users.h"

void setup() {
    Serial.begin(9600);
    delay(2000); // Give time for serial monitor to connect
    Serial.println("RFID Gate System Starting...");
    
    // Skip diagnostic mode for now to prevent hanging
    Serial.println("Skipping diagnostic mode - starting normal initialization...");
    
    // Initialize all modules
    if (!initializeHardware()) {
        Serial.println("Hardware initialization failed!");
        while(1) delay(1000); // Stop execution on hardware failure
    }
    
    if (!initializeUsers()) {
        Serial.println("Users initialization failed!");
        showErrorScreen("Users Init Failed");
        while(1) delay(1000);
    }
    
    if (!initializeNetwork()) {
        Serial.println("Network initialization failed!");
        showErrorScreen("Network Init Failed");
        delay(3000); // Show error for 3 seconds
        // Don't stop on network failure, continue without network
    } else {
        // Show IP address on display
        String ip = getDeviceIP();
        Serial.println("ESP32 IP Address: " + ip);
        showSystemInfoScreen(); // This will show the IP
        delay(3000); // Show IP for 3 seconds
    }
    
    Serial.println("All systems initialized successfully!");
    showIdleScreen();
}

void loop() {
    // Handle web server requests
    handleWebRequests();
    
    // Handle gate control (auto-close)
    gateMaybeClose();
    
    // Update display (handle timeouts and return to idle)
    updateDisplay();
    
    // Check for RFID card
    String cardUID = readRFIDCard();
    if (cardUID.length() > 0) {
        processCardScan(cardUID);
    }
    
    // WiFi reconnection check (every 30 seconds)
    static unsigned long lastWiFiCheck = 0;
    if (millis() - lastWiFiCheck > 30000) { // 30 seconds
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi disconnected - attempting reconnection...");
            connectWiFi();
        }
        lastWiFiCheck = millis();
    }
    
    // Periodic user sync (every 5 minutes) - only if WiFi connected
    static unsigned long lastSync = 0;
    if (millis() - lastSync > 300000) { // 5 minutes
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("Performing periodic sync with server...");
            syncUsersWithServer();
        } else {
            Serial.println("Skipping sync - WiFi offline (will sync when connected)");
        }
        lastSync = millis();
    }
    
    delay(100);
}