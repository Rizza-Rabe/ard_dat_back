#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <SoftwareSerial.h>

int totalCoinsInserted = 0;
int price = 0;
int foggingDuration = 0;
int vacuumDuration = 0;
int dryingDuration = 0;
int uvLightDuration = 0;
int selectedMode = 0;
int foggingConsumableDuration = 0;
int vacuumConsumableDuration = 0;
int dryingConsumableDuration = 0;
int uvLightConsumableDuration = 0;
int totalConsumedSeconds = 0;
int totalCombinedSeconds = 0;
int totalCleaned = 0;
int recipientNumberEditingPosition = 0;

long totalIncome = 0L;

const int decreaseButtonPin = A0;
const int setModeButtonPin = A1;
const int increaseButtonPin = A2;
const int resetSettingsButtonPin = A3;
const int coinAcceptorPin = 2;
const int coinAcceptorSignalPin = 3;
const int foggingRelaySignalPin = 4;
const int vacuumRelaySignalPin = 5;
const int dryingRelaySignalPin = 6;
const int uvLightRelaySignalPin = 7;
const int numStrings = 4;
const int maxStringLength = 10;

unsigned long coinPulseLasDebounceTime = 0;
unsigned long editingBlinkerPreviousMillis = 0;
unsigned long timerPreviousMillis = 0;
unsigned long lastActionTime = 0;
unsigned long lastReceiveCheck = 0;

const unsigned long actionInterval = 100;
const unsigned long receiveInterval = 100;

bool isBooting = true;
bool isCoinInserted = false;
bool isModeSelecting = false;
bool isInEditMode = false;
bool hasBlinked = false;
bool isTimerRunning = false;

String currentModeInProgress = "";
String recipientNumberInString = "+63";
String messageToSend;
String receivedMessage;
String receivedSMSCommand;

int recipientNumber[] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// SET/MODE BUTTON
bool isModeSelectionButtonPresed;
unsigned long modeSelectionButtonPressStartTime = 0;
bool isModeSelectionButtonLongPressDetected = false;

// DECREASE BUTTON
bool isDecreaseButtonPresed;
unsigned long decreaseButtonPressStartTime = 0;
bool isDecreaseButtonLongPressDetected = false;

// INCREASE BUTTON
bool isIncreaseButtonPresed;
unsigned long increaseButtonPressStartTime = 0;
bool isIncreaseButtonLongPressDetected = false;

// RESET BUTTON
bool isResetSettingsButtonPresed;
unsigned long resetSettingsButtonPressStartTime = 0;
bool isResetSettingsButtonLongPressDetected = false;

enum SMSSendState { 
  SMS_IDLE, 
  SMS_INIT, 
  SMS_SET_TEXT_MODE, 
  SMS_SET_RECIPIENT, 
  SMS_SEND_MESSAGE, 
  SMS_COMPLETE 
};

// SMS Receiving States
enum SMSReceiveState { 
  SMS_RECEIVE_IDLE, 
  SMS_RECEIVE_INIT, 
  SMS_RECEIVE_LIST, 
  SMS_RECEIVE_PROCESS 
};

SMSSendState smsSendState = SMS_IDLE;
SMSReceiveState smsReceiveState = SMS_RECEIVE_IDLE;

LiquidCrystal_I2C lcd(0x27, 16, 2);
SoftwareSerial sim800L(8, 9);

void setModeButton() {
  bool buttonState = analogRead(setModeButtonPin) < 500;
  if (buttonState && !isModeSelectionButtonPresed) {
    modeSelectionButtonPressStartTime = millis();
    isModeSelectionButtonPresed = true;
    isModeSelectionButtonLongPressDetected = false;
  } else if (!buttonState && isModeSelectionButtonPresed) {
    unsigned long modeSelectionSinglePressDuration = millis() - modeSelectionButtonPressStartTime;
    if (modeSelectionSinglePressDuration < 1000) {
      Serial.println("Mode Selection short press");
      if (!isInEditMode && !isTimerRunning) {
        if (!isModeSelecting) {
          selectedMode = 1;
          isModeSelecting = true;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Set Price:");
          lcd.setCursor(0, 1);
          lcd.print("Price: P" + String(price));
        } else {
          selectedMode = 0;
          isModeSelecting = false;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Insert P" + String(price) + " Coin");
          lcd.setCursor(0, 1);
          lcd.print("Coins: P" + String(totalCoinsInserted));
        }
      }
    }
    isModeSelectionButtonPresed = false;
  }

  if (isModeSelectionButtonPresed) {
    unsigned long modeSelectionLongPressDuration = millis() - modeSelectionButtonPressStartTime;
    if (modeSelectionLongPressDuration >= 1000 && !isTimerRunning) {
      if (!isModeSelectionButtonLongPressDetected) {
        Serial.println("Mode selection long pressed.");
        if (!isInEditMode) {
          isInEditMode = true;
        } else {
          isInEditMode = false;
          if (selectedMode == 1) {
            saveToEEPROM(0, price);
            lcd.setCursor(7, 1);
            lcd.print("P" + String(price));
          } else if (selectedMode == 2) {
            saveToEEPROM(2, foggingDuration);
            lcd.setCursor(10, 1);
            lcd.print("    ");
            lcd.setCursor(10, 1);
            int minutes = foggingDuration / 60;
            int seconds = foggingDuration % 60;
            if (minutes < 10) {
              lcd.print("0");
            }
            lcd.print(minutes);
            lcd.print(":");
            if (seconds < 10) {
              lcd.print("0");
            }
            lcd.print(seconds);
          } else if (selectedMode == 3) {
            saveToEEPROM(4, vacuumDuration);
            lcd.setCursor(10, 1);
            lcd.print("    ");
            lcd.setCursor(10, 1);
            int minutes = vacuumDuration / 60;
            int seconds = vacuumDuration % 60;
            if (minutes < 10) {
              lcd.print("0");
            }
            lcd.print(minutes);
            lcd.print(":");
            if (seconds < 10) {
              lcd.print("0");
            }
            lcd.print(seconds);
          } else if (selectedMode == 4) {
            saveToEEPROM(6, dryingDuration);
            lcd.setCursor(10, 1);
            lcd.print("    ");
            lcd.setCursor(10, 1);
            int minutes = dryingDuration / 60;
            int seconds = dryingDuration % 60;
            if (minutes < 10) {
              lcd.print("0");
            }
            lcd.print(minutes);
            lcd.print(":");
            if (seconds < 10) {
              lcd.print("0");
            }
            lcd.print(seconds);
          } else if (selectedMode == 5) {
            saveToEEPROM(8, uvLightDuration);
            lcd.setCursor(10, 1);
            lcd.print("    ");
            lcd.setCursor(10, 1);
            int minutes = uvLightDuration / 60;
            int seconds = uvLightDuration % 60;
            if (minutes < 10) {
              lcd.print("0");
            }
            lcd.print(minutes);
            lcd.print(":");
            if (seconds < 10) {
              lcd.print("0");
            }
            lcd.print(seconds);
          } else if (selectedMode == 6) {
            saveArrayToEEPROM(recipientNumber);
            lcd.setCursor(4, 1);
            lcd.print("          ");
            lcd.setCursor(4, 1);
            recipientNumberInString = "+63";
            for (int a = 0; a != 10; a++) {
              lcd.print(recipientNumber[a]);
              recipientNumberInString += String(recipientNumber[a]);
            }
            Serial.print("New mobile number is ");
            Serial.println(recipientNumberInString);
          }
        }
      }
      isModeSelectionButtonLongPressDetected = true;
    }
  }
}

