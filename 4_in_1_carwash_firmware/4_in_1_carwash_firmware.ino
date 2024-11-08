#include <TM1637Display.h>
#include <EEPROM.h>

// Define the connections pins
#define CLK 9  // Clock pin
#define DIO 3  // Data pin

// Create an instance of the TM1637Display object
TM1637Display display = TM1637Display(CLK, DIO);

// COIN ACCEPTOR
int coinPulsePin = 2;

// BUTTONS
const int analog_waterButton = 0;
const int analog_soapButton = 1;
const int analog_blowerButton = 2;
const int analog_dispenserButton = 3;
const int analog_modeSelection = 4;
const int analog_decreaseSecondButton = 5;
const int analog_increaseSecondButton = 6;

// RELAYS
const int water_relay = 4;
const int soap_relay = 10;
const int blower_relay = 11;
const int dispenser_relay = 12;
const int coinAcceptor_powerOff_relay = 13;

// SEGMENTS
const uint8_t SEGMENT_D = 0b00111110;  // D
const uint8_t SEGMENT_O = 0b00111111;  // O
const uint8_t SEGMENT_N = 0b00110111;  // N
const uint8_t SEGMENT_E = 0b01111001;  // E
const uint8_t SEGMENT_F = 0b01110001;  // F

// BOOLEAN
bool isCoinInserted = false;
bool isBooting = true;

// INTEGERS
int totalSeconds = 0;
int waterSecondsValue = 0;
int soapSecondsValue = 0;
int blowerSecondsValue = 0;
int insertedCoins = 0;

// DEBOUNCER
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

// OTHERS
const unsigned long longPressThreshold = 1000;
unsigned long buttonPressStartTime = 0;
const int buzzerPin = 8;

uint8_t letterP = 0x73;
uint8_t blankSegment = 0x00;
uint8_t digitSegments[] = {
  0x3F,
  0x06,
  0x5B,
  0x4F,
  0x66,
  0x6D,
  0x7D,
  0x07,
  0x7F,
  0x6F
};

void coinpulse() {
  unsigned long currentTime = millis();
  if ((currentTime - lastDebounceTime) > debounceDelay) {
    isCoinInserted = true;
    lastDebounceTime = currentTime;
  }
}

void setDisplay(int displayValue) {
  display.showNumberDecEx(displayValue, 0x00, false, 4, 0);
}

void setDisplayWithColon(int displayValue) {
  display.showNumberDecEx(displayValue, 0x40, false, 4, 0);
}

void setDisplayMoney(int displayValue) {
  int hundreds = displayValue / 100;
  int tens = (displayValue / 10) % 10;
  int ones = displayValue % 10;

  uint8_t hundredsSegment = (hundreds == 0) ? blankSegment : digitSegments[hundreds];
  uint8_t tensSegment = (tens == 0 && hundreds == 0) ? blankSegment : digitSegments[tens];
  uint8_t onesSegment = digitSegments[ones];

  uint8_t data[] = { letterP, hundredsSegment, tensSegment, onesSegment };
  display.setSegments(data);
}

void initializeEEPROM() {
  int water_EEPROM_Value = 0;
  int soap_EEPROM_Value = 0;
  int blower_EEPROM_Value = 0;

  EEPROM.get(0, water_EEPROM_Value);
  EEPROM.get(1, soap_EEPROM_Value);
  EEPROM.get(2, blower_EEPROM_Value);

  if (water_EEPROM_Value == -1 || water_EEPROM_Value == 0) {
    water_EEPROM_Value = 60;
  }
  if (soap_EEPROM_Value == -1 || soap_EEPROM_Value == 0) {
    soap_EEPROM_Value = 30;
  }
  if (blower_EEPROM_Value == -1 || blower_EEPROM_Value == 0) {
    blower_EEPROM_Value = 60;
  }
}

void writeToEEPROM(int value, int address) {
  EEPROM.update(address, value);

  // Check update result. Force update if not reflected.
  if (address == 0) {
    int current_EEPROM_value = 0;
    EEPROM.get(0, current_EEPROM_value);
    if (current_EEPROM_value != waterSecondsValue) {
      // Force write to water address 0.
      EEPROM.put(0, waterSecondsValue);
      Serial.println("Forced written to water address 0.");
    } else {
      Serial.println("Water value matched.");
    }
  } else if (address == 1) {
    int current_EEPROM_value = 0;
    EEPROM.get(1, current_EEPROM_value);
    if (current_EEPROM_value != soapSecondsValue) {
      // Force write to water address 0.
      EEPROM.put(1, soapSecondsValue);
      Serial.println("Forced written to soap address 1.");
    } else {
      Serial.println("Soap value matched.");
    }
  } else if (address == 2) {
    int current_EEPROM_value = 0;
    EEPROM.get(2, current_EEPROM_value);
    if (current_EEPROM_value != blowerSecondsValue) {
      // Force write to water address 0.
      EEPROM.put(2, blowerSecondsValue);
      Serial.println("Forced written to blower address 2.");
    } else {
      Serial.println("Blower value matched.");
    }
  }
}

void setup() {
  Serial.begin(9600);
  attachInterrupt(digitalPinToInterrupt(coinPulsePin), coinpulse, RISING);

  // RELAYS
  pinMode(water_relay, OUTPUT);
  pinMode(soap_relay, OUTPUT);
  pinMode(blower_relay, OUTPUT);
  pinMode(dispenser_relay, OUTPUT);
  pinMode(coinAcceptor_powerOff_relay, OUTPUT);
  pinMode(buzzerPin, OUTPUT);

  display.setBrightness(7);
  initializeEEPROM();
}

void loop() {
  if (isBooting) {
    uint8_t dashes[] = { 0x40, 0x40, 0x40, 0x40 };
    display.setSegments(dashes);
    if (waterSecondsValue == 0 || waterSecondsValue == -1 || soapSecondsValue == 0 || soapSecondsValue == -1 || blowerSecondsValue == 0 || blowerSecondsValue == -1) {
      // Re-initialize EEPROM values when it fails to get all the corresponding values.
      initializeEEPROM();
    }
    delay(3000);
    isBooting = false;
    insertedCoins = 0;
    totalSeconds = 0;
    setDisplayMoney(0);

  } else {
    if (isCoinInserted) {
      insertedCoins++;
      isCoinInserted = false;
      Serial.print("Inserted coin: ");
      Serial.println(insertedCoins);
      setDisplayMoney(insertedCoins);
    }
  }
}
