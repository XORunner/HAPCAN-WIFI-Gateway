/*
  Project Name: HAPCAN WiFi Gateway
  Description:
    - Loads Wi‑Fi credentials from NVS. If connection fails (or no credentials exist),
      the ESP32 starts in Access Point mode with SSID "HAPCAN" for configuration.
    - Once connected, the OLED top bar displays the client count and IP address.
    - Uses FreeRTOS tasks to update the OLED, process TWAI (CAN) frames, and handle incoming TCP socket data.
    - Debug messages are printed to Serial only.
  
  Hardware:
    - ESP32-S2 (WiFi + TWAI)
    - OLED display (SSD1306, 128x64, I2C)
    - CAN transceiver
  
  Connections:
    - OLED SDA -> GPIO 21
    - OLED SCL -> GPIO 22
    - CAN TX -> GPIO 5
    - CAN RX -> GPIO 4

  WiFi AP: https://hapcan.local Cred. HAPCAN/12345678
*/

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <ESPAsyncWebServer.h>  // Using ESPAsyncWebServer library
#include <Wire.h>
#include "driver/twai.h"

#include "common.h"    // Defines HAPCAN_START_BYTE, HAPCAN_END_BYTE, MAX_CLIENTS
#include "clients.h"   // Full definition of SocketConnection and global clients array
#include "oled.h"      // OLED display functions
#include "HTTP.h"      // HTTP/AP mode functionality
#include "hapcan.h"    // HAPCAN-related functions

#include <Adafruit_GFX.h>  // Required by oled.cpp

// ----- Global Instances -----
Preferences preferences;         // Global Preferences instance (defined once)
HTTP http;                       // HTTP object for AP mode configuration

// WiFi & TCP server for CAN/TCP communication.
const uint16_t TCP_PORT = 10001;
WiFiServer wifiServer(TCP_PORT);

// Define the global clients array (definition in clients.h)
SocketConnection clients[MAX_CLIENTS];

// ----- TWAI (CAN) Configuration -----
twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_7, GPIO_NUM_6, TWAI_MODE_NORMAL);
twai_timing_config_t t_config = TWAI_TIMING_CONFIG_125KBITS();
twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

// ----- FreeRTOS Task Prototypes -----
void oledTask(void * parameter);
void hapcanTask(void * parameter);
void socketTask(void * parameter);

// ----- Debug: Print Reset Reason -----
void printResetReason() {
  esp_reset_reason_t resetReason = esp_reset_reason();
  Serial.print("Last Reset Reason: ");
  switch (resetReason) {
    case ESP_RST_UNKNOWN: Serial.println("Unknown"); break;
    case ESP_RST_POWERON: Serial.println("Power-on Reset"); break;
    case ESP_RST_EXT: Serial.println("External Pin Reset"); break;
    case ESP_RST_SW: Serial.println("Software Reset"); break;
    case ESP_RST_PANIC: Serial.println("Panic Reset (Crash)"); break;
    case ESP_RST_INT_WDT: Serial.println("Interrupt Watchdog Reset"); break;
    case ESP_RST_TASK_WDT: Serial.println("Task Watchdog Reset"); break;
    case ESP_RST_WDT: Serial.println("Other Watchdog Reset"); break;
    case ESP_RST_DEEPSLEEP: Serial.println("Deep Sleep Reset"); break;
    case ESP_RST_BROWNOUT: Serial.println("Brownout Reset"); break;
    case ESP_RST_SDIO: Serial.println("SDIO Reset"); break;
    default: Serial.println("No Meaningful Reset Reason"); break;
  }
}

// ----- Wi-Fi Connection Logic for Main -----
bool connectToStoredWiFi() {
  preferences.begin("wifi", false);
  String storedSSID = preferences.getString("ssid", "");
  String storedPass = preferences.getString("password", "");
  preferences.end();
  
  if (storedSSID == "") return false;
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(storedSSID.c_str(), storedPass.c_str());
  
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
    delay(500);
  }
  return (WiFi.status() == WL_CONNECTED);
}

// ----- FreeRTOS Tasks -----
// OLED Refresh Task: updates the OLED display (top bar and message area) every second.
void oledTask(void * parameter) {
  for (;;) {
    updateOLEDDisplay();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

// HAPCAN Task: polls TWAI for CAN messages and updates the OLED message area.
void hapcanTask(void * parameter) {
  twai_message_t rx_msg;
  for (;;) {
    if (twai_receive(&rx_msg, 0) == ESP_OK) {
      uint8_t outFrame[15];
      size_t outLen = encodeFrame(rx_msg, outFrame);
      broadcastFrame(outFrame, outLen);
      addHAPCANDisplayMessage(false, outFrame, outLen);
      
      // Also output CAN traffic to Serial with "<-" prefix.
      String debugMsg = "<- ";
      for (size_t i = 0; i < outLen; i++) {
        if (outFrame[i] < 16) debugMsg += "0";
        debugMsg += String(outFrame[i], HEX) + ":";
      }
      Serial.println(debugMsg);
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// Socket Task: accepts new TCP client connections and processes incoming data.
void socketTask(void * parameter) {
  for (;;) {
    WiFiClient newClient = wifiServer.available();
    if (newClient && newClient.connected()) {
      bool added = false;
      for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].active || !clients[i].client.connected()) {
          if (clients[i].active) clients[i].client.stop();
          clients[i].client = newClient;
          clients[i].active = true;
          clients[i].parser.reset();
          Serial.println("New client: " + newClient.remoteIP().toString());
          added = true;
          break;
        }
      }
      if (!added) {
        newClient.stop();
        Serial.println("Rejected client (max connections reached)");
      }
    }
    
    // Update the client count on the OLED.
    int count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (clients[i].active && clients[i].client.connected()) count++;
    }
    setOLEDClientCount(count);
    
    // Process incoming data from active clients.
    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (clients[i].active && clients[i].client.connected()) {
        while (clients[i].client.available() > 0) {
          uint8_t byte = clients[i].client.read();
          if (clients[i].parser.parseByte(byte)) {
            size_t len = clients[i].parser.getFrameLength();
            processFrame(clients[i].parser.getFrame(), len);
            clients[i].parser.reset();
          }
        }
      }
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// ----- Setup -----
void setup() {
  Serial.begin(115200);
  printResetReason();

  // preferences.begin("wlan", false);
  // preferences.clear();
  // preferences.end();
  // preferences.begin("wifi", false);
  // preferences.clear();
  // preferences.end();
  
  initOLED();
  setOLEDClientCount(0);
  updateOLEDDisplay();
  
  if (connectToStoredWiFi()) {
    Serial.println("Connected to Wi-Fi: " + WiFi.localIP().toString());
    wifiServer.begin();  // Start TCP server on port 10001.
  } else {
    Serial.println("Wi-Fi connection failed, entering AP mode");
    http.begin();  // Start AP mode with SSID "HAPCAN-WLAN"
  }
  
  esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);
  if (err == ESP_OK) {
    twai_start();
    Serial.println("TWAI started");
  } else {
    Serial.println("TWAI install failed: " + String(err));
  }
  
  // Create FreeRTOS tasks.
  xTaskCreate(oledTask, "OLED Task", 8192, NULL, 1, NULL);
  xTaskCreate(hapcanTask, "HAPCAN Task", 8192, NULL, 1, NULL);
  xTaskCreate(socketTask, "Socket Task", 8192, NULL, 1, NULL);
}

void loop() {
  // AsyncWebServer runs in the background.
  vTaskDelay(10 / portTICK_PERIOD_MS);
}