void decreaseButton() {
  if (selectedMode == 6) {
    bool buttonState = analogRead(decreaseButtonPin) < 500;
    if (buttonState && !isDecreaseButtonPresed) {
      decreaseButtonPressStartTime = millis();
      isDecreaseButtonPresed = true;
      isDecreaseButtonLongPressDetected = false;
    } else if (!buttonState && isDecreaseButtonPresed) {
      unsigned long decreaseSinglePressDuration = millis() - decreaseButtonPressStartTime;
      if (decreaseSinglePressDuration < 1000) {
        Serial.println("Decrease short press");
        decreaseButtonFunction();
        delay(200);
      }
      isDecreaseButtonPresed = false;
    }

    if (isDecreaseButtonPresed) {
      unsigned long decreaseLongPressDuration = millis() - decreaseButtonPressStartTime;
      if (decreaseLongPressDuration >= 1000) {
        if (!isDecreaseButtonLongPressDetected) {
          Serial.println("Decrease long pressed.");
          if (selectedMode == 6 && recipientNumberEditingPosition > 0) {
            lcd.setCursor(4, 1);
            for (int a = 0; a != 10; a++) {
              lcd.print(recipientNumber[a]);
            }
            recipientNumberEditingPosition--;
          }
        }
        isDecreaseButtonLongPressDetected = true;
      }
    }
  } else {
    bool buttonState = analogRead(decreaseButtonPin) < 500;
    if (buttonState) {
      decreaseButtonFunction();
      delay(200);
    }
  }
}

