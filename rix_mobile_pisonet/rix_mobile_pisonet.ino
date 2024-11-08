#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include "SD.h"
#include "SPI.h"
#include "FS.h"
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Pins
const int CS_PIN = 5;
const int COIN_ACCEPTOR_PIN = 26;
const int PREVIOUS_BUTTON_PIN = 13;
const int NEXT_BUTTON_PIN = 12;
const int ENTER_BUTTON_PIN = 27;
const int VIEW_INFO_BUTTON_PIN = 14;
const int COIN_SLOT_RELAY_PIN = 33;

// Integer Value Holder
const int coinValue = 10;  // minutes
const int maxDevices = 15;
const int longPressDuration = 1500;

// Characters
char* errorCode = "OK";

// Longs
unsigned long buttonPressTime = 0;

bool isCoinInserted = false;
bool hasSDCard = false;
bool isViewingInfo = false;
bool isButtonPressed = false;
bool isLongPress = false;
bool isConnectedToWifi = false;
bool isAcceptingCoin = false;

int selectedDeviceNumber = 1;
int status = 0;
int insertedCoin = 0;
int viewingInfoStage = 0;

String wifi_SSID = "";
String wifi_Password = "";
String wifi_IP_Address = "";
String esp32_ID = "";

unsigned long previousMillis = 0;
const long interval = 1000;

void IRAM_ATTR isr() {
  isCoinInserted = true;
}

WebServer server(8080);
LiquidCrystal_I2C lcd(0x27, 16, 2);  // 16x2 LCD

void setLCDFirstColumn(String message) {
  lcd.setCursor(0, 0);
  if (message.length() > 16) {
    message = message.substring(0, 13) + "...";
  }
  lcd.print(message);
}

void setLCDSecondColumn(String message) {
  lcd.setCursor(0, 1);
  if (message.length() > 16) {
    message = message.substring(0, 13) + "...";
  }
  lcd.print(message);
}

void setLCDLongMessage(String message) {
  if (message.length() > 16) {
    String firstColumn = message.substring(0, 16);
    String secondColumn;
    if (message.length() > 32) {
      secondColumn = message.substring(16, 29) + "...";
    } else {
      secondColumn = message.substring(16, message.length());
    }
    lcd.setCursor(0, 0);
    lcd.print(firstColumn);
    lcd.setCursor(0, 1);
    lcd.print(secondColumn);
  } else {
    lcd.setCursor(0, 0);
    lcd.print(message);
  }
}

void longPressDetector() {
  int buttonState = digitalRead(VIEW_INFO_BUTTON_PIN);  // Assuming button is connected to ground

  // Button is pressed (LOW because of pull-up)
  if (buttonState == LOW) {
    if (!isButtonPressed) {
      // Button is initially pressed
      isButtonPressed = true;
      buttonPressTime = millis();  // Record the time the button was pressed
    } else {
      // If the button is held, check for long press
      if ((millis() - buttonPressTime > longPressDuration) && !isLongPress) {
        isLongPress = true;
        if (isViewingInfo) {
          isViewingInfo = false;
          viewingInfoStage = 0;
          lcd.clear();
          setLCDFirstColumn("PRESS ENTER TO");
          setLCDSecondColumn("SELECT DEVICE");
        } else {
          isViewingInfo = true;
          if (viewingInfoStage == 0) {
            lcd.clear();
            setLCDFirstColumn("WiFi SSDI:");
            setLCDSecondColumn(wifi_SSID);
            viewingInfoStage = 1;
          }
        }
      }
    }
  } else {
    // Button is released
    if (isButtonPressed) {
      if (!isLongPress) {
        if (isViewingInfo) {
          if (viewingInfoStage == 1) {
            lcd.clear();
            setLCDFirstColumn("WiFi Pass:");
            setLCDSecondColumn(wifi_Password);
            viewingInfoStage = 2;
          } else if (viewingInfoStage == 2) {
            lcd.clear();
            setLCDFirstColumn("IP Address:");
            setLCDSecondColumn(wifi_IP_Address);
            viewingInfoStage = 3;
          } else if (viewingInfoStage == 3) {
            lcd.clear();
            setLCDFirstColumn("ESP32 Id:");
            setLCDSecondColumn(esp32_ID);
            viewingInfoStage = 4;
          } else if (viewingInfoStage == 4) {
            lcd.clear();
            setLCDFirstColumn("WiFi Status:");
            if (isConnectedToWifi) {
              setLCDSecondColumn("Connected");
            } else {
              setLCDSecondColumn("Disconnected");
            }
            viewingInfoStage = 0;
          } else if (viewingInfoStage == 0) {
            lcd.clear();
            setLCDFirstColumn("WiFi SSDI:");
            setLCDSecondColumn(wifi_SSID);
          }
        }
      }
      // Reset states
      isButtonPressed = false;
      isLongPress = false;
    }
  }
}

