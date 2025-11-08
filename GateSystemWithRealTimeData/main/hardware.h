#ifndef HARDWARE_H
#define HARDWARE_H

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <MFRC522.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>
#include <RTClib.h>

// ================== Firmware Version ==================
extern const char* FW_VERSION;

// ================== Hardware Pin Definitions ==================
// OLED Display (I2C)
#define OLED_W         128
#define OLED_H          64
#define OLED_ADDR      0x3C
#define OLED_SDA        21
#define OLED_SCL        22

// RC522 RFID (VSPI)
#define RC522_SS         5
#define RC522_RST       27
#define RC522_SCK       18
#define RC522_MOSI      23
#define RC522_MISO      19

// Servo Gate Control
#define SERVO_PIN       25
#define GATE_CLOSED_DEG  0
#define GATE_OPEN_DEG   90
#define GATE_OPEN_MS    2000UL

// RGB LED Control (Fixed pin assignments)
#define LED_R_PIN       26
#define LED_G_PIN       33
#define LED_B_PIN       32

// RTC Module (I2C - same bus as OLED)
// Tiny RTC module typically uses DS1307 chip
#define RTC_SDA         21
#define RTC_SCL         22

// ================== Hardware Objects ==================
extern Adafruit_SSD1306 display;
extern MFRC522 rfid;
extern Servo gateServo;
extern RTC_DS1307 rtc;  // Changed to DS1307 for Tiny RTC module

// ================== Gate State ==================
extern unsigned long gateCloseAtMs;
extern bool gateIsOpen;

// ================== Hardware Functions ==================
bool initializeHardware();
bool initializeDisplay();
bool initializeRFID();
bool initializeServo();
bool initializeLED();
bool initializeRTC();

// LED Control Functions
void setLED(bool r, bool g, bool b);
void ledIdleBlue();
void ledAccessGranted();
void ledAccessDenied();
void ledOff();

// Gate Control Functions
void gateOpen();
void openGate();  // Alias for gateOpen
void gateClose();
void gateMaybeClose();
void handleGateControl();  // Alias for gateMaybeClose

// Hardware Test Functions
bool testRFID();
bool testOLED();
bool testLED();
bool testServo();
bool testRTC();

// I2C Diagnostic Functions
void scanI2CDevices();
void diagnoseRTC();

#endif // HARDWARE_H