void decreaseButtonFunction() {
  if (isModeSelecting) {
    if (!isInEditMode) {
      if (selectedMode != 1) {
        selectedMode--;
        if (selectedMode == 1) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Set Price:");
          lcd.setCursor(0, 1);
          lcd.print("Price: P" + String(price));
        } else if (selectedMode == 2) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("1. FOGGING:");
          lcd.setCursor(0, 1);
          lcd.print("Duration: ");
          int minutes = foggingDuration / 60;
          int seconds = foggingDuration % 60;
          if (minutes < 10) {
            lcd.print("0");
          }
          lcd.print(minutes);
          lcd.print(":");
          if (seconds < 10) {
            lcd.print("0");
          }
          lcd.print(seconds);
        } else if (selectedMode == 3) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("2. VACUUM:");
          lcd.setCursor(0, 1);
          lcd.print("Duration: ");
          int minutes = vacuumDuration / 60;
          int seconds = vacuumDuration % 60;
          if (minutes < 10) {
            lcd.print("0");
          }
          lcd.print(minutes);
          lcd.print(":");
          if (seconds < 10) {
            lcd.print("0");
          }
          lcd.print(seconds);
        } else if (selectedMode == 4) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("3. DRYING:");
          lcd.setCursor(0, 1);
          lcd.print("Duration: ");
          int minutes = dryingDuration / 60;
          int seconds = dryingDuration % 60;
          if (minutes < 10) {
            lcd.print("0");
          }
          lcd.print(minutes);
          lcd.print(":");
          if (seconds < 10) {
            lcd.print("0");
          }
          lcd.print(seconds);
        } else if (selectedMode == 5) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("4. UV LIGHT:");
          lcd.setCursor(0, 1);
          lcd.print("Duration: ");
          int minutes = foggingDuration / 60;
          int seconds = foggingDuration % 60;
          if (minutes < 10) {
            lcd.print("0");
          }
          lcd.print(minutes);
          lcd.print(":");
          if (seconds < 10) {
            lcd.print("0");
          }
          lcd.print(seconds);
        } else if (selectedMode == 6) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("RECIPIENT:");
          lcd.setCursor(0, 1);
          lcd.print("+63 ");
          for (int a = 0; a != 10; a++) {
            lcd.print(recipientNumber[a]);
          }
        }
      }
    } else {
      if (selectedMode == 1) {
        if (price != 5) {
          price--;
          lcd.setCursor(7, 1);
          lcd.print("   ");
          lcd.setCursor(7, 1);
          lcd.print("P" + String(price));
        }
      } else if (selectedMode == 2) {
        if (foggingDuration != 0) {
          foggingDuration--;
          lcd.setCursor(10, 1);
          lcd.print("    ");
          lcd.setCursor(10, 1);
          int minutes = foggingDuration / 60;
          int seconds = foggingDuration % 60;
          if (minutes < 10) {
            lcd.print("0");
          }
          lcd.print(minutes);
          lcd.print(":");
          if (seconds < 10) {
            lcd.print("0");
          }
          lcd.print(seconds);
        }
      } else if (selectedMode == 3) {
        if (vacuumDuration != 0) {
          vacuumDuration--;
          lcd.setCursor(10, 1);
          lcd.print("    ");
          lcd.setCursor(10, 1);
          int minutes = vacuumDuration / 60;
          int seconds = vacuumDuration % 60;
          if (minutes < 10) {
            lcd.print("0");
          }
          lcd.print(minutes);
          lcd.print(":");
          if (seconds < 10) {
            lcd.print("0");
          }
          lcd.print(seconds);
        }
      } else if (selectedMode == 4) {
        if (dryingDuration != 0) {
          dryingDuration--;
          lcd.setCursor(10, 1);
          lcd.print("    ");
          lcd.setCursor(10, 1);
          int minutes = dryingDuration / 60;
          int seconds = dryingDuration % 60;
          if (minutes < 10) {
            lcd.print("0");
          }
          lcd.print(minutes);
          lcd.print(":");
          if (seconds < 10) {
            lcd.print("0");
          }
          lcd.print(seconds);
        }
      } else if (selectedMode == 5) {
        if (uvLightDuration != 0) {
          uvLightDuration--;
          lcd.setCursor(10, 1);
          lcd.print("    ");
          lcd.setCursor(10, 1);
          int minutes = uvLightDuration / 60;
          int seconds = uvLightDuration % 60;
          if (minutes < 10) {
            lcd.print("0");
          }
          lcd.print(minutes);
          lcd.print(":");
          if (seconds < 10) {
            lcd.print("0");
          }
          lcd.print(seconds);
        }
      } else if (selectedMode == 6) {
        if (recipientNumberEditingPosition == 0) {
          lcd.setCursor(4, 1);
          if (recipientNumber[0] == 0) {
            recipientNumber[0] = 9;
            lcd.print(recipientNumber[0]);
          } else {
            recipientNumber[0]--;
            lcd.print(recipientNumber[0]);
          }
        } else if (recipientNumberEditingPosition == 1) {
          lcd.setCursor(5, 1);
          if (recipientNumber[1] == 0) {
            recipientNumber[1] = 9;
            lcd.print(recipientNumber[1]);
          } else {
            recipientNumber[1]--;
            lcd.print(recipientNumber[1]);
          }
        } else if (recipientNumberEditingPosition == 2) {
          lcd.setCursor(6, 1);
          if (recipientNumber[2] == 0) {
            recipientNumber[2] = 9;
            lcd.print(recipientNumber[2]);
          } else {
            recipientNumber[2]--;
            lcd.print(recipientNumber[2]);
          }
        } else if (recipientNumberEditingPosition == 3) {
          lcd.setCursor(7, 1);
          if (recipientNumber[3] == 0) {
            recipientNumber[3] = 9;
            lcd.print(recipientNumber[3]);
          } else {
            recipientNumber[3]--;
            lcd.print(recipientNumber[3]);
          }
        } else if (recipientNumberEditingPosition == 4) {
          lcd.setCursor(8, 1);
          if (recipientNumber[4] == 0) {
            recipientNumber[4] = 9;
            lcd.print(recipientNumber[4]);
          } else {
            recipientNumber[4]--;
            lcd.print(recipientNumber[4]);
          }
        } else if (recipientNumberEditingPosition == 5) {
          lcd.setCursor(9, 1);
          if (recipientNumber[5] == 0) {
            recipientNumber[5] = 9;
            lcd.print(recipientNumber[5]);
          } else {
            recipientNumber[5]--;
            lcd.print(recipientNumber[5]);
          }
        } else if (recipientNumberEditingPosition == 6) {
          lcd.setCursor(10, 1);
          if (recipientNumber[6] == 0) {
            recipientNumber[6] = 9;
            lcd.print(recipientNumber[6]);
          } else {
            recipientNumber[6]--;
            lcd.print(recipientNumber[6]);
          }
        } else if (recipientNumberEditingPosition == 7) {
          lcd.setCursor(11, 1);
          if (recipientNumber[7] == 0) {
            recipientNumber[7] = 9;
            lcd.print(recipientNumber[0]);
          } else {
            recipientNumber[7]--;
            lcd.print(recipientNumber[7]);
          }
        } else if (recipientNumberEditingPosition == 8) {
          lcd.setCursor(12, 1);
          if (recipientNumber[8] == 0) {
            recipientNumber[8] = 9;
            lcd.print(recipientNumber[8]);
          } else {
            recipientNumber[8]--;
            lcd.print(recipientNumber[8]);
          }
        } else if (recipientNumberEditingPosition == 9) {
          lcd.setCursor(13, 1);
          if (recipientNumber[9] == 0) {
            recipientNumber[9] = 9;
            lcd.print(recipientNumber[9]);
          } else {
            recipientNumber[9]--;
            lcd.print(recipientNumber[9]);
          }
        } else if (recipientNumberEditingPosition == 10) {
          lcd.setCursor(14, 1);
          if (recipientNumber[10] == 0) {
            recipientNumber[10] = 9;
            lcd.print(recipientNumber[10]);
          } else {
            recipientNumber[10]--;
            lcd.print(recipientNumber[10]);
          }
        }
      }
    }
  }
}