void createDataFile() {
  StaticJsonDocument<200> doc;
  doc["wifiSSID"] = "";
  doc["wifiPassword"] = "";
  doc["ipAddress"] = "192.168.1.254";
  doc["deviceId"] = "rixExample001";

  char jsonBuffer[200];
  serializeJson(doc, jsonBuffer);
  File file = SD.open("/data.txt", FILE_WRITE);
  if (file) {
    file.println(jsonBuffer);
    file.close();
    Serial.println("Data file successfully created");
  } else {
    errorCode = "FILE-001";
    lcd.clear();
    setLCDFirstColumn("ERROR:");
    setLCDSecondColumn(errorCode);
    Serial.println("Failed to create data file");
  }
}

void readDataFile() {
  File file = SD.open("/data.txt");
  if (!file) {
    errorCode = "CARD-002";
    lcd.clear();
    setLCDFirstColumn("ERROR:");
    setLCDSecondColumn(errorCode);
    Serial.println("Data file does not exist or failed to open.");
    return;
  }

  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, file);

  file.close();

  if (error) {
    errorCode = "CARD-003";
    Serial.println("Deserialization failed!");
    lcd.clear();
    setLCDFirstColumn("ERROR:");
    setLCDSecondColumn(errorCode);
    Serial.println(error.f_str());
    isConnectedToWifi = false;
    return;
  }

  if (doc["wifiSSID"].isNull()) {
    Serial.println("Wifi SSID key is missing");
    errorCode = "SSID-MSNG";
    lcd.clear();
    setLCDFirstColumn("ERROR:");
    setLCDSecondColumn(errorCode);
    isConnectedToWifi = false;
    return;
  }

  if (doc["wifiPassword"].isNull()) {
    Serial.println("Wifi Password key is missing");
    errorCode = "PSWD-MSNG";
    lcd.clear();
    setLCDFirstColumn("ERROR:");
    setLCDSecondColumn(errorCode);
    isConnectedToWifi = false;
    return;
  }

  if (doc["ipAddress"].isNull()) {
    Serial.println("Wifi IP address key is missing");
    errorCode = "ADDRS-MSNG";
    lcd.clear();
    setLCDFirstColumn("ERROR:");
    setLCDSecondColumn(errorCode);
    isConnectedToWifi = false;
    return;
  }

  if (doc["esp32ID"].isNull()) {
    Serial.println("ESP32 ID key is missing");
    errorCode = "ID-MSNG";
    lcd.clear();
    setLCDFirstColumn("ERROR:");
    setLCDSecondColumn(errorCode);
    isConnectedToWifi = false;
    return;
  }

  const char* wifiSSID = doc["wifiSSID"];
  const char* wifiPassword = doc["wifiPassword"];
  const char* ipAddress = doc["ipAddress"];
  const char* esp32Id = doc["esp32ID"];

  wifi_SSID = String(wifiSSID);
  wifi_Password = String(wifiPassword);
  wifi_IP_Address = String(ipAddress);
  esp32_ID = String(esp32Id);

  Serial.print("Wifi SSID: ");
  Serial.println(wifiSSID);
  Serial.print("Wifi Password: ");
  Serial.println(wifiPassword);
  Serial.print("Device IP Address: ");
  Serial.println(ipAddress);
  Serial.print("ESP32 ID: ");
  Serial.println(esp32Id);
  initializeDevice(wifiSSID, wifiPassword, ipAddress, esp32Id);
}

