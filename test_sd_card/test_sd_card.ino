#include <WiFi.h>
#include <SPIFFS.h>  // For serving files from SPIFFS (optional)
#include <WebServer.h>  // Choose between WebServer or ESPAsyncWebServer
#include <FS.h>
#include <SD.h>
#include <SPI.h>

const char* apSSID = "RiX System v1.0";
const char* apPassword = "";

const int chipSelect = 5;

WebServer server(80);  // Or ESPAsyncWebServer(80);

// If using SPIFFS:
#ifdef USE_SPIFFS

// Define the path to your Flutter web app directory in SPIFFS
const char* spiffsPath = "/flutter_web_app";

void setupSPIFFS() {
  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS mount failed!");
    return;
  }

  Serial.println("SPIFFS mounted successfully.");
}

#endif  // USE_SPIFFS

void setup() {
  Serial.begin(115200);

  // Initialize SD card (if using SD card)
  if (SD.begin(chipSelect)) {
    Serial.println("SD Card mounted successfully.");
  } else {
    Serial.println("SD Card failed to mount.");
  }

  initWiFi();
}

void loop() {
  server.handleClient();
}

void initWiFi() {
  WiFi.softAP(apSSID, apPassword);
  Serial.println("Access Point Started");
  Serial.println("IP Address: " + WiFi.softAPIP().toString());

  server.on("/", HTTP_GET, handleRoot);
  server.onNotFound(handleFileRequest);

  server.begin();
  Serial.println("Server started.");
}

void handleRoot() {
  // Serve the index.html from SPIFFS or SD card
  #ifdef USE_SPIFFS
    File file = SPIFFS.open(spiffsPath + "/index.html");
  #else
    File file = SD.open("/index.html");  // Replace with your actual path
  #endif

  if (file) {
    server.streamFile(file, "text/html");
    file.close();
  } else {
    server.send(404, "text/plain", "Not file found or Invalid file found. Make sure you have flashed true copy of the firmware.");
  }
}

void handleFileRequest() {
  String path = server.uri();
  if (path == "/") {
    path = "/index.html"; // Default file to serve
  }

  // Check both SPIFFS and SD card (order depends on your preference)
  #ifdef USE_SPIFFS
    File file = SPIFFS.open(path);
    if (!file) {
  #endif
      File file = SD.open(path);
  #ifdef USE_SPIFFS
    }
  #endif

  if (file) {
    String contentType = getContentType(path);  // Use a function to determine content type
    server.streamFile(file, contentType);
    file.close();
  } else {
    server.send(404, "text/plain", "File not found");
  }
}

String getContentType(const String& path) {
  String contentType = "text/html";
  if (path.endsWith(".css")) {
    contentType = "text/css";
  } else if (path.endsWith(".js")) {
    contentType = "application/javascript";
  } else if (path.endsWith(".png")) {
    contentType = "image/png";
  }
}