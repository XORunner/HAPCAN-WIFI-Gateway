#ifndef HTTP_H
#define HTTP_H

#include <ESPAsyncWebServer.h>  // Use ESPAsyncWebServer for asynchronous HTTP handling
#include <Preferences.h>

extern Preferences preferences;

class HTTP {
public:
  HTTP();
  void begin();
  bool connectToWiFi();
  void startAPMode();
  void serveWiFiConfigPage();
  void serveDALIConfigPage();
  void handleResetConfig(AsyncWebServerRequest* request); // Fixed declaration
  
  AsyncWebServer server; // Now properly declared
  String ssid;
  String password;
};

#endif