void increaseButton() {
  if (selectedMode == 6) {
    bool buttonState = analogRead(increaseButtonPin) < 500;
    if (buttonState && !isIncreaseButtonPresed) {
      increaseButtonPressStartTime = millis();
      isIncreaseButtonPresed = true;
      isIncreaseButtonLongPressDetected = false;
    } else if (!buttonState && isIncreaseButtonPresed) {
      unsigned long increaseSinglePressDuration = millis() - increaseButtonPressStartTime;
      if (increaseSinglePressDuration < 1000) {
        Serial.println("Increase short press");
        increaseButtonFunction();
        delay(200);
      }
      isIncreaseButtonPresed = false;
    }

    if (isIncreaseButtonPresed) {
      unsigned long increaseLongPressDuration = millis() - increaseButtonPressStartTime;
      if (increaseLongPressDuration >= 1000) {
        if (!isIncreaseButtonLongPressDetected) {
          Serial.println("Increase long pressed.");
          if (selectedMode == 6 && recipientNumberEditingPosition < 9) {
            lcd.setCursor(4, 1);
            for (int a = 0; a != 10; a++) {
              lcd.print(recipientNumber[a]);
            }
            recipientNumberEditingPosition++;
          }
        }
        isIncreaseButtonLongPressDetected = true;
      }
    }
  } else {
    bool buttonState = analogRead(increaseButtonPin) < 500;
    if (buttonState) {
      increaseButtonFunction();
      delay(200);
    }
  }
}

void increaseButtonFunction() {
  if (isModeSelecting) {
    if (!isInEditMode) {
      if (selectedMode != 6) {
        selectedMode++;
        if (selectedMode == 1) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Set Price:");
          lcd.setCursor(0, 1);
          lcd.print("Price: P" + String(price));
        } else if (selectedMode == 2) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("1. FOGGING:");
          lcd.setCursor(0, 1);
          lcd.print("Duration: ");
          int minutes = foggingDuration / 60;
          int seconds = foggingDuration % 60;
          if (minutes < 10) {
            lcd.print("0");
          }
          lcd.print(minutes);
          lcd.print(":");
          if (seconds < 10) {
            lcd.print("0");
          }
          lcd.print(seconds);
        } else if (selectedMode == 3) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("2. VACUUM:");
          lcd.setCursor(0, 1);
          lcd.print("Duration: ");
          int minutes = vacuumDuration / 60;
          int seconds = vacuumDuration % 60;
          if (minutes < 10) {
            lcd.print("0");
          }
          lcd.print(minutes);
          lcd.print(":");
          if (seconds < 10) {
            lcd.print("0");
          }
          lcd.print(seconds);
        } else if (selectedMode == 4) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("3. DRYING:");
          lcd.setCursor(0, 1);
          lcd.print("Duration: ");
          int minutes = dryingDuration / 60;
          int seconds = dryingDuration % 60;
          if (minutes < 10) {
            lcd.print("0");
          }
          lcd.print(minutes);
          lcd.print(":");
          if (seconds < 10) {
            lcd.print("0");
          }
          lcd.print(seconds);
        } else if (selectedMode == 5) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("4. UV LIGHT:");
          lcd.setCursor(0, 1);
          lcd.print("Duration: ");
          int minutes = uvLightDuration / 60;
          int seconds = uvLightDuration % 60;
          if (minutes < 10) {
            lcd.print("0");
          }
          lcd.print(minutes);
          lcd.print(":");
          if (seconds < 10) {
            lcd.print("0");
          }
          lcd.print(seconds);
        } else if (selectedMode == 6) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("RECIPIENT:");
          lcd.setCursor(0, 1);
          lcd.print("+63 ");
          for (int a = 0; a != 10; a++) {
            lcd.print(recipientNumber[a]);
          }
        }
      }

    } else {
      if (selectedMode == 1) {
        if (price != 500) {
          price++;
          lcd.setCursor(7, 1);
          lcd.print("   ");
          lcd.setCursor(7, 1);
          lcd.print("P" + String(price));
        }
      } else if (selectedMode == 2) {
        if (foggingDuration != 960) {
          foggingDuration++;
          lcd.setCursor(10, 1);
          lcd.print("    ");
          lcd.setCursor(10, 1);
          int minutes = foggingDuration / 60;
          int seconds = foggingDuration % 60;
          if (minutes < 10) {
            lcd.print("0");
          }
          lcd.print(minutes);
          lcd.print(":");
          if (seconds < 10) {
            lcd.print("0");
          }
          lcd.print(seconds);
        }
      } else if (selectedMode == 3) {
        if (vacuumDuration != 960) {
          vacuumDuration++;
          lcd.setCursor(10, 1);
          lcd.print("    ");
          lcd.setCursor(10, 1);
          int minutes = vacuumDuration / 60;
          int seconds = vacuumDuration % 60;
          if (minutes < 10) {
            lcd.print("0");
          }
          lcd.print(minutes);
          lcd.print(":");
          if (seconds < 10) {
            lcd.print("0");
          }
          lcd.print(seconds);
        }
      } else if (selectedMode == 4) {
        if (dryingDuration != 960) {
          dryingDuration++;
          lcd.setCursor(10, 1);
          lcd.print("    ");
          lcd.setCursor(10, 1);
          int minutes = dryingDuration / 60;
          int seconds = dryingDuration % 60;
          if (minutes < 10) {
            lcd.print("0");
          }
          lcd.print(minutes);
          lcd.print(":");
          if (seconds < 10) {
            lcd.print("0");
          }
          lcd.print(seconds);
        }
      } else if (selectedMode == 5) {
        if (uvLightDuration != 960) {
          uvLightDuration++;
          lcd.setCursor(10, 1);
          lcd.print("    ");
          lcd.setCursor(10, 1);
          int minutes = uvLightDuration / 60;
          int seconds = uvLightDuration % 60;
          if (minutes < 10) {
            lcd.print("0");
          }
          lcd.print(minutes);
          lcd.print(":");
          if (seconds < 10) {
            lcd.print("0");
          }
          lcd.print(seconds);
        }
      } else if (selectedMode == 6) {
        if (recipientNumberEditingPosition == 0) {
          lcd.setCursor(4, 1);
          if (recipientNumber[0] == 9) {
            recipientNumber[0] = 0;
            lcd.print(recipientNumber[0]);
          } else {
            recipientNumber[0]++;
            lcd.print(recipientNumber[0]);
          }
        } else if (recipientNumberEditingPosition == 1) {
          lcd.setCursor(5, 1);
          if (recipientNumber[1] == 9) {
            recipientNumber[1] = 0;
            lcd.print(recipientNumber[1]);
          } else {
            recipientNumber[1]++;
            lcd.print(recipientNumber[1]);
          }
        } else if (recipientNumberEditingPosition == 2) {
          lcd.setCursor(6, 1);
          if (recipientNumber[2] == 9) {
            recipientNumber[2] = 0;
            lcd.print(recipientNumber[2]);
          } else {
            recipientNumber[2]++;
            lcd.print(recipientNumber[2]);
          }
        } else if (recipientNumberEditingPosition == 3) {
          lcd.setCursor(7, 1);
          if (recipientNumber[3] == 9) {
            recipientNumber[3] = 0;
            lcd.print(recipientNumber[3]);
          } else {
            recipientNumber[3]++;
            lcd.print(recipientNumber[3]);
          }
        } else if (recipientNumberEditingPosition == 4) {
          lcd.setCursor(8, 1);
          if (recipientNumber[4] == 9) {
            recipientNumber[4] = 0;
            lcd.print(recipientNumber[4]);
          } else {
            recipientNumber[4]++;
            lcd.print(recipientNumber[4]);
          }
        } else if (recipientNumberEditingPosition == 5) {
          lcd.setCursor(9, 1);
          if (recipientNumber[5] == 9) {
            recipientNumber[5] = 0;
            lcd.print(recipientNumber[5]);
          } else {
            recipientNumber[5]++;
            lcd.print(recipientNumber[5]);
          }
        } else if (recipientNumberEditingPosition == 6) {
          lcd.setCursor(10, 1);
          if (recipientNumber[6] == 9) {
            recipientNumber[6] = 0;
            lcd.print(recipientNumber[6]);
          } else {
            recipientNumber[6]++;
            lcd.print(recipientNumber[6]);
          }
        } else if (recipientNumberEditingPosition == 7) {
          lcd.setCursor(11, 1);
          if (recipientNumber[7] == 9) {
            recipientNumber[7] = 0;
            lcd.print(recipientNumber[7]);
          } else {
            recipientNumber[7]++;
            lcd.print(recipientNumber[7]);
          }
        } else if (recipientNumberEditingPosition == 8) {
          lcd.setCursor(12, 1);
          if (recipientNumber[8] == 9) {
            recipientNumber[8] = 0;
            lcd.print(recipientNumber[8]);
          } else {
            recipientNumber[8]++;
            lcd.print(recipientNumber[8]);
          }
        } else if (recipientNumberEditingPosition == 9) {
          lcd.setCursor(13, 1);
          if (recipientNumber[9] == 9) {
            recipientNumber[9] = 0;
            lcd.print(recipientNumber[9]);
          } else {
            recipientNumber[9]++;
            lcd.print(recipientNumber[9]);
          }
        } else if (recipientNumberEditingPosition == 10) {
          lcd.setCursor(14, 1);
          if (recipientNumber[10] == 9) {
            recipientNumber[10] = 0;
            lcd.print(recipientNumber[10]);
          } else {
            recipientNumber[10]++;
            lcd.print(recipientNumber[10]);
          }
        }
      }
    }
  }
}