void initializeDevice(const char* WifiSSID, const char* WifiPassword, const char* WifiIPAddress, const char* esp32Id) {
  if (WifiSSID[0] == '\0') {
    Serial.println("Wifi SSID is empty or invalid.");
    errorCode = "INVLD-SSID";
    lcd.clear();
    setLCDFirstColumn("ERROR:");
    setLCDSecondColumn(errorCode);
    isConnectedToWifi = false;
    return;
  }
  if (WifiPassword[0] == '\0') {
    Serial.println("Wifi password is empty or invalid.");
    errorCode = "INVLD-PSWD";
    lcd.clear();
    setLCDFirstColumn("ERROR:");
    setLCDSecondColumn(errorCode);
    isConnectedToWifi = false;
    return;
  }
  if (WifiIPAddress[0] == '\0') {
    Serial.println("Wifi IP address is empty or invalid.");
    errorCode = "INVLD-ADDRS";
    lcd.clear();
    setLCDFirstColumn("ERROR:");
    setLCDSecondColumn(errorCode);
    isConnectedToWifi = false;
    return;
  }
  if (esp32Id[0] == '\0') {
    Serial.println("ESP32 ID is empty or invalid.");
    errorCode = "INVLD-ID";
    lcd.clear();
    setLCDFirstColumn("ERROR:");
    setLCDSecondColumn(errorCode);
    isConnectedToWifi = false;
    return;
  }

  IPAddress ip;
  if (ip.fromString(WifiIPAddress)) {
    Serial.println("Successfully converted the ip address.");
  } else {
    errorCode = "CONVRT-FAIL";
    lcd.clear();
    setLCDFirstColumn("ERROR:");
    setLCDSecondColumn(errorCode);
    Serial.println("Failed to convert the ip address.");
    isConnectedToWifi = false;
    return;
  }

  if (!WiFi.config(ip)) {
    errorCode = "STA-FAIL";
    lcd.clear();
    setLCDFirstColumn("ERROR:");
    setLCDSecondColumn(errorCode);
    Serial.println("STA failed!");
    isConnectedToWifi = false;
    return;
  }

  WiFi.begin(WifiSSID, WifiPassword);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting...");
    lcd.clear();
    setLCDFirstColumn("Connecting to");
    setLCDSecondColumn("WiFI...");
    isConnectedToWifi = false;
    delay(1000);
  }

  isConnectedToWifi = true;
  Serial.println("Wi-Fi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("***PLEASE PRESS ENTER***");
  lcd.clear();
  setLCDFirstColumn("PRESS ENTER TO");
  setLCDSecondColumn("SELECT DEVICE");
}

void sendDataToAndroid(const char* path, const char* data) {
  server.on(path, HTTP_GET, []() {
    server.send(200, "text/plain", "Hello from ESP32");
  });
}

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();
  setLCDFirstColumn("Please wait...");
  attachInterrupt(COIN_ACCEPTOR_PIN, isr, FALLING);

  pinMode(PREVIOUS_BUTTON_PIN, INPUT_PULLUP);
  pinMode(NEXT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(ENTER_BUTTON_PIN, INPUT_PULLUP);
  pinMode(VIEW_INFO_BUTTON_PIN, INPUT_PULLUP);
  pinMode(COIN_SLOT_RELAY_PIN, OUTPUT);

  digitalWrite(COIN_SLOT_RELAY_PIN, HIGH);

  if (!SD.begin(CS_PIN)) {
    Serial.println("Card Mount Failed");
    hasSDCard = false;
    lcd.clear();
    setLCDFirstColumn("Card mount");
    setLCDSecondColumn("Failed!");
    return;
  }

  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    errorCode = "CARD-001";
    lcd.clear();
    setLCDFirstColumn("ERROR:");
    setLCDSecondColumn(errorCode);
    hasSDCard = false;
    return;
  }

  Serial.println("SD card initialized successfully");
  lcd.clear();
  setLCDFirstColumn("SD CARD: OK!");
  hasSDCard = true;
  if (SD.exists("/data.txt")) {
    readDataFile();
  } else {
    createDataFile();
  }

  server.begin();

  //findI2CAddress();
}

