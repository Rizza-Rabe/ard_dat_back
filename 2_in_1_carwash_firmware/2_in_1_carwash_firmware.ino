#include <TM1637Display.h>
#include <EEPROM.h>

// Pins
int decreaseValuePin = A0;
int modeSelectionPin = A1;
int increaseValuePin = A2;
int resetSettingsPin = A3;
int waterButtonPin = A4;
int soapButtonPin = A5;
int endTimeButtonPin = A6;
int coinAcceptorPin = 2;
int tm1637_CLK_Pin = 3;
int tm1637_DIO_Pin = 4;
int buzzerPin = 5;
int coinAcceptorSwitchPin = 6;
int soapRelayPin = 7;
int waterRelayPin = 8;

// EEPROM Addresses
int waterAddress = 0;
int soapAddress = 2;
int pauseResumeAddress = 4;
int smartSwitchingAddress = 6;

// Values
int waterPulseValue = 0;
int soapPulseValue = 0;
int pauseResumeValue = 0;
int smartSwitchingValue = 0;

// Device boot
bool isBooting = true;

// Mode selection button
bool isModeSelectionButtonPresed;
unsigned long modeSelectionButtonPressStartTime = 0;
bool isModeSelectionButtonLongPressDetected = false;

// Water button
bool isWaterButtonPressed;
unsigned long waterButtonPressStartTime = 0;
bool isWaterButtonLongPressDetected = false;

// Soap button
bool isSoapButtonPressed;
unsigned long soapButtonPressStartTime = 0;
bool isSoapButtonLongPressDetected = false;

// Reset settings button
bool resetSettingsButtonPressed;
unsigned long resetSettingsButtonPressStartTime = 0;
bool resetSettingsButtonLongPressDetected = false;

// End time button
bool endTimeButtonPressed;
unsigned long endTimeButtonPressStartTime = 0;
bool endTimeButtonLongPressDetected = false;

const uint8_t SEGMENT_F = 0b01110001;  // F = Water Value Edit Mode
const uint8_t SEGMENT_E = 0b01111001;  // E = Soap Value Edit Mode
const uint8_t SEGMENT_A = 0b01110111;  // A = Blower value Edt Mode
const uint8_t SEGMENT_L = 0b00111000;  // L = Vacuum value Edit Mode

const uint8_t SEGMENT_P = 0b01110011;  // P = Peso Sign
const uint8_t SEGMENT_H = 0b01110110;  // H = Pause/Resume turn ON and OFF. (0 is OFF | 1 is ON)
const uint8_t SEGMENT_C = 0b00111001;  // C = Smart Switching turn ON and OFF (0 is OFF | 1 is ON)

int selectedMode = 1;
int maxModeSelection = 4;

unsigned long secondsPreviousMillis = 0;
unsigned long tm1637BlinkerPreviousMillis = 0;
unsigned long coinPulseLasDebounceTime = 0;

bool isTM1637TurnedON = false;
bool isValueEditing = false;
bool isCoinInserted = false;
bool isColonActive = false;
bool isTimerRunning = false;
bool isWaterPaused = false;
bool isSoapPaused = false;
bool isBuzzerTurnedON = false;
bool isModeSelecting = false;

int totalSeconds = 0;
int totalCoinsInserted = 0;
int totalSecondsUsed = 0;
String userSelectedMode = "none";

TM1637Display display = TM1637Display(tm1637_CLK_Pin, tm1637_DIO_Pin);

void writeToEEPROM(int address, int addressValue) {
  EEPROM.update(address, addressValue);
  delay(50);
  if (address == 0) {
    int value = 0;
    EEPROM.get(0, value);
    if (value != waterPulseValue) {
      // Force update
      EEPROM.put(address, addressValue);
      delay(50);
      Serial.println("Forced update the address 0");
    }
  } else if (address == 2) {
    int value = 0;
    EEPROM.get(address, value);
    if (value != soapPulseValue) {
      EEPROM.put(address, addressValue);
      delay(50);
      Serial.println("Forced update the address 2");
    }
  } else if (address == 4) {
    int value = 0;
    EEPROM.get(address, value);
    if (value != pauseResumeValue) {
      EEPROM.put(address, addressValue);
      delay(50);
      Serial.println("Forced update the address 4");
    }
  } else if (address == 6) {
    int value = 0;
    EEPROM.get(address, value);
    if (value != smartSwitchingValue) {
      EEPROM.put(address, addressValue);
      delay(50);
      Serial.println("Forced update the address 6");
    }
  }
}