void resetSettingsButton() {
  bool buttonState = analogRead(resetSettingsButtonPin) < 500;
  if (buttonState && !isResetSettingsButtonPresed) {
    resetSettingsButtonPressStartTime = millis();
    isResetSettingsButtonPresed = true;
    isModeSelectionButtonLongPressDetected = false;
  }

  if (isResetSettingsButtonPresed) {
    unsigned long isResetSettingsButtonLongPressDetected = millis() - resetSettingsButtonPressStartTime;
    if (isResetSettingsButtonLongPressDetected >= 1000) {
      if (!isModeSelectionButtonLongPressDetected) {
        Serial.println("Reset settings long pressed.");
      }
      isModeSelectionButtonLongPressDetected = true;
    }
  }
}

void initializeEEPROM() {
  EEPROM.get(0, price);
  EEPROM.get(2, foggingDuration);
  EEPROM.get(4, vacuumDuration);
  EEPROM.get(6, dryingDuration);
  EEPROM.get(8, uvLightDuration);

  if (price == 0 || price == -1) {
    price = 25;
  }

  if (foggingDuration == 0 || foggingDuration == -1) {
    foggingDuration = 120;
  }

  if (vacuumDuration == 0 || vacuumDuration == -1) {
    vacuumDuration = 120;
  }

  if (dryingDuration == 0 || dryingDuration == -1) {
    dryingDuration = 90;
  }

  if (uvLightDuration == 0 || uvLightDuration == -1) {
    uvLightDuration = 120;
  }

  Serial.println("EEPROM has been successfully initialized.");
}