void loop() {
  server.handleClient();
  longPressDetector();
  if (isAcceptingCoin) {
    if (isCoinInserted) {
      isCoinInserted = false;
      Serial.println("Coin is inserted!");
      if (status == 1) {
        insertedCoin++;
        String selectedDevice = String(selectedDeviceNumber);
        String insertedCoinInString = String(insertedCoin);
        lcd.clear();
        setLCDFirstColumn("Device NO." + selectedDevice);
        setLCDSecondColumn("COIN: P" + insertedCoinInString);
      }
    }
  }

  if (!isViewingInfo) {
    if (digitalRead(PREVIOUS_BUTTON_PIN) == LOW) {
      if (status == 1) {
        if (selectedDeviceNumber != 1) {
          selectedDeviceNumber--;
          Serial.print("Selected device: ");
          Serial.println(selectedDeviceNumber);
          String selectedDevice = String(selectedDeviceNumber);
          String insertedCoinInString = String(insertedCoin);
          lcd.clear();
          setLCDFirstColumn("Device NO." + selectedDevice);
          setLCDSecondColumn("COIN: P" + insertedCoinInString);
        }
      }
      delay(300);
    }

    if (digitalRead(NEXT_BUTTON_PIN) == LOW) {
      if (status == 1) {
        if (selectedDeviceNumber != maxDevices) {
          selectedDeviceNumber++;
          Serial.print("Selected device: ");
          Serial.println(selectedDeviceNumber);
          String selectedDevice = String(selectedDeviceNumber);
          String insertedCoinInString = String(insertedCoin);
          lcd.clear();
          setLCDFirstColumn("Device NO." + selectedDevice);
          setLCDSecondColumn("COIN: P" + insertedCoinInString);
        }
      }
      delay(300);
    }

    if (digitalRead(ENTER_BUTTON_PIN) == LOW) {
      if (status == 0) {
        status = 1;
        String selectedDevice = String(selectedDeviceNumber);
        digitalWrite(COIN_SLOT_RELAY_PIN, HIGH);
        isAcceptingCoin = false;
        lcd.clear();
        setLCDFirstColumn("Please wait...");
        delay(500);
        lcd.clear();
        setLCDFirstColumn("Device NO." + selectedDevice);
        setLCDSecondColumn("COIN: P0");
        isAcceptingCoin = true;
      } else if (status == 1) {
        // Send the data to android app.
        String selectedDevice = String(selectedDeviceNumber);
        String insertedCoinInString = String(insertedCoin);
        lcd.clear();
        setLCDFirstColumn("Crediting...");
        setLCDSecondColumn("NO." + selectedDevice + " (P" + insertedCoinInString + ")");
        if (insertedCoin == 0) {
          lcd.clear();
          setLCDFirstColumn("PRESS ENTER TO");
          setLCDSecondColumn("SELECT DEVICE");
          digitalWrite(COIN_SLOT_RELAY_PIN, LOW);
          status = 0;
          insertedCoin = 0;
          isAcceptingCoin = false;
        } else {
          delay(3000);
          lcd.clear();
          setLCDFirstColumn("SUCCESSFUL!");
          delay(1500);
          lcd.clear();
          setLCDFirstColumn("PRESS ENTER TO");
          setLCDSecondColumn("SELECT DEVICE");
          digitalWrite(COIN_SLOT_RELAY_PIN, LOW);
          status = 0;
          insertedCoin = 0;
          isAcceptingCoin = false;
        }
      }
      delay(400);
    }
  }
}