int readFromEEPROM(int address) {
  int eepromValue = 0;
  EEPROM.get(address, eepromValue);
  return eepromValue;
}

void buttonListeners() {

  // MODE SELECTION BUTTON
  bool modeSelectionButtonState = analogRead(modeSelectionPin) < 500;
  if (modeSelectionButtonState && !isModeSelectionButtonPresed) {
    // Button was just pressed
    modeSelectionButtonPressStartTime = millis();
    isModeSelectionButtonPresed = true;
    isModeSelectionButtonLongPressDetected = false;
  } else if (!modeSelectionButtonState && isModeSelectionButtonPresed && !isTimerRunning) {
    unsigned long modeSelectionSinglePressDuration = millis() - modeSelectionButtonPressStartTime;
    if (modeSelectionSinglePressDuration < 1000) {
      Serial.println("Short press detected");
      if (!isValueEditing) {
        if (isModeSelecting) {
          Serial.println("Exited mode selection");
          display.setSegments(&SEGMENT_P, 1, 0);
          display.showNumberDec(totalCoinsInserted, false, 3, 1);
          digitalWrite(coinAcceptorSwitchPin, HIGH);
          isModeSelecting = false;
          delay(55);
          isCoinInserted = false;
        } else {
          Serial.println("Entered mode selection");
          display.setSegments(&SEGMENT_F, 1, 0);
          display.showNumberDec(waterPulseValue, false, 3, 1);
          digitalWrite(coinAcceptorSwitchPin, LOW);
          isModeSelecting = true;
        }
      }
    }
    isModeSelectionButtonPresed = false;
  }

  if (isModeSelectionButtonPresed) {
    unsigned long modeSelectionLongPressDuration = millis() - modeSelectionButtonPressStartTime;
    if (modeSelectionLongPressDuration >= 1000) {  // 1000 is 1 second
      if (!isModeSelectionButtonLongPressDetected) {
        if (isModeSelecting) {
          Serial.println("Mode selection long press");
          if (isValueEditing) {
            Serial.println("Exited value edit mode");
            if (selectedMode == 1) {
              display.setSegments(&SEGMENT_F, 1, 0);
              display.showNumberDec(waterPulseValue, false, 3, 1);
              writeToEEPROM(waterAddress, waterPulseValue);
            } else if (selectedMode == 2) {
              display.setSegments(&SEGMENT_E, 1, 0);
              display.showNumberDec(soapPulseValue, false, 3, 1);
              writeToEEPROM(soapAddress, soapPulseValue);
            } else if (selectedMode == 3) {
              display.setSegments(&SEGMENT_H, 1, 0);
              display.showNumberDec(pauseResumeValue, false, 3, 1);
              writeToEEPROM(pauseResumeAddress, pauseResumeValue);
            } else if (selectedMode == 4) {
              display.setSegments(&SEGMENT_C, 1, 0);
              display.showNumberDec(smartSwitchingValue, false, 3, 1);
              writeToEEPROM(smartSwitchingAddress, smartSwitchingValue);
            }
            digitalWrite(buzzerPin, HIGH);
            delay(350);
            digitalWrite(buzzerPin, LOW);
            isValueEditing = false;
          } else {
            digitalWrite(buzzerPin, HIGH);
            delay(70);
            digitalWrite(buzzerPin, LOW);
            delay(70);
            digitalWrite(buzzerPin, HIGH);
            delay(70);
            digitalWrite(buzzerPin, LOW);
            Serial.println("Entered value edit mode!");
            isValueEditing = true;
          }
          isModeSelectionButtonLongPressDetected = true;
        }
      }
    }
  }

  // RESET BUTTON
  bool resetSettingsButtonState = analogRead(resetSettingsPin) < 500;
  if (resetSettingsButtonState && !resetSettingsButtonPressed) {
    resetSettingsButtonPressStartTime = millis();
    resetSettingsButtonPressed = true;
    resetSettingsButtonLongPressDetected = false;
  } else if (!resetSettingsButtonState && resetSettingsButtonPressed) {
    resetSettingsButtonPressed = false;
  }

  if (resetSettingsButtonPressed && !isTimerRunning) {
    unsigned long resetSettingsLongPressDuration = millis() - resetSettingsButtonPressStartTime;
    if (resetSettingsLongPressDuration >= 1000) {
      if (!resetSettingsButtonLongPressDetected) {
        Serial.println("Reset settings long press");
        resetSettings();
        resetSettingsButtonLongPressDetected = true;
      }
    }
  }

  if (analogRead(increaseValuePin) < 500) {
    if (isModeSelecting && !isValueEditing && !isTimerRunning) {
      if (selectedMode != maxModeSelection) {
        selectedMode++;
        if (selectedMode == 1) {
          display.setSegments(&SEGMENT_F, 1, 0);
          display.showNumberDec(waterPulseValue, false, 3, 1);
        } else if (selectedMode == 2) {
          display.setSegments(&SEGMENT_E, 1, 0);
          display.showNumberDec(soapPulseValue, false, 3, 1);
        } else if (selectedMode == 3) {
          display.setSegments(&SEGMENT_H, 1, 0);
          display.showNumberDec(pauseResumeValue, false, 3, 1);
        } else if (selectedMode == 4) {
          display.setSegments(&SEGMENT_C, 1, 0);
          display.showNumberDec(smartSwitchingValue, false, 3, 1);
        }
      }
    } else {
      if (selectedMode == 1 && isModeSelecting && !isTimerRunning) {
        if (waterPulseValue != 960) {
          waterPulseValue++;
        }
        display.setSegments(&SEGMENT_F, 1, 0);
        display.showNumberDec(waterPulseValue, false, 3, 1);
      } else if (selectedMode == 2 && isModeSelecting && !isTimerRunning) {
        if (soapPulseValue != 960) {
          soapPulseValue++;
        }
        display.setSegments(&SEGMENT_E, 1, 0);
        display.showNumberDec(soapPulseValue, false, 3, 1);
      } else if (selectedMode == 3 && isModeSelecting && !isTimerRunning) {
        if (pauseResumeValue != 1) {
          pauseResumeValue++;
        }
        display.setSegments(&SEGMENT_H, 1, 0);
        display.showNumberDec(pauseResumeValue, false, 3, 1);
      } else if (selectedMode == 4 && isModeSelecting && !isTimerRunning) {
        if (smartSwitchingValue != 1) {
          smartSwitchingValue++;
        }
        display.setSegments(&SEGMENT_C, 1, 0);
        display.showNumberDec(smartSwitchingValue, false, 3, 1);
      }
    }
    delay(270);
  }

  if (analogRead(decreaseValuePin) < 500) {
    if (isModeSelecting && !isValueEditing && !isTimerRunning) {
      if (selectedMode != 1) {
        selectedMode--;
        if (selectedMode == 1) {
          display.setSegments(&SEGMENT_F, 1, 0);
          display.showNumberDec(waterPulseValue, false, 3, 1);
        } else if (selectedMode == 2) {
          display.setSegments(&SEGMENT_E, 1, 0);
          display.showNumberDec(soapPulseValue, false, 3, 1);
        } else if (selectedMode == 3) {
          display.setSegments(&SEGMENT_H, 1, 0);
          display.showNumberDec(pauseResumeValue, false, 3, 1);
        } else if (selectedMode == 4) {
          display.setSegments(&SEGMENT_C, 1, 0);
          display.showNumberDec(smartSwitchingValue, false, 3, 1);
        }
      }
    } else {
      if (selectedMode == 1 && isModeSelecting && !isTimerRunning) {
        if (waterPulseValue != 5) {
          waterPulseValue--;
        }
        display.setSegments(&SEGMENT_F, 1, 0);
        display.showNumberDec(waterPulseValue, false, 3, 1);
      } else if (selectedMode == 2) {
        if (soapPulseValue != 0) {
          soapPulseValue--;
        }
        display.setSegments(&SEGMENT_E, 1, 0);
        display.showNumberDec(soapPulseValue, false, 3, 1);
      } else if (selectedMode == 3 && isModeSelecting && !isTimerRunning) {
        if (pauseResumeValue != 0) {
          pauseResumeValue--;
        }
        display.setSegments(&SEGMENT_H, 1, 0);
        display.showNumberDec(pauseResumeValue, false, 3, 1);
      } else if (selectedMode == 4 && isModeSelecting && !isTimerRunning) {
        if (smartSwitchingValue != 0) {
          smartSwitchingValue--;
        }
        display.setSegments(&SEGMENT_C, 1, 0);
        display.showNumberDec(smartSwitchingValue, false, 3, 1);
      }
    }
    delay(270);
  }

  // PURE ANALOG PIN. READING IS BETWEEN 500 to 530. NEARLY 0 when connected
  // END TIME BUTTON LONG PRESS
  // bool endTimeButtonState = analogRead(endTimeButtonPin) < 50;
  // if (endTimeButtonState && !endTimeButtonPressed) {
  //   endTimeButtonPressStartTime = millis();
  //   endTimeButtonPressed = true;
  //   endTimeButtonLongPressDetected = false;
  // } else if (!endTimeButtonState && endTimeButtonPressed) {
  //   endTimeButtonPressed = false;
  // }

  // if (endTimeButtonPressed && totalSeconds > 0) {
  //   unsigned long endTimeLongPressDuration = millis() - endTimeButtonPressStartTime;
  //   if (endTimeLongPressDuration >= 1000) {
  //     if (!endTimeButtonLongPressDetected) {
  //       Serial.println("End Time long pressed!");
  //       endTimeButtonLongPressDetected = true;
  //     }
  //   }
  // }

  // END TIME BUTTON SHORT PRESS
  if (analogRead(endTimeButtonPin) < 50) {
    if (totalSeconds > 5) {
      digitalWrite(buzzerPin, HIGH);
      digitalWrite(waterRelayPin, LOW);
      digitalWrite(soapRelayPin, LOW);
      display.setSegments(&SEGMENT_P, 1, 0);
      display.showNumberDec(0, false, 3, 1);
      digitalWrite(coinAcceptorSwitchPin, HIGH);
      delay(350);

      isCoinInserted = false;
      totalSeconds = 0;
      totalCoinsInserted = 0;
      isTimerRunning = false;
      isWaterPaused = false;
      isSoapPaused = false;
      userSelectedMode = "none";
      delay(500);
      digitalWrite(buzzerPin, LOW);
      delay(300);
    }
  }

  // Water button long and short press
  bool waterButtonState = analogRead(waterButtonPin) < 500;
  if (waterButtonState && !isWaterButtonPressed) {
    // Button was just pressed
    waterButtonPressStartTime = millis();
    isWaterButtonPressed = true;
    isWaterButtonLongPressDetected = false;
  } else if (!waterButtonState && isWaterButtonPressed) {
    unsigned long waterButtonShortPress = millis() - waterButtonPressStartTime;
    if (waterButtonShortPress < 1000) {
      Serial.println("Water short pressed!");
      if (!isTimerRunning && totalCoinsInserted > 0) {
        totalSeconds = (totalCoinsInserted * waterPulseValue);
        isTimerRunning = true;
        userSelectedMode = "water";
        digitalWrite(coinAcceptorSwitchPin, LOW);
        Serial.println("Turn ON the water Relay");
        digitalWrite(waterRelayPin, HIGH);
      } else {
        if (pauseResumeValue == 1 && totalSeconds > 5) {
          if (userSelectedMode == "water") {
            if (isWaterPaused) {
              digitalWrite(waterRelayPin, HIGH);
              isWaterPaused = false;
            } else {
              digitalWrite(waterRelayPin, LOW);
              Serial.println("Pause the water!");
              isWaterPaused = true;
            }
          } else {
            int calculatedNewSeconds = (waterPulseValue * totalCoinsInserted - totalSecondsUsed);
            Serial.print("Calculated total seconds in water: ");
            Serial.println(calculatedNewSeconds);
            if (calculatedNewSeconds >= 5) {
              totalSeconds = calculatedNewSeconds;
              if (smartSwitchingValue == 1 && isSoapPaused) {
                Serial.println("Water has been pressed in pause mode. Swicth to water mode!");
                display.clear();
                delay(50);
                display.showNumberDec(3, false, 3, 1);
                delay(950);
                display.clear();
                delay(50);
                display.showNumberDec(2, false, 3, 1);
                delay(950);
                display.clear();
                delay(50);
                display.showNumberDec(1, false, 3, 1);
                delay(1000);
                userSelectedMode = "water";
                isSoapPaused = false;
                isWaterPaused = false;
                digitalWrite(soapRelayPin, LOW);
                digitalWrite(waterRelayPin, HIGH);
              }
            }
          }
        }
      }
      delay(270);
    }
    isWaterButtonPressed = false;
  }

  if (isWaterButtonPressed) {
    unsigned long waterButtonLongPressPress = millis() - waterButtonPressStartTime;
    if (waterButtonLongPressPress >= 2000) {
      if (!isWaterButtonLongPressDetected) {
        Serial.println("Water long pressed!");
        if (totalSeconds <= 5) {
          isWaterButtonLongPressDetected = true;
          return;
        }
        digitalWrite(buzzerPin, HIGH);
        digitalWrite(waterRelayPin, LOW);
        digitalWrite(soapRelayPin, LOW);
        display.setSegments(&SEGMENT_P, 1, 0);
        display.showNumberDec(0, false, 3, 1);
        digitalWrite(coinAcceptorSwitchPin, HIGH);
        delay(350);

        isCoinInserted = false;
        totalSeconds = 0;
        totalCoinsInserted = 0;
        isTimerRunning = false;
        isWaterPaused = false;
        isSoapPaused = false;
        userSelectedMode = "none";
        delay(500);
        digitalWrite(buzzerPin, LOW);
        isWaterButtonLongPressDetected = true;
      }
    }
  }

  // Soap button long and short press
  bool soapButtonState = analogRead(soapButtonPin) < 500;
  if (soapButtonState && !isSoapButtonPressed) {
    // Button was just pressed
    soapButtonPressStartTime = millis();
    isSoapButtonPressed = true;
    isSoapButtonLongPressDetected = false;
  } else if (!soapButtonState && isSoapButtonPressed) {
    unsigned long soapButtonShortPress = millis() - soapButtonPressStartTime;
    if (soapButtonShortPress < 1000) {
      Serial.println("Soap short pressed!");
      if (!isTimerRunning && totalCoinsInserted > 0) {
        totalSeconds = (totalCoinsInserted * soapPulseValue);
        isTimerRunning = true;
        userSelectedMode = "soap";
        digitalWrite(coinAcceptorSwitchPin, LOW);
        Serial.println("Turn ON the soap Relay");
        digitalWrite(soapRelayPin, HIGH);
      } else {
        if (pauseResumeValue == 1 && totalSeconds > 5) {
          if (userSelectedMode == "soap") {
            if (isSoapPaused) {
              digitalWrite(soapRelayPin, HIGH);
              isSoapPaused = false;
            } else {
              Serial.println("Pause the soap!");
              digitalWrite(soapRelayPin, LOW);
              isSoapPaused = true;
            }
          } else {
            int calculatedNewSeconds = (soapPulseValue * totalCoinsInserted - totalSecondsUsed);
            Serial.print("Calculated total seconds in soap: ");
            Serial.println(calculatedNewSeconds);
            if (calculatedNewSeconds >= 5) {
              totalSeconds = calculatedNewSeconds;
              if (smartSwitchingValue == 1 && isWaterPaused) {
                Serial.println("Soap has been pressed in pause mode. Swicth to soap mode!");
                totalSeconds = (soapPulseValue * totalCoinsInserted - totalSecondsUsed);
                Serial.print("Calculated total seconds in soap: ");
                Serial.println(totalSeconds);
                if (totalSeconds > 0) {
                  display.clear();
                  delay(50);
                  display.showNumberDec(3, false, 3, 1);
                  delay(950);
                  display.clear();
                  delay(50);
                  display.showNumberDec(2, false, 3, 1);
                  delay(950);
                  display.clear();
                  delay(50);
                  display.showNumberDec(1, false, 3, 1);
                  delay(1000);
                  userSelectedMode = "soap";
                  isSoapPaused = false;
                  isWaterPaused = false;
                  digitalWrite(soapRelayPin, HIGH);
                  digitalWrite(waterRelayPin, LOW);
                }
              }
            }
          }
        }
      }
      delay(270);
    }
    isSoapButtonPressed = false;
  }

  if (isSoapButtonPressed) {
    unsigned long soapButtonLongPressPress = millis() - soapButtonPressStartTime;
    if (soapButtonLongPressPress >= 2000) {
      if (!isSoapButtonLongPressDetected) {
        Serial.println("Soap long pressed!");
        if (totalSeconds <= 5) {
          isSoapButtonLongPressDetected = true;
          return;
        }
        digitalWrite(buzzerPin, HIGH);
        digitalWrite(waterRelayPin, LOW);
        digitalWrite(soapRelayPin, LOW);
        display.setSegments(&SEGMENT_P, 1, 0);
        display.showNumberDec(0, false, 3, 1);
        digitalWrite(coinAcceptorSwitchPin, HIGH);
        delay(350);

        isCoinInserted = false;
        totalSeconds = 0;
        totalCoinsInserted = 0;
        isTimerRunning = false;
        isWaterPaused = false;
        isSoapPaused = false;
        userSelectedMode = "none";
        delay(500);
        digitalWrite(buzzerPin, LOW);
        isSoapButtonLongPressDetected = true;
      }
    }
  }
}

