#include "HTTP.h"
#include <WiFi.h>       // Required for AsyncWebServer
#include <ESPmDNS.h>    // Added for mDNS functionality
#include <Preferences.h>
#include "clients.h"

// HTML pages (unchanged)
const char* wifiConfigHTML = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Wi-Fi Configuration</title>
</head>
<body>
  <h1>Wi-Fi Configuration</h1>
  <form action="/save" method="POST">
    <label for="ssid">SSID:</label>
    <input type="text" id="ssid" name="ssid" required><br><br>
    <label for="password">Password:</label>
    <input type="password" id="password" name="password" required><br><br>
    <input type="submit" value="Save">
  </form>
  <br>
  <form action="/reset" method="POST">
    <input type="submit" value="Reset Configuration">
  </form>
</body>
</html>
)rawliteral";

const char* daliConfigHTML = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>DALI Configuration</title>
</head>
<body>
  <h1>DALI Configuration</h1>
  <p>Welcome to the DALI configuration page!</p>
  <form action="/reset" method="POST">
    <input type="submit" value="Reset Configuration">
  </form>
</body>
</html>
)rawliteral";

HTTP::HTTP() : server(80) {
  // Constructor
}

void HTTP::begin() {
  if (!connectToWiFi()) {
    startAPMode();
  } else {
    serveDALIConfigPage();
    server.begin();
  }
}

bool HTTP::connectToWiFi() {
  preferences.begin("wifi", true);
  ssid = preferences.getString("ssid", "");
  password = preferences.getString("password", "");
  preferences.end();

  if (ssid.isEmpty() || password.isEmpty()) {
    Serial.println("No Wi-Fi credentials stored.");
    return false;
  }

  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(ssid.c_str(), password.c_str());

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to Wi-Fi");
    Serial.println("IP Address: " + WiFi.localIP().toString());
    return true;
  } else {
    Serial.println("\nFailed to connect to Wi-Fi");
    return false;
  }
}

void HTTP::startAPMode() {
  // Use "HAPCAN-WLAN" as the AP SSID with a default password.
  WiFi.softAP("HAPCAN", "12345678");
  Serial.println("Access Point Started");
  Serial.println("AP IP Address: " + WiFi.softAPIP().toString());
  
  // Initialize mDNS with hostname "hapcan"
  if (!MDNS.begin("hapcan")) {
    Serial.println("Error setting up MDNS responder!");
  } else {
    Serial.println("mDNS responder started: http://hapcan.local");
  }
  
  serveWiFiConfigPage();
  server.begin();
}

void HTTP::serveWiFiConfigPage() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/html", wifiConfigHTML);
  });

  server.on("/save", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
      ssid = request->getParam("ssid", true)->value();
      password = request->getParam("password", true)->value();

      preferences.begin("wifi", false);
      preferences.putString("ssid", ssid);
      preferences.putString("password", password);
      preferences.end();

      request->send(200, "text/plain", "Credentials saved. Restarting...");
      delay(2000);
      ESP.restart();
    } else {
      request->send(400, "text/plain", "Missing SSID or Password");
    }
  });

  server.on("/reset", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleResetConfig(request);
  });
}

void HTTP::serveDALIConfigPage() {
    Serial.println("Serving DALI configuration page...");
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        Serial.println("Received request for DALI config page.");
        request->send(200, "text/html", daliConfigHTML);
    });

    // Handle reset request
    server.on("/reset", HTTP_POST, [this](AsyncWebServerRequest* request) {
        Serial.println("Received reset request.");
        handleResetConfig(request);
    });

    Serial.println("Starting server...");
    server.begin();
    Serial.println("Server started.");
}

void HTTP::handleResetConfig(AsyncWebServerRequest* request) {
  preferences.begin("wifi", false);
  preferences.clear();
  preferences.end();
  request->send(200, "text/plain", "Configuration reset. Restarting...");
  delay(2000);
  ESP.restart();
}
