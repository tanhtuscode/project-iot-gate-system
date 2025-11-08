#include "display.h"
#include <WiFi.h>
#include "hardware.h"
#include "users.h"

// ================== Display State ==================
bool displayBusy = false;
unsigned long displayUntilMs = 0;
unsigned long lastClockUpdate = 0;
bool showing = false;
unsigned long showUntilMs = 0;

// ================== Display Initialization ==================
bool initializeDisplay() {
    // Initialize the OLED display
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println("ERROR: OLED display initialization failed!");
        return false;
    }
    
    Serial.println("OLED display initialized successfully");
    
    // Show startup message
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Gate System v" + String(FW_VERSION));
    display.setCursor(0, 12);
    display.println("Starting up...");
    display.display();
    delay(1000);
    
    Serial.println("OLED startup screen shown");
    return true;
}

void updateDisplay() {
    // Update clock regularly
    if (millis() - lastClockUpdate > CLOCK_UPDATE_INTERVAL) {
        if (!displayBusy) {
            showIdleScreen(); // Refresh idle screen with updated time
        }
        lastClockUpdate = millis();
    }
    
    // Check if temporary screen should return to idle
    if (displayBusy && millis() >= displayUntilMs) {
        displayBusy = false;
        showIdleScreen();
    }
}

void showScreen(DisplayScreen screen, const String& message) {
    switch (screen) {
        case SCREEN_IDLE:
            showIdleScreen();
            break;
        case SCREEN_CARD_DETECTED:
            showCardDetectedScreen(message);
            break;
        case SCREEN_ACCESS_GRANTED:
            showAccessGrantedScreen(message, 0, true); // Default values
            break;
        case SCREEN_ACCESS_DENIED:
            showAccessDeniedScreen(message);
            break;
        case SCREEN_INPUT_MODE:
            showInputModeScreen(message);
            break;
        case SCREEN_SYSTEM_INFO:
            showSystemInfoScreen();
            break;
        case SCREEN_ERROR:
            showErrorScreen(message);
            break;
    }
}

// ================== Specific Screen Functions ==================
void showIdleScreen() {
    display.clearDisplay();
    drawHeaderWithClock("Gate System");
    
    // Main content area
    display.setTextSize(1);
    display.setCursor(0, 28);
    
    // Greeting based on time (with RTC error handling)
    String greeting = "Good Day";
    try {
        DateTime now = rtc.now();
        if (now.year() >= 2023) { // Valid RTC time
            if (now.hour() >= 6 && now.hour() < 12) {
                greeting = "Good Morning";
            } else if (now.hour() >= 12 && now.hour() < 18) {
                greeting = "Good Afternoon";  
            } else if (now.hour() >= 18) {
                greeting = "Good Evening";
            }
        }
    } catch (...) {
        // RTC error, use default greeting
        greeting = "Welcome";
    }
    
    display.println(greeting + "!");
    display.setCursor(0, 40);
    
    // Show different message based on input mode
    if (isInputModeActive()) {
        display.println("INPUT MODE ACTIVE");
        display.setCursor(0, 50);
        display.println("Scan new cards to add");
    } else {
        display.println("Please scan your ID");
    }
    
    // Status line - show WiFi status
    display.setCursor(0, 54);
    if (WiFi.status() == WL_CONNECTED) {
        display.println("WiFi: Connected");
    } else {
        display.println("WiFi: Connecting...");
    }
    
    display.display();
    displayBusy = false;
}

void showCardDetectedScreen(const String& uid) {
    display.clearDisplay();
    drawHeaderWithClock("Card Detected");
    
    display.setTextSize(1);
    display.setCursor(0, 28);
    display.println("UID: " + truncateText(uid, 18));
    display.setCursor(0, 40);
    display.println("Processing...");
    
    display.display();
    displayBusy = true;
    displayUntilMs = millis() + DISPLAY_TIMEOUT_MS;
}

void showAccessGrantedScreen(const String& name, long credit, bool isEntry) {
    display.clearDisplay();
    drawHeaderWithClock("Access Granted");
    
    display.setTextSize(1);
    display.setCursor(0, 28);
    display.println("User: " + truncateText(name, 18));
    display.setCursor(0, 40);
    display.println(isEntry ? "Welcome IN" : "Safe travels OUT");
    display.setCursor(0, 52);
    display.println("Balance: " + String(credit) + " VND");
    
    display.display();
    displayBusy = true;
    displayUntilMs = millis() + DISPLAY_TIMEOUT_MS;
    
    animateAccessGranted();
}

void showAccessDeniedScreen(const String& reason) {
    display.clearDisplay();
    drawHeaderWithClock("Access Denied");
    
    display.setTextSize(2);
    display.setCursor(10, 30);
    display.println("DENIED");
    
    display.setTextSize(1);
    display.setCursor(0, 52);
    display.println(truncateText(reason, 21));
    
    display.display();
    displayBusy = true;
    displayUntilMs = millis() + DISPLAY_TIMEOUT_MS;
    
    animateAccessDenied();
}

void showInputModeScreen(const String& status) {
    display.clearDisplay();
    drawHeaderWithClock("Input Mode");
    
    display.setTextSize(1);
    display.setCursor(0, 28);
    display.println("Status: " + status);
    display.setCursor(0, 40);
    display.println("Scan card to register");
    display.setCursor(0, 52);
    display.println("new user...");
    
    display.display();
    displayBusy = true;
    displayUntilMs = millis() + 5000; // Longer timeout for input mode
    
    animateInputMode();
}

