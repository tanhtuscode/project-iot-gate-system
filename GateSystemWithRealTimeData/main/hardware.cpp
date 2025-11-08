#include "hardware.h"
#include "display.h"

// ================== Firmware Version ==================
const char* FW_VERSION = "2.1";

// ================== Hardware Objects ==================
Adafruit_SSD1306 display(OLED_W, OLED_H, &Wire, -1);
MFRC522 rfid(RC522_SS, RC522_RST);
Servo gateServo;
RTC_DS1307 rtc;  // Using DS1307 for Tiny RTC module

// ================== Gate State ==================
unsigned long gateCloseAtMs = 0;
bool gateIsOpen = false;

// ================== Hardware Initialization ==================
bool initializeHardware() {
    Serial.println("Initializing hardware...");
    
    // Initialize I2C bus for OLED and RTC
    Wire.begin(OLED_SDA, OLED_SCL);
    Serial.println("I2C initialized");
    
    // Initialize display first - critical component
    Serial.println("Initializing display...");
    if (!initializeDisplay()) {
        Serial.println("CRITICAL: Display initialization failed!");
        return false;
    }
    Serial.println("Display OK");
    
    // Initialize other components - continue even if some fail
    Serial.println("Initializing RFID...");
    bool rfidOk = initializeRFID();
    Serial.println(rfidOk ? "RFID OK" : "RFID FAILED");
    showInitProgress("RFID", rfidOk);
    
    Serial.println("Initializing servo...");
    bool servoOk = initializeServo();
    Serial.println(servoOk ? "Servo OK" : "Servo FAILED");
    showInitProgress("Servo", servoOk);
    
    Serial.println("Initializing LED...");
    bool ledOk = initializeLED();
    Serial.println(ledOk ? "LED OK" : "LED FAILED");
    showInitProgress("LED", ledOk);
    
    Serial.println("Initializing RTC...");
    bool rtcOk = initializeRTC();
    Serial.println(rtcOk ? "RTC OK" : "RTC FAILED");
    showInitProgress("RTC", rtcOk);
    
    // Show summary
    // Show completion message on display
    delay(1000); // Show final status
    
    Serial.println("=== Hardware Status ===");
    Serial.printf("RFID: %s\n", rfidOk ? "OK" : "FAIL");
    Serial.printf("Servo: %s\n", servoOk ? "OK" : "FAIL");
    Serial.printf("LED: %s\n", ledOk ? "OK" : "FAIL");
    Serial.printf("RTC: %s\n", rtcOk ? "OK" : "FAIL");
    Serial.println("Hardware initialization complete");
    
    // Show completion on display
    display.fillRect(0, 54, OLED_W, 10, SSD1306_BLACK);
    display.setCursor(0, 54);
    display.println("Hardware Ready!");
    display.display();
    delay(1000);
    
    return true;  // Continue even if some components failed
}

bool initializeRFID() {
    Serial.println("Starting SPI for RFID...");
    SPI.begin(RC522_SCK, RC522_MISO, RC522_MOSI, RC522_SS);
    
    Serial.println("Initializing RFID module...");
    rfid.PCD_Init();
    delay(100);
    
    Serial.println("Testing RFID reader...");
    // Skip self-test as it can hang - just do basic check
    byte version = rfid.PCD_ReadRegister(MFRC522::VersionReg);
    if (version == 0x00 || version == 0xFF) {
        Serial.println("WARNING: RFID reader not detected or communication failed");
        Serial.printf("Version register: 0x%02X\n", version);
        return false;
    } else {
        Serial.printf("RFID reader detected - Version: 0x%02X\n", version);
        Serial.println("RFID reader initialized successfully");
        return true;
    }
}

bool initializeServo() {
    gateServo.attach(SERVO_PIN);
    gateServo.write(GATE_CLOSED_DEG);
    gateIsOpen = false;
    gateCloseAtMs = 0;
    Serial.println("Servo gate initialized (closed position)");
    return true;
}

bool initializeLED() {
    pinMode(LED_R_PIN, OUTPUT);
    pinMode(LED_G_PIN, OUTPUT);
    pinMode(LED_B_PIN, OUTPUT);
    
    // Test LED sequence
    setLED(true, false, false);  // Red
    delay(200);
    setLED(false, true, false);  // Green
    delay(200);
    setLED(false, false, true);  // Blue
    delay(200);
    ledOff();
    
    Serial.println("RGB LED initialized");
    return true;
}

bool initializeRTC() {
    Serial.println("Attempting RTC connection...");
    
    // Skip diagnostic to prevent hanging
    Serial.println("Skipping RTC diagnostic for faster boot");
    
    // Add timeout for RTC initialization
    unsigned long startTime = millis();
    bool rtcFound = false;
    
    while (millis() - startTime < 2000) { // 2 second timeout
        if (rtc.begin()) {
            rtcFound = true;
            break;
        }
        delay(100);
    }
    
    if (!rtcFound) {
        Serial.println("WARNING: RTC module not found or timeout!");
        return false;
    }
    
    Serial.println("RTC module detected, checking time...");
    
    try {
        // For DS1307, we check if the time is valid (not 2000-01-01)
        DateTime now = rtc.now();
        if (now.year() < 2023) {
            Serial.println("RTC time invalid, setting time from compile time");
            rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
            delay(100); // Wait for RTC to update
            now = rtc.now();
        }
        
        Serial.printf("RTC initialized - Current time: %04d-%02d-%02d %02d:%02d:%02d\n",
                      now.year(), now.month(), now.day(),
                      now.hour(), now.minute(), now.second());
        return true;
    } catch (...) {
        Serial.println("WARNING: RTC communication error!");
        return false;
    }
}

