#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

const char* apSSID = "RiX System v1.0";  // Name of the AP
const char* apPassword = "";

const int chipSelect = 5;

AsyncWebServer server(80);

const char * indexFilePath = "/index.html";

void setup() {
  Serial.begin(115200);
  initSDCard();
}

void loop() {
  readFile(); // replace with your file name
  delay(5000);

}

void initSDCard(){
  SPI.begin(18, 19, 23, chipSelect);
  if (!SD.begin(chipSelect)) {
    Serial.println("Card mount failed!");
    return;
  }
  Serial.println("SD card initialized.");
  uint8_t cardType = SD.cardType();
  if(cardType == CARD_NONE){
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");
  if(cardType == CARD_MMC){
    Serial.println("MMC");
  } else if(cardType == CARD_SD){
    Serial.println("SDSC");
  } else if(cardType == CARD_SDHC){
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
}

void initWIFI(){

}

void readFile() {
  File file = SD.open(indexFilePath);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.println("File Content:");
  while (file.available()) {
    Serial.write(file.read());
  }
  file.close();
}