void showSystemInfoScreen() {
    display.clearDisplay();
    drawHeaderWithClock("System Ready");
    
    display.setTextSize(1);
    display.setCursor(0, 28);
    
    // Show IP address prominently
    if (WiFi.status() == WL_CONNECTED) {
        String ip = WiFi.localIP().toString();
        display.println("IP: " + ip);
        display.setCursor(0, 36);
        display.println("SSID: " + WiFi.SSID());
        display.setCursor(0, 44);
        display.println("Signal: " + String(WiFi.RSSI()) + " dBm");
    } else {
        display.println("WiFi: Not connected");
        display.setCursor(0, 36);
        display.println("Check network config");
    }
    
    display.setCursor(0, 52);
    display.println("FW: v" + String(FW_VERSION));
    
    display.display();
    displayBusy = true;
    displayUntilMs = millis() + 5000;
}

void showErrorScreen(const String& error) {
    display.clearDisplay();
    drawHeaderWithClock("System Error");
    
    display.setTextSize(1);
    display.setCursor(0, 28);
    display.println("ERROR:");
    display.setCursor(0, 40);
    // Split long error messages
    if (error.length() > 21) {
        display.println(error.substring(0, 21));
        display.setCursor(0, 48);
        display.println(error.substring(21, 42));
        if (error.length() > 42) {
            display.setCursor(0, 56);
            display.println(error.substring(42, 63));
        }
    } else {
        display.println(error);
    }
    
    display.display();
    displayBusy = true;
    displayUntilMs = millis() + 5000;
}

void showInitProgress(const String& component, bool success) {
    static int progressY = 24;
    
    // If this is the first component, clear the lower part
    if (component == "RFID") {
        display.fillRect(0, 24, OLED_W, OLED_H - 24, SSD1306_BLACK);
        progressY = 24;
    }
    
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, progressY);
    
    String status = component + ": ";
    status += success ? "OK" : "FAIL";
    display.println(status);
    display.display();
    
    progressY += 10;
    if (progressY > 54) progressY = 54; // Don't go off screen
    
    delay(500); // Show progress for a moment
}

// ================== Clock and Time Functions ==================
void drawHeaderWithClock(const String& title) {
    // Clear header area
    display.fillRect(0, 0, OLED_W, 22, SSD1306_BLACK);
    
    // Draw title
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(title);
    
    // Draw current time and date
    display.setCursor(0, 10);
    display.println(getFormattedDate() + " " + getFormattedTime());
    
    // Draw separator line
    display.drawLine(0, 20, OLED_W, 20, SSD1306_WHITE);
}

void drawClock() {
    display.setCursor(0, 10);
    display.println(getFormattedDate() + " " + getFormattedTime());
}

String getFormattedTime() {
    try {
        DateTime now = rtc.now();
        if (now.year() >= 2023) { // Valid RTC time
            char timeStr[10];
            sprintf(timeStr, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
            return String(timeStr);
        }
    } catch (...) {
        // RTC error
    }
    
    // Fallback to uptime if RTC fails
    unsigned long uptime = millis() / 1000;
    int hours = (uptime / 3600) % 24;
    int minutes = (uptime / 60) % 60;
    int seconds = uptime % 60;
    char timeStr[10];
    sprintf(timeStr, "%02d:%02d:%02d", hours, minutes, seconds);
    return String(timeStr);
}

String getFormattedDate() {
    try {
        DateTime now = rtc.now();
        if (now.year() >= 2023) { // Valid RTC time
            char dateStr[12];
            sprintf(dateStr, "%04d-%02d-%02d", now.year(), now.month(), now.day());
            return String(dateStr);
        }
    } catch (...) {
        // RTC error
    }
    
    // Fallback to system uptime in days
    return "Boot Day " + String(millis() / 86400000 + 1);
}

String getTimeString() {
    return getFormattedTime();
}

String getDateString() {
    return getFormattedDate();
}

// ================== Utility Functions ==================
void clearDisplayArea(int x, int y, int w, int h) {
    display.fillRect(x, y, w, h, SSD1306_BLACK);
}

void drawCenteredText(const String& text, int y, int textSize) {
    display.setTextSize(textSize);
    int textWidth = text.length() * 6 * textSize; // Approximate character width
    int x = (OLED_W - textWidth) / 2;
    if (x < 0) x = 0;
    display.setCursor(x, y);
    display.println(text);
}

void drawProgressBar(int x, int y, int width, int height, int progress) {
    // Draw border
    display.drawRect(x, y, width, height, SSD1306_WHITE);
    
    // Draw filled portion
    int fillWidth = (width - 2) * progress / 100;
    if (fillWidth > 0) {
        display.fillRect(x + 1, y + 1, fillWidth, height - 2, SSD1306_WHITE);
    }
}

String truncateText(const String& text, int maxChars) {
    if (text.length() <= maxChars) {
        return text;
    }
    return text.substring(0, maxChars - 3) + "...";
}

// ================== Display Animation Functions ==================
void animateAccessGranted() {
    // Quick green flash effect (handled by LED in hardware)
    // This could be expanded with display animations
}

void animateAccessDenied() {
    // Quick red flash effect (handled by LED in hardware)
    // This could be expanded with display animations
}

void animateInputMode() {
    // Subtle animation for input mode (could add blinking cursor, etc.)
}