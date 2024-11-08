#include <WiFi.h>
#include <SPIFFS.h>
#include <WebServer.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>

char* apSSID = "ESP-32 AP";  // Name of the AP
char* apPassword = "";

const int chipSelect = 5;

const int dnsPort = 53;

WebServer server(80);

IPAddress local_IP(10, 0, 0, 2);  // IP address of the ESP32
IPAddress gateway(10, 0, 0, 1);   // Gateway address (same as ESP32 IP for AP)
IPAddress subnet(255, 255, 255, 0);

const char * path = "/index.html";

void setup() {
  Serial.begin(115200);
  initSDCard();
  initWiFi();
}

void loop() {
  server.handleClient();
  readFile("/index.html"); // replace with your file name
  delay(5000);
}

void initWiFi(){
  // Set up the Access Point
  File wifiSSID = SD.open("/wifi_ssid.txt");
  File wifiPassword = SD.open("/wifi_password.txt");
  if (!wifiSSID) {
    apSSID = "ESP-32 AP";
  }
  if (!wifiPassword) {
    apPassword = "admin12345";
  }

  String wifiSSIDName = "";
  while (wifiSSID.available()) {
    char data = wifiSSID.read();
    wifiSSIDName += data;
  }

  String wifiPass = "";
  while (wifiPassword.available()) {
    char data = wifiPassword.read();
    wifiPass += data;
  }

  const char* wifiSSIDChar = wifiSSIDName.c_str();
  const char* wifiPasswordChar = wifiPass.c_str();

  Serial.println("SSID Name: " + wifiSSIDName);
  Serial.println("SSID Password: " + wifiPass);

    WiFi.softAP(wifiSSIDChar, wifiPasswordChar);
    Serial.println("Access Point Started");
    Serial.println("IP Address: " + WiFi.softAPIP().toString());

    WiFi.softAPConfig(local_IP, subnet, gateway);

    server.on("/", HTTP_GET, handleRoot);
    server.onNotFound(handleFileRequest);

    server.begin();
    Serial.println("Server started.");

    server.on("/adminInfo", HTTP_GET, []() {
    // Perform an action or respond to the web request
    Serial.println("Button pressed from web interface!");
    server.send(200, "text/plain", "Button press received!");
  });
}

void initSDCard(){
  SPI.begin(18, 19, 23, chipSelect);
  if (!SD.begin(chipSelect)) {
    Serial.println("Card mount failed!");
    return;
  }
  Serial.println("Card mounted successfully.");
}

void handleRoot() {
  File file = SD.open(path);
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

  if (SD.exists(path)) {
    File file = SD.open(path, FILE_READ);
    if (file) {
      String contentType = "text/html"; // Default content type
      if (path.endsWith(".css")) {
    contentType = "text/css";
} else if (path.endsWith(".js")) {
    contentType = "application/javascript";
} else if (path.endsWith(".png")) {
    contentType = "image/png";
} else if (path.endsWith(".jpg")) {
    contentType = "image/jpeg";
} else if (path.endsWith(".wasm")) {
    contentType = "application/wasm";
} else if (path.endsWith(".svg")) {
    contentType = "image/svg+xml";
} else if (path.endsWith(".woff")) {
    contentType = "font/woff";
} else if (path.endsWith(".woff2")) {
    contentType = "font/woff2";
} else if (path.endsWith(".symbols")) {
    contentType = "application/x-symbols"; // Custom or generic MIME type
} else if (path.endsWith(".json")) {
    contentType = "application/json";
} else if (path.endsWith(".last_build_id")) {
    contentType = "text/plain"; // Generic MIME type for custom file types
} else if (path.endsWith(".otf")) {
    contentType = "font/otf";
} else if (path.endsWith(".bin")) {
    contentType = "application/octet-stream"; // Generic MIME type for binary files
} else if (path.endsWith(".frag")) {
    contentType = "text/x-glsl"; // MIME type for GLSL fragment shaders
}

      server.streamFile(file, contentType);
      file.close();
    } else {
      server.send(404, "text/plain", "File not found");
    }
  } else {
    server.send(404, "text/plain", "File not found");
  }
}

void readFile(const char * path) {
  File file = SD.open(path);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }
  Serial.println("File has been read! Hooorayyyy!");
  file.close();
}