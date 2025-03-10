#include "oled.h"
#include <WiFi.h>

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

HAPCANMsgDisplay msgBuffer[MAX_DISPLAY_MESSAGES];
int msgBufferCount = 0;
static int oledClientCount = 0;

static String debugOLEDStr = "";
static bool initialSetupDone = false;

static String padHex(uint8_t val) {
  String s = String(val, HEX);
  s.toUpperCase();
  if (s.length() < 2) s = "0" + s;
  return s;
}

void setOLEDClientCount(int count) {
  oledClientCount = count;
}

void initOLED() {
  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS)) {
    Serial.println("OLED init failed");
  } else {
    display.clearDisplay();
    display.display();
    Serial.println("OLED initialized");
  }
}

void drawTopBar() {
  display.fillRect(0, 0, SCREEN_WIDTH, 12, SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(0, 2);
  if (WiFi.getMode() == WIFI_AP) {
    display.print("Wi-Fi: Setup");  
  }else if (WiFi.status() != WL_CONNECTED) {
    display.print("Wi-Fi: Connecting...");
  } else {
    display.print("C:" + String(oledClientCount) + " IP:" + WiFi.localIP().toString());
  }
}

void updateOLEDDisplay() {
  display.clearDisplay();
  drawTopBar();
  
  // Only display the last 2 messages (4 lines) for the HAPCAN frames.
  int messagesToShow = (msgBufferCount > 2) ? 2 : msgBufferCount;
  int startIndex = (msgBufferCount > messagesToShow) ? (msgBufferCount - messagesToShow) : 0;
  
  // Push the message area 3 pixels down from top bar (top bar uses 12 pixels)
  int y = 15;
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if (WiFi.getMode() == WIFI_AP) {
    display.setCursor(0, 20);
    display.print("WiFi: HAPCAN");
    display.setCursor(0, 30);
    display.print("http://hapcan.local");
        display.setCursor(0, 40);
    display.print("or http://192.168.4.1");
  }

  for (int i = startIndex; i < msgBufferCount; i++) {
    display.setCursor(0, y);
    display.println(msgBuffer[i].row1);
    y += 8;
    display.setCursor(0, y);
    display.println(msgBuffer[i].row2);
    y += 8;
  }
  // Clear bottom debug area to remove artifacts.
  display.fillRect(0, SCREEN_HEIGHT - 8, SCREEN_WIDTH, 8, SSD1306_BLACK);
  
  display.display();
}

void updateOLEDDebugArea() {
  display.fillRect(0, SCREEN_HEIGHT - 8, SCREEN_WIDTH, 8, SSD1306_BLACK);
  display.setCursor(0, SCREEN_HEIGHT - 8);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.print(debugOLEDStr);
  display.display();
}

void addHAPCANDisplayMessage(bool tcpToCan, const uint8_t* frame, size_t len) {
  if (len != 15) return;
  
  String cmdPart = padHex(frame[2]) + padHex(frame[1]);
  String commandCode = cmdPart.substring(0, cmdPart.length() - 1);
  String responseFlag = cmdPart.substring(cmdPart.length() - 1);
  
  String nodeId = padHex(frame[3]);
  String groupId = padHex(frame[4]);
  
  String arrow = tcpToCan ? "->" : "<-";
  String row1 = arrow + commandCode + " (" + responseFlag + ") N:" + nodeId + " G:" + groupId;
  
  String row2 = "";
  for (int i = 5; i < 13; i++) {
    row2 += padHex(frame[i]);
  }
  
  if (msgBufferCount < MAX_DISPLAY_MESSAGES) {
    msgBuffer[msgBufferCount].row1 = row1;
    msgBuffer[msgBufferCount].row2 = row2;
    msgBufferCount++;
  } else {
    for (int i = 0; i < MAX_DISPLAY_MESSAGES - 1; i++) {
      msgBuffer[i] = msgBuffer[i + 1];
    }
    msgBuffer[MAX_DISPLAY_MESSAGES - 1].row1 = row1;
    msgBuffer[MAX_DISPLAY_MESSAGES - 1].row2 = row2;
  }
  updateOLEDDisplay();
  if (!initialSetupDone) updateOLEDDebugArea();
}

// void debugPrintln(const String &msg) {
//   Serial.println(msg);
//   if (!initialSetupDone) {
//     debugOLEDStr += msg + " ";
//     updateOLEDDebugArea();
//     delay(50);
//   }
// }

void debugPrintln(const String &msg) {
  Serial.println(msg);
  // Once initial setup is finished, do not update the OLED debug area.
  // if (!initialSetupDone) {
  //   debugOLEDStr += msg + " ";
  //   updateOLEDDebugArea();
  //   delay(50);
  // }
}


void finishInitialSetup() {
  initialSetupDone = true;
  debugOLEDStr = "";
  updateOLEDDebugArea();
}
