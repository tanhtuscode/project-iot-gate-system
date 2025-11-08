#ifndef DISPLAY_H
#define DISPLAY_H

// ================== Display State Variables ==================
extern bool displayBusy;
extern unsigned long displayUntilMs;
extern unsigned long lastClockUpdate;
extern bool showing;
extern unsigned long showUntilMs;

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>

// ================== Display Configuration ==================
#define DISPLAY_TIMEOUT_MS 3000
#define CLOCK_UPDATE_INTERVAL 1000

// ================== Display State ==================
extern bool displayBusy;
extern unsigned long displayUntilMs;
extern unsigned long lastClockUpdate;

// ================== Display Screen Types ==================
enum DisplayScreen {
    SCREEN_IDLE,
    SCREEN_CARD_DETECTED,
    SCREEN_ACCESS_GRANTED,
    SCREEN_ACCESS_DENIED,
    SCREEN_INPUT_MODE,
    SCREEN_SYSTEM_INFO,
    SCREEN_ERROR
};

// ================== Display Functions ==================
bool initializeDisplay();
void clearDisplay();
void setTextSize(int size);
void setCursor(int x, int y);
void printDisplay(const String& text);
void updateDisplay();
void showScreen(DisplayScreen screen, const String& message = "");

// ================== Specific Screen Functions ==================
void showIdleScreen();
void showCardDetectedScreen(const String& uid);
void showAccessGrantedScreen(const String& name, long credit, bool isEntry);
void showAccessDeniedScreen(const String& reason);
void showInputModeScreen(const String& status);
void showSystemInfoScreen();
void showErrorScreen(const String& error);
void showInitProgress(const String& component, bool success);

// ================== Clock and Time Functions ==================
void drawHeaderWithClock(const String& title);
void drawClock();
String getFormattedTime();
String getFormattedDate();
String getTimeString();
String getCurrentTimeString();  // Alias for getTimeString
String getDateString();
String formatCurrency(long amount);
String formatCredit(long amount);

// ================== Utility Functions ==================
void clearDisplayArea(int x, int y, int w, int h);
void drawCenteredText(const String& text, int y, int textSize = 1);
void drawProgressBar(int x, int y, int width, int height, int progress);
String truncateText(const String& text, int maxChars);

// ================== Display Animation Functions ==================
void animateAccessGranted();
void animateAccessDenied();
void animateInputMode();

#endif // DISPLAY_H