void initializeTime() {
  if (isTimerRunning) {
    unsigned long timerCurrentMillis = millis();
    if ((timerCurrentMillis - timerPreviousMillis) >= 1000) {
      timerPreviousMillis = timerCurrentMillis;
      lcd.setCursor(0, 1);
      lcd.print("    ");
      lcd.setCursor(0, 1);

      if (currentModeInProgress == "fog") {
        if (foggingConsumableDuration != 0) {
          int minutes = foggingConsumableDuration / 60;
          int seconds = foggingConsumableDuration % 60;
          if (minutes < 10) {
            lcd.print("0");
          }
          lcd.print(minutes);
          lcd.print(":");
          if (seconds < 10) {
            lcd.print("0");
          }
          lcd.print(seconds);
          foggingConsumableDuration--;
          totalConsumedSeconds++;
          digitalWrite(foggingRelaySignalPin, HIGH);
          digitalWrite(vacuumRelaySignalPin, LOW);
          digitalWrite(dryingRelaySignalPin, LOW);
          digitalWrite(uvLightRelaySignalPin, LOW);
        } else {
          currentModeInProgress = "vacuum";
          vacuumConsumableDuration = vacuumDuration;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("2. VACUUM");
          lcd.setCursor(0, 1);
          lcd.print("    ");
          digitalWrite(foggingRelaySignalPin, LOW);
          digitalWrite(vacuumRelaySignalPin, LOW);
          digitalWrite(dryingRelaySignalPin, LOW);
          digitalWrite(uvLightRelaySignalPin, LOW);
        }
      } else if (currentModeInProgress == "vacuum") {
        if (vacuumConsumableDuration != 0) {
          int minutes = vacuumConsumableDuration / 60;
          int seconds = vacuumConsumableDuration % 60;
          if (minutes < 10) {
            lcd.print("0");
          }
          lcd.print(minutes);
          lcd.print(":");
          if (seconds < 10) {
            lcd.print("0");
          }
          lcd.print(seconds);
          vacuumConsumableDuration--;
          totalConsumedSeconds++;
          digitalWrite(foggingRelaySignalPin, LOW);
          digitalWrite(vacuumRelaySignalPin, HIGH);
          digitalWrite(dryingRelaySignalPin, LOW);
          digitalWrite(uvLightRelaySignalPin, LOW);
        } else {
          currentModeInProgress = "dry";
          dryingConsumableDuration = dryingDuration;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("3. DRYING");
          lcd.setCursor(0, 1);
          lcd.print("    ");
          digitalWrite(foggingRelaySignalPin, LOW);
          digitalWrite(vacuumRelaySignalPin, LOW);
          digitalWrite(dryingRelaySignalPin, LOW);
          digitalWrite(uvLightRelaySignalPin, LOW);
        }
      } else if (currentModeInProgress == "dry") {
        if (dryingConsumableDuration != 0) {
          int minutes = dryingConsumableDuration / 60;
          int seconds = dryingConsumableDuration % 60;
          if (minutes < 10) {
            lcd.print("0");
          }
          lcd.print(minutes);
          lcd.print(":");
          if (seconds < 10) {
            lcd.print("0");
          }
          lcd.print(seconds);
          dryingConsumableDuration--;
          totalConsumedSeconds++;
          digitalWrite(foggingRelaySignalPin, LOW);
          digitalWrite(vacuumRelaySignalPin, LOW);
          digitalWrite(dryingRelaySignalPin, HIGH);
          digitalWrite(uvLightRelaySignalPin, LOW);
        } else {
          currentModeInProgress = "uv";
          uvLightConsumableDuration = uvLightDuration;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("4. UV LIGHT");
          lcd.setCursor(0, 1);
          lcd.print("    ");
          digitalWrite(foggingRelaySignalPin, LOW);
          digitalWrite(vacuumRelaySignalPin, LOW);
          digitalWrite(dryingRelaySignalPin, LOW);
          digitalWrite(uvLightRelaySignalPin, LOW);
        }
      } else if (currentModeInProgress == "uv") {
        if (uvLightConsumableDuration != 0) {
          int minutes = uvLightConsumableDuration / 60;
          int seconds = uvLightConsumableDuration % 60;
          if (minutes < 10) {
            lcd.print("0");
          }
          lcd.print(minutes);
          lcd.print(":");
          if (seconds < 10) {
            lcd.print("0");
          }
          lcd.print(seconds);
          uvLightConsumableDuration--;
          totalConsumedSeconds++;
          digitalWrite(foggingRelaySignalPin, LOW);
          digitalWrite(vacuumRelaySignalPin, LOW);
          digitalWrite(dryingRelaySignalPin, LOW);
          digitalWrite(uvLightRelaySignalPin, HIGH);
        } else {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("CLEANING DONE!");
          Serial.println("CLEANING DONE!");
          digitalWrite(foggingRelaySignalPin, LOW);
          digitalWrite(vacuumRelaySignalPin, LOW);
          digitalWrite(dryingRelaySignalPin, LOW);
          digitalWrite(uvLightRelaySignalPin, LOW);
          delay(3000);
          digitalWrite(coinAcceptorSignalPin, HIGH);
          delay(3000);
          isTimerRunning = false;
          isCoinInserted = false;
          totalCoinsInserted = 0;
          totalConsumedSeconds = 0;
          totalCombinedSeconds = 0;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Insert P" + String(price) + " Coin");
          lcd.setCursor(0, 1);
          lcd.print("Coins: P" + String(totalCoinsInserted));
          lcd.setCursor(12, 1);
          lcd.print("    ");
          totalCleaned++;
        }
      }

      if (isTimerRunning) {
        long percentage = (totalConsumedSeconds * 100L) / totalCombinedSeconds;

        Serial.print("Percentage: ");
        Serial.println(percentage);
        Serial.print("Total consumed seconds: ");
        Serial.println(totalConsumedSeconds);
        Serial.print("/");
        Serial.println(totalCombinedSeconds);

        if (percentage < 9 && percentage != 100) {
          lcd.setCursor(14, 1);
          lcd.print(String(percentage) + "%");
        } else if (percentage >= 10 && percentage != 100) {
          lcd.setCursor(13, 1);
          lcd.print(String(percentage) + "%");
        } else if (percentage >= 100) {
          lcd.setCursor(12, 1);
          lcd.print(String(percentage) + "%");
        }
      }
    }
  }
  if (isInEditMode) {
    unsigned long blinkerCurrentMillis = millis();
    if (blinkerCurrentMillis - editingBlinkerPreviousMillis >= 500) {
      editingBlinkerPreviousMillis = blinkerCurrentMillis;
      if (selectedMode == 1) {
        if (hasBlinked) {
          hasBlinked = false;
          lcd.setCursor(7, 1);
          lcd.print("    ");
          lcd.setCursor(7, 1);
          lcd.print("P" + String(price));
        } else {
          hasBlinked = true;
          lcd.setCursor(7, 1);
          lcd.print("    ");
        }
      } else if (selectedMode == 2) {
        if (hasBlinked) {
          hasBlinked = false;
          lcd.setCursor(10, 1);
          lcd.print("     ");
          lcd.setCursor(10, 1);
          int minutes = foggingDuration / 60;
          int seconds = foggingDuration % 60;
          if (minutes < 10) {
            lcd.print("0");
          }
          lcd.print(minutes);
          lcd.print(":");
          if (seconds < 10) {
            lcd.print("0");
          }
          lcd.print(seconds);
        } else {
          hasBlinked = true;
          lcd.setCursor(10, 1);
          lcd.print("     ");
        }
      } else if (selectedMode == 3) {
        if (hasBlinked) {
          hasBlinked = false;
          lcd.setCursor(10, 1);
          lcd.print("     ");
          lcd.setCursor(10, 1);
          int minutes = vacuumDuration / 60;
          int seconds = vacuumDuration % 60;
          if (minutes < 10) {
            lcd.print("0");
          }
          lcd.print(minutes);
          lcd.print(":");
          if (seconds < 10) {
            lcd.print("0");
          }
          lcd.print(seconds);
        } else {
          hasBlinked = true;
          lcd.setCursor(10, 1);
          lcd.print("     ");
        }
      } else if (selectedMode == 4) {
        if (hasBlinked) {
          hasBlinked = false;
          lcd.setCursor(10, 1);
          lcd.print("     ");
          lcd.setCursor(10, 1);
          int minutes = dryingDuration / 60;
          int seconds = dryingDuration % 60;
          if (minutes < 10) {
            lcd.print("0");
          }
          lcd.print(minutes);
          lcd.print(":");
          if (seconds < 10) {
            lcd.print("0");
          }
          lcd.print(seconds);
        } else {
          hasBlinked = true;
          lcd.setCursor(10, 1);
          lcd.print("     ");
        }
      } else if (selectedMode == 5) {
        if (hasBlinked) {
          hasBlinked = false;
          lcd.setCursor(10, 1);
          lcd.print("     ");
          lcd.setCursor(10, 1);
          int minutes = uvLightDuration / 60;
          int seconds = uvLightDuration % 60;
          if (minutes < 10) {
            lcd.print("0");
          }
          lcd.print(minutes);
          lcd.print(":");
          if (seconds < 10) {
            lcd.print("0");
          }
          lcd.print(seconds);
        } else {
          hasBlinked = true;
          lcd.setCursor(10, 1);
          lcd.print("     ");
        }
      } else if (selectedMode == 6) {
        if (hasBlinked) {
          hasBlinked = false;
          if (recipientNumberEditingPosition == 0) {
            lcd.setCursor(4, 1);
            lcd.print(" ");
          } else if (recipientNumberEditingPosition == 1) {
            lcd.setCursor(5, 1);
            lcd.print(" ");
          } else if (recipientNumberEditingPosition == 2) {
            lcd.setCursor(6, 1);
            lcd.print(" ");
          } else if (recipientNumberEditingPosition == 3) {
            lcd.setCursor(7, 1);
            lcd.print(" ");
          } else if (recipientNumberEditingPosition == 4) {
            lcd.setCursor(8, 1);
            lcd.print(" ");
          } else if (recipientNumberEditingPosition == 5) {
            lcd.setCursor(9, 1);
            lcd.print(" ");
          } else if (recipientNumberEditingPosition == 6) {
            lcd.setCursor(10, 1);
            lcd.print(" ");
          } else if (recipientNumberEditingPosition == 7) {
            lcd.setCursor(11, 1);
            lcd.print(" ");
          } else if (recipientNumberEditingPosition == 8) {
            lcd.setCursor(12, 1);
            lcd.print(" ");
          } else if (recipientNumberEditingPosition == 9) {
            lcd.setCursor(13, 1);
            lcd.print(" ");
          } else if (recipientNumberEditingPosition == 10) {
            lcd.setCursor(14, 1);
            lcd.print(" ");
          }
        } else {
          hasBlinked = true;
          if (recipientNumberEditingPosition == 0) {
            lcd.setCursor(4, 1);
            lcd.print(recipientNumber[0]);
          } else if (recipientNumberEditingPosition == 1) {
            lcd.setCursor(5, 1);
            lcd.print(recipientNumber[1]);
          } else if (recipientNumberEditingPosition == 2) {
            lcd.setCursor(6, 1);
            lcd.print(recipientNumber[2]);
          } else if (recipientNumberEditingPosition == 3) {
            lcd.setCursor(7, 1);
            lcd.print(recipientNumber[3]);
          } else if (recipientNumberEditingPosition == 4) {
            lcd.setCursor(8, 1);
            lcd.print(recipientNumber[4]);
          } else if (recipientNumberEditingPosition == 5) {
            lcd.setCursor(9, 1);
            lcd.print(recipientNumber[5]);
          } else if (recipientNumberEditingPosition == 6) {
            lcd.setCursor(10, 1);
            lcd.print(recipientNumber[6]);
          } else if (recipientNumberEditingPosition == 7) {
            lcd.setCursor(11, 1);
            lcd.print(recipientNumber[7]);
          } else if (recipientNumberEditingPosition == 8) {
            lcd.setCursor(12, 1);
            lcd.print(recipientNumber[8]);
          } else if (recipientNumberEditingPosition == 9) {
            lcd.setCursor(13, 1);
            lcd.print(recipientNumber[9]);
          } else if (recipientNumberEditingPosition == 10) {
            lcd.setCursor(14, 1);
            lcd.print(recipientNumber[10]);
          }
        }
      }
    }
  }
}

