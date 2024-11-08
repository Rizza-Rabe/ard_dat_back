#include <TM1637Display.h>
#include <EEPROM.h>

int totalSeconds = 0;
int currentPulseValue = 0;
int coinPulsePin = 2;
const int CLK = 9;
const int DIO = 3;
int relaySignalPin = 4;
int buzzerSignalPin = 8;

const int decreaseSecondButton = 6;
const int editPulseButton = 5;
const int increaseSecondButton = 7;
const int resetSettingsButton = A3;

const unsigned long longPressThreshold = 1000;
unsigned long buttonPressStartTime = 0;

bool buttonPressed = false;
bool longPressDetected = false;
bool isPulseValueEditMode = false;
bool isTimerInUse = false;
volatile bool isCoinInserted = false;
bool isBeeping = false;
bool isBooting = true;
bool isColonActive = false;

const uint8_t SEGMENT_F = 0b01110001;  // P

unsigned long previousMillis = 0;  // Store the last time the countdown was updated
const long interval = 1000;        // Interval to count down (1000 ms = 1 second)

const uint8_t SEGMENT_D = 0b00111110;  // D
const uint8_t SEGMENT_O = 0b00111111;  // O
const uint8_t SEGMENT_N = 0b00110111;  // N
const uint8_t SEGMENT_E = 0b01111001;  // E

// DEBOUNCER
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

TM1637Display display(CLK, DIO);

void setup() {
  Serial.begin(9600);
  attachInterrupt(digitalPinToInterrupt(coinPulsePin), coinpulse, RISING);

  pinMode(relaySignalPin, OUTPUT);
  pinMode(buzzerSignalPin, OUTPUT);
  pinMode(editPulseButton, INPUT_PULLUP);       // Use internal pull-up resistor
  pinMode(decreaseSecondButton, INPUT_PULLUP);  // Enable internal pull-up resistor
  pinMode(increaseSecondButton, INPUT_PULLUP);  // Enable internal pull-up resistor

  digitalWrite(relaySignalPin, LOW);
  display.setBrightness(7);
  currentPulseValue = readIntFromEEPROM();
  Serial.println("Current pulse value: " + currentPulseValue);
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
    delay(4000);
    resetDisplay();
    if (currentPulseValue != readIntFromEEPROM()) {
      Serial.println("INVALID EEPROM VALUE.");
      uint8_t dashes[] = {
        0x40,  // Dash segment code
        0x00,  // Dash segment code
        0x00,  // Dash segment code
        0x40   // Dash segment code
      };
      display.setSegments(dashes);
      digitalWrite(relaySignalPin, HIGH);
    } else {
      isCoinInserted = false;
      totalSeconds = 0;
      isBooting = false;
      Serial.println("EEPROM NORMAL STATE! Value: " + currentPulseValue);
    }

  } else {
    if (isCoinInserted) {
      isCoinInserted = false;
      Serial.println("Coin detected!");
      isTimerInUse = true;
      if (totalSeconds >= 32400) {
        totalSeconds = 32400;
        calculateTime(totalSeconds);
      } else {
        totalSeconds = (totalSeconds + currentPulseValue);
        calculateTime(totalSeconds);
      }
    }

    if (isTimerInUse) {
      countDownTimer();
    } else {
      longPressDetector();
      detectDecreseButton();
      detectIncreaseButton();
    }
  }
}