void resetSettings() {
  display.showNumberDecEx(3333, 0x00, false, 4, 0);
  if (waterPulseValue != 60) {
    writeToEEPROM(waterAddress, 60);
    waterPulseValue = 60;
  }
  if (soapPulseValue != 40) {
    writeToEEPROM(soapAddress, 40);
    soapPulseValue = 40;
  }

  digitalWrite(buzzerPin, HIGH);
  delay(500);
  display.showNumberDecEx(2222, 0x00, false, 4, 0);
  delay(500);
  display.showNumberDecEx(1111, 0x00, false, 4, 0);
  delay(500);
  digitalWrite(buzzerPin, LOW);
  display.showNumberDecEx(0, 0x00, false, 4, 0);
  Serial.println("Reset settings successful");
}

void initializeTime() {
  if (isTimerRunning && totalCoinsInserted > 0) {
    unsigned long secondsCurrentMillis = millis();
    if (secondsCurrentMillis - secondsPreviousMillis >= 1000) {
      secondsPreviousMillis = secondsCurrentMillis;
      // Code here to be executed every 1 second.
      if (!isWaterPaused && !isSoapPaused) {
        if (totalSeconds > 0) {
          totalSeconds--;
          totalSecondsUsed++;
          calculateTime(totalSeconds);
          isCoinInserted = false;
          if (totalSeconds <= 10) {
            if (isBuzzerTurnedON) {
              digitalWrite(buzzerPin, LOW);
              isBuzzerTurnedON = false;
            } else {
              digitalWrite(buzzerPin, HIGH);
              isBuzzerTurnedON = true;
            }
          }
        } else {
          digitalWrite(buzzerPin, LOW);
          digitalWrite(waterRelayPin, LOW);
          digitalWrite(soapRelayPin, LOW);
          display.setSegments(&SEGMENT_P, 1, 0);
          display.showNumberDec(0, false, 3, 1);
          digitalWrite(coinAcceptorSwitchPin, HIGH);
          delay(350);

          isCoinInserted = false;
          totalSeconds = 0;
          totalCoinsInserted = 0;
          isTimerRunning = false;
          isWaterPaused = false;
          isSoapPaused = false;
          userSelectedMode = "none";
        }
      } else {
        digitalWrite(buzzerPin, LOW);
        blinkDisplayInPaused();
      }
    }
  }


  // TM1637 Blinker
  if (isValueEditing) {
    unsigned long blinkerCurrentMillis = millis();
    if (blinkerCurrentMillis - tm1637BlinkerPreviousMillis >= 300) {
      tm1637BlinkerPreviousMillis = blinkerCurrentMillis;
      if (isTM1637TurnedON) {
        display.clear();
        isTM1637TurnedON = false;
      } else {
        if (selectedMode == 1) {
          display.setSegments(&SEGMENT_F, 1, 0);
          display.showNumberDec(waterPulseValue, false, 3, 1);
        } else if (selectedMode == 2) {
          display.setSegments(&SEGMENT_E, 1, 0);
          display.showNumberDec(soapPulseValue, false, 3, 1);
        } else if (selectedMode == 3) {
          display.setSegments(&SEGMENT_H, 1, 0);
          display.showNumberDec(pauseResumeValue, false, 3, 1);
        } else if (selectedMode == 4) {
          display.setSegments(&SEGMENT_C, 1, 0);
          display.showNumberDec(smartSwitchingValue, false, 3, 1);
        }
        isTM1637TurnedON = true;
      }
    }
  }
}

void calculateTime(int inputSeconds) {
  if (inputSeconds >= 3600) {
    int hours = inputSeconds / 3600;
    int minutes = (inputSeconds % 3600) / 60;
    int displayValue = (hours * 100) + minutes;

    if (hours > 0) {
      if (isCoinInserted) {
        display.showNumberDecEx(displayValue, 0x40, false, 4, 0);
      } else {
        if (isColonActive) {
          display.showNumberDecEx(displayValue, 0x00, false, 4, 0);
          isColonActive = false;
        } else {
          display.showNumberDecEx(displayValue, 0x40, false, 4, 0);
          isColonActive = true;
        }
      }
    } else {
      display.showNumberDecEx(displayValue, 0x00, false, 4, 0);
    }

  } else {
    int minutes = inputSeconds / 60;
    int seconds = inputSeconds % 60;

    int displayValue = minutes * 100 + seconds;
    if (minutes == 0 && seconds < 60) {
      display.showNumberDecEx(displayValue, 0x00, false, 4, 0);
    } else {
      display.showNumberDecEx(displayValue, 0x40, false, 4, 0);
    }
  }
}

void coinpulse() {
  if (!isModeSelecting) {
    unsigned long coinPulseCurrentTime = millis();
    if (coinPulseCurrentTime - coinPulseLasDebounceTime > 70) {
      isCoinInserted = true;
      coinPulseLasDebounceTime = coinPulseCurrentTime;
    }
  }
}