void coinpulse() {
  unsigned long coinPulseCurrentTime = millis();
  if (coinPulseCurrentTime - coinPulseLasDebounceTime > 70) {
    isCoinInserted = true;
    coinPulseLasDebounceTime = coinPulseCurrentTime;
  }
}

void coinInsertDetector() {
  if (isCoinInserted) {
    isCoinInserted = false;
    totalCoinsInserted++;
    totalIncome++;
    lcd.setCursor(8, 1);
    lcd.print(" ");
    lcd.setCursor(9, 1);
    lcd.print(" ");
    lcd.setCursor(10, 1);
    lcd.print(" ");
    lcd.setCursor(11, 1);
    lcd.print(" ");
    lcd.setCursor(8, 1);
    lcd.print(String(totalCoinsInserted));
    Serial.println("Coin Inserted.");
    if (totalCoinsInserted >= price && !isTimerRunning) {
      digitalWrite(coinAcceptorSignalPin, LOW);
      isTimerRunning = true;
      currentModeInProgress = "fog";
      foggingConsumableDuration = foggingDuration;
      Serial.println("User is paid. Start the cleaning process.");
      Serial.print("Total income: ");
      Serial.println(totalIncome);
      totalCombinedSeconds = (foggingDuration + vacuumDuration + dryingDuration + uvLightDuration);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("1. FOGGING");
      lcd.setCursor(0, 1);
      lcd.print(String(foggingConsumableDuration) + "s");
      lcd.setCursor(14, 1);
      lcd.print("0%");
    }
  }
}