// ================== LED Control Functions ==================
void setLED(bool r, bool g, bool b) {
    digitalWrite(LED_R_PIN, r ? HIGH : LOW);
    digitalWrite(LED_G_PIN, g ? HIGH : LOW);
    digitalWrite(LED_B_PIN, b ? HIGH : LOW);
}

void ledIdleBlue() {
    setLED(false, false, true);
}

void ledAccessGranted() {
    setLED(false, true, false);
}

void ledAccessDenied() {
    setLED(true, false, false);
}

void ledOff() {
    setLED(false, false, false);
}

// ================== Gate Control Functions ==================
void gateOpen() {
    gateServo.write(GATE_OPEN_DEG);
    gateCloseAtMs = millis() + GATE_OPEN_MS;
    gateIsOpen = true;
    Serial.println("Gate opened");
}

void gateClose() {
    gateServo.write(GATE_CLOSED_DEG);
    gateCloseAtMs = 0;
    gateIsOpen = false;
    Serial.println("Gate closed");
}

void gateMaybeClose() {
    if (gateCloseAtMs && millis() >= gateCloseAtMs) {
        gateClose();
    }
}

void openGate() {
    gateOpen();  // Alias for gateOpen
}

void handleGateControl() {
    gateMaybeClose();  // Alias for gateMaybeClose
}

// ================== Hardware Test Functions ==================
bool testRFID() {
    return rfid.PCD_PerformSelfTest();
}

bool testOLED() {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("OLED Test");
    display.display();
    return true;
}

bool testLED() {
    // Test each color
    setLED(true, false, false);
    delay(100);
    setLED(false, true, false);
    delay(100);
    setLED(false, false, true);
    delay(100);
    ledOff();
    return true;
}

bool testServo() {
    int currentPos = gateServo.read();
    gateServo.write(GATE_OPEN_DEG);
    delay(500);
    gateServo.write(GATE_CLOSED_DEG);
    delay(500);
    return true;
}

bool testRTC() {
    if (!rtc.begin()) {
        return false;
    }
    DateTime now = rtc.now();
    return (now.year() > 2000); // Basic sanity check
}

// ================== I2C Diagnostic Functions ==================
void scanI2CDevices() {
    Serial.println("=== I2C Device Scanner ===");
    Serial.println("Scanning I2C bus for devices...");
    Serial.printf("SDA: Pin %d, SCL: Pin %d\n", RTC_SDA, RTC_SCL);
    
    int deviceCount = 0;
    
    for (byte address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        byte error = Wire.endTransmission();
        
        if (error == 0) {
            Serial.printf("I2C device found at address 0x%02X", address);
            
            // Identify common devices
            switch (address) {
                case 0x3C:
                case 0x3D:
                    Serial.print(" (OLED Display)");
                    break;
                case 0x68:
                    Serial.print(" (DS1307 RTC or DS3231 RTC)");
                    break;
                case 0x50:
                    Serial.print(" (EEPROM - often on RTC modules)");
                    break;
                case 0x57:
                    Serial.print(" (EEPROM)");
                    break;
                default:
                    Serial.print(" (Unknown device)");
                    break;
            }
            Serial.println();
            deviceCount++;
        } else if (error == 4) {
            Serial.printf("Unknown error at address 0x%02X\n", address);
        }
    }
    
    if (deviceCount == 0) {
        Serial.println("No I2C devices found!");
    } else {
        Serial.printf("Found %d I2C device(s)\n", deviceCount);
    }
    Serial.println("=== Scan Complete ===\n");
}

void diagnoseRTC() {
    Serial.println("=== RTC Diagnostic ===");
    
    // First scan I2C to see what's connected
    scanI2CDevices();
    
    // Check if DS1307 is at expected address (0x68)
    Wire.beginTransmission(0x68);
    byte error = Wire.endTransmission();
    
    if (error == 0) {
        Serial.println("✓ DS1307 RTC detected at address 0x68");
        
        // Try to initialize RTC
        if (rtc.begin()) {
            Serial.println("✓ RTC initialization successful");
            
            // Check if RTC is running
            DateTime now = rtc.now();
            Serial.printf("Current RTC time: %04d-%02d-%02d %02d:%02d:%02d\n",
                          now.year(), now.month(), now.day(),
                          now.hour(), now.minute(), now.second());
            
            if (now.year() < 2020) {
                Serial.println("⚠ RTC time seems invalid (year < 2020)");
                Serial.println("This might indicate:");
                Serial.println("  - RTC battery is dead/missing");
                Serial.println("  - RTC needs to be set for first time");
            } else {
                Serial.println("✓ RTC time looks valid");
            }
            
            // Test RTC communication by reading seconds twice
            byte sec1 = now.second();
            delay(1100);
            DateTime now2 = rtc.now();
            byte sec2 = now2.second();
            
            if (sec1 != sec2) {
                Serial.println("✓ RTC is running (time is advancing)");
            } else {
                Serial.println("⚠ RTC might not be running (time not advancing)");
            }
            
        } else {
            Serial.println("✗ RTC initialization failed!");
        }
    } else {
        Serial.println("✗ No device found at 0x68 (DS1307 RTC expected address)");
        Serial.println("Possible issues:");
        Serial.println("  - RTC module not connected");
        Serial.println("  - Wrong I2C pins (should be SDA=21, SCL=22)");
        Serial.println("  - RTC module uses different address");
        Serial.println("  - Faulty RTC module");
    }
    
    Serial.println("=== RTC Diagnostic Complete ===\n");
}