void coinInsertDetector() {
  if (isCoinInserted) {
    isCoinInserted = false;
    totalCoinsInserted++;
    display.setSegments(&SEGMENT_P, 1, 0);
    display.showNumberDec(totalCoinsInserted, false, 3, 1);
    Serial.println("Coin inserted");
  }
}

void blinkDisplayInPaused() {
  if (isTM1637TurnedON) {
    display.clear();
    isTM1637TurnedON = false;
  } else {
    calculateTime(totalSeconds);
    isTM1637TurnedON = true;
  }
}

void setup() {
  Serial.begin(9600);
  attachInterrupt(digitalPinToInterrupt(coinAcceptorPin), coinpulse, RISING);

  pinMode(modeSelectionPin, INPUT_PULLUP);
  pinMode(increaseValuePin, INPUT_PULLUP);
  pinMode(decreaseValuePin, INPUT_PULLUP);
  pinMode(resetSettingsPin, INPUT_PULLUP);
  pinMode(waterButtonPin, INPUT_PULLUP);
  pinMode(soapButtonPin, INPUT_PULLUP);
  pinMode(endTimeButtonPin, INPUT);

  pinMode(buzzerPin, OUTPUT);
  pinMode(coinAcceptorSwitchPin, OUTPUT);
  pinMode(waterRelayPin, OUTPUT);
  pinMode(soapRelayPin, OUTPUT);

  display.setBrightness(7);
  waterPulseValue = readFromEEPROM(waterAddress);
  soapPulseValue = readFromEEPROM(soapAddress);
  pauseResumeValue = readFromEEPROM(pauseResumeAddress);
  smartSwitchingValue = readFromEEPROM(smartSwitchingAddress);

  Serial.print("Water pulse value: ");
  Serial.println(waterPulseValue);
  Serial.print("Soap pulse value: ");
  Serial.println(soapPulseValue);
}

void loop() {
  if (isBooting) {
    uint8_t dashes[] = {
      0x40,  // Dash segment code
      0x40,  // Dash segment code
      0x40,  // Dash segment code
      0x40   // Dash segment code
    };

    display.setSegments(dashes);
    if (waterPulseValue == 0 || soapPulseValue == 0) {
      waterPulseValue = readFromEEPROM(0);
      soapPulseValue = readFromEEPROM(1);
    }
    digitalWrite(coinAcceptorSwitchPin, HIGH);
    digitalWrite(soapRelayPin, LOW);
    digitalWrite(waterRelayPin, LOW);
    delay(5000);
    isBooting = false;
    totalCoinsInserted = 0;
    isTimerRunning = false;
    isCoinInserted = false;
    display.setSegments(&SEGMENT_P, 1, 0);
    display.showNumberDec(totalCoinsInserted, false, 3, 1);
  } else {
    if (!isTimerRunning) {
      coinInsertDetector();
    }
    buttonListeners();
    initializeTime();
  }
}