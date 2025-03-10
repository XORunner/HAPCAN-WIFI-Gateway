#ifndef OLED_H
#define OLED_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "common.h"  // Include common definitions

// OLED configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_I2C_ADDRESS 0x3C

// Declare the OLED display object (defined in oled.cpp)
extern Adafruit_SSD1306 display;

// Maximum number of HAPCAN messages to display in the message area
#define MAX_DISPLAY_MESSAGES 3

// Structure to hold a two‐line HAPCAN message for the OLED
struct HAPCANMsgDisplay {
  String row1;
  String row2;
};

// Global message buffer and counter (defined in oled.cpp)
extern HAPCANMsgDisplay msgBuffer[MAX_DISPLAY_MESSAGES];
extern int msgBufferCount;

// OLED‐related function prototypes
void initOLED();
void drawTopBar();
void updateOLEDDisplay();
void addHAPCANDisplayMessage(bool tcpToCan, const uint8_t* frame, size_t len);
void setOLEDClientCount(int count);

// Debug output function – during initial setup prints to Serial and updates the OLED debug area.
void debugPrintln(const String &msg);

// Call this at the end of initial setup so subsequent debug messages update only Serial.
void finishInitialSetup();

#endif