void saveToEEPROM(int address, int value) {
  int currentValue = 0;
  EEPROM.update(address, value);
  EEPROM.get(address, currentValue);

  if (address == 0) {
    if (price != currentValue) {
      EEPROM.put(address, value);
      Serial.println("Forced write to address 0");
    }
  } else if (address == 2) {
    if (foggingDuration != currentValue) {
      EEPROM.put(address, value);
      Serial.println("Forced write to address 2");
    }
  } else if (address == 4) {
    if (vacuumDuration != currentValue) {
      EEPROM.put(address, value);
      Serial.println("Forced write to address 4");
    }
  } else if (address == 6) {
    if (dryingDuration != currentValue) {
      EEPROM.put(address, value);
      Serial.println("Forced write to address 6");
    }
  } else if (address == 8) {
    if (uvLightDuration != currentValue) {
      EEPROM.put(address, value);
      Serial.println("Forced write to address 8");
    }
  }
}

void saveArrayToEEPROM(int* array) {
  int address = 10;
  for (int i = 0; i < 10; i++) {
    EEPROM.put(address, array[i]);
    Serial.print("Saving ");
    Serial.print(array[i]);
    Serial.print(" in address ");
    Serial.println(address);
    address += sizeof(int);
  }
}

void loadArrayFromEEPROM(int* array) {
  int address = 10;
  int placeHolder = 0;
  for (int i = 0; i < 10; i++) {
    EEPROM.get(address, placeHolder);
    if (placeHolder < 0) {
      placeHolder = 0;
    }
    Serial.print("Address ");
    Serial.print(address);
    Serial.print(" has a data of ");
    Serial.println(placeHolder);

    array[i] = placeHolder;
    recipientNumberInString += String(array[i]);
    address += sizeof(int);
  }
  Serial.print("Current mobile number: ");
  Serial.println(recipientNumberInString);
}

void setup() {
  Serial.begin(9600);
  sim800L.begin(9600);
  attachInterrupt(digitalPinToInterrupt(coinAcceptorPin), coinpulse, RISING);

  pinMode(coinAcceptorSignalPin, OUTPUT);
  pinMode(decreaseButtonPin, INPUT_PULLUP);
  pinMode(setModeButtonPin, INPUT_PULLUP);
  pinMode(increaseButtonPin, INPUT_PULLUP);
  pinMode(resetSettingsButtonPin, INPUT_PULLUP);
  pinMode(foggingRelaySignalPin, OUTPUT);
  pinMode(vacuumRelaySignalPin, OUTPUT);
  pinMode(dryingRelaySignalPin, OUTPUT);
  pinMode(uvLightRelaySignalPin, OUTPUT);

  digitalWrite(foggingRelaySignalPin, LOW);
  digitalWrite(vacuumRelaySignalPin, LOW);
  digitalWrite(dryingRelaySignalPin, LOW);
  digitalWrite(uvLightRelaySignalPin, LOW);

  loadArrayFromEEPROM(recipientNumber);

  lcd.init();
  lcd.backlight();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Starting, please");
  lcd.setCursor(0, 1);
  lcd.print("wait...");
  initializeEEPROM();
  sim800L.println("AT+CMGF=1");
  delay(1000);
  sim800L.println("AT+CNMI=1,2,0,0,0");
  Serial.println("Setup complete. Ready to receive SMS.");
  delay(4500);
  digitalWrite(coinAcceptorSignalPin, HIGH);
  delay(200);
  isCoinInserted = false;
  totalCoinsInserted = 0;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Insert P" + String(price) + " Coin");
  lcd.setCursor(0, 1);
  lcd.print("Coins: P" + String(totalCoinsInserted));

}

void loop() {
  setModeButton();
  decreaseButton();
  increaseButton();
  initializeTime();
  coinInsertDetector();
}