void countDownTimer() {
  // Countdown loop
  if (!isPulseValueEditMode) {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;
      if (totalSeconds > 0) {
        digitalWrite(relaySignalPin, HIGH);
        calculateTime(totalSeconds);
        if (totalSeconds <= 10) {
          if (isBeeping) {
            digitalWrite(buzzerSignalPin, LOW);
            isBeeping = false;
          } else {
            digitalWrite(buzzerSignalPin, HIGH);
            isBeeping = true;
          }
        }else{
          digitalWrite(buzzerSignalPin, LOW);
        }
        totalSeconds--;
      } else {
        digitalWrite(relaySignalPin, LOW);
        digitalWrite(buzzerSignalPin, LOW);
        totalSeconds = 0;
        isTimerInUse = false;
        resetDisplay();
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

void resetDisplay() {
  display.showNumberDecEx(0, 0x40, false, 4, 0);
}

void writeIntToEEPROM(int value) {
  EEPROM.update(0, value);
  resetDisplay();
}

void forceWriteToEEPROM(int value) {
  EEPROM.put(0, value);
  resetDisplay();
}

int readIntFromEEPROM() {
  int value = 0;
  EEPROM.get(0, value);
  if (value == -1 || value == 0) {
    value = 60;
  }
  return value;
}

void setDisplayInSetupMode(int pulseValueInSeconds) {
  display.setSegments(&SEGMENT_F, 1, 0);
  display.showNumberDec(pulseValueInSeconds, false, 3, 1);
}

void longPressDetector() {
  // Read the button state
  bool currentButtonState = digitalRead(editPulseButton) == LOW;  // Assuming button is connected to ground

  if (currentButtonState && !buttonPressed) {
    // Button was just pressed
    buttonPressStartTime = millis();
    buttonPressed = true;
    longPressDetected = false;  // Reset long press detection
  } else if (!currentButtonState && buttonPressed) {
    buttonPressed = false;
  }

  if (buttonPressed) {
    // Check if the button has been pressed long enough
    unsigned long pressDuration = millis() - buttonPressStartTime;
    if (pressDuration >= longPressThreshold) {
      if (!longPressDetected) {
        Serial.println("Long press detected");
        longPressDetected = true;
        if (isPulseValueEditMode) {
          isPulseValueEditMode = false;
          Serial.println("Saving value to EEPROM...");
          writeIntToEEPROM(currentPulseValue);
          if (readIntFromEEPROM() == currentPulseValue) {
            Serial.println("Saved to EEPROM");
            uint8_t data[] = { SEGMENT_D, SEGMENT_O, SEGMENT_N, SEGMENT_E };  // DONE
            display.setSegments(data);
            digitalWrite(buzzerSignalPin, HIGH);
            delay(100);
            digitalWrite(buzzerSignalPin, LOW);
            delay(100);
            digitalWrite(buzzerSignalPin, HIGH);
            delay(100);
            digitalWrite(buzzerSignalPin, LOW);
            delay(1000);
            resetDisplay();
          } else {
            forceWriteToEEPROM(currentPulseValue);
            if (readIntFromEEPROM() == currentPulseValue) {
              Serial.println("Saved to EEPROM");
              uint8_t data[] = { SEGMENT_D, SEGMENT_O, SEGMENT_N, SEGMENT_E };  // DONE
              display.setSegments(data);
              digitalWrite(buzzerSignalPin, HIGH);
              delay(100);
              digitalWrite(buzzerSignalPin, LOW);
              delay(100);
              digitalWrite(buzzerSignalPin, HIGH);
              delay(100);
              digitalWrite(buzzerSignalPin, LOW);
              delay(1000);
              resetDisplay();
            } else {
              digitalWrite(buzzerSignalPin, HIGH);
              delay(2000);
              digitalWrite(buzzerSignalPin, LOW);
              Serial.println("Failed to save data to EEPROM at address 0.");
            }
          }
        } else {
          isPulseValueEditMode = true;
          Serial.println("Editing pulse value!");
          resetDisplay();
          setDisplayInSetupMode(currentPulseValue);
          digitalWrite(buzzerSignalPin, HIGH);
          delay(500);
          digitalWrite(buzzerSignalPin, LOW);
        }
      }
    }
  }
}

void oneBeep(int delayMs) {
  digitalWrite(buzzerSignalPin, HIGH);
  delay(delayMs);
  digitalWrite(buzzerSignalPin, LOW);
}

void detectDecreseButton() {
  if (isPulseValueEditMode && currentPulseValue != 1) {
    bool buttonState = digitalRead(decreaseSecondButton) == LOW;  // Button press reads LOW
    if (buttonState) {
      currentPulseValue--;
      setDisplayInSetupMode(currentPulseValue);
    }
    delay(70);
  }
}

void detectIncreaseButton() {
  if (isPulseValueEditMode && currentPulseValue != 960) {
    bool buttonState = digitalRead(increaseSecondButton) == LOW;
    if (buttonState) {
      currentPulseValue++;
      setDisplayInSetupMode(currentPulseValue);
    }
    delay(70);  // Debounce delay
  }
}

void coinpulse() {
  unsigned long currentTime = millis();
  if ((currentTime - lastDebounceTime) > debounceDelay) {
    isCoinInserted = true;
    lastDebounceTime = currentTime;
  }
}
