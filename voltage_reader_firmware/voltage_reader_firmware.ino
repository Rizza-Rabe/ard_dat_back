const int voltagePin = A0; // Pin connected to the voltage divider output
const float referenceVoltage = 5.0; // Reference voltage of the Arduino (usually 5V)
const float R1 = 10000; // 10k ohms
const float R2 = 1000;  // 1k ohms
const float dividerRatio = (R1 + R2) / R2;

float readBatteryVoltage() {
  int sensorValue = analogRead(voltagePin);
  float voltage = sensorValue * (referenceVoltage / 1023.0) * dividerRatio;
  return voltage;
}

int voltageToPercentage(float voltage) {
  if (voltage >= 12.6) return 100;
  if (voltage >= 12.4) return 90;
  if (voltage >= 12.2) return 80;
  if (voltage >= 12.0) return 70;
  if (voltage >= 11.8) return 60;
  if (voltage >= 11.6) return 50;
  if (voltage >= 11.4) return 40;
  if (voltage >= 11.2) return 30;
  if (voltage >= 11.0) return 20;
  if (voltage >= 10.8) return 10;
  return 0;
}

void setup() {
  Serial.begin(9600);
}

void loop() {
  float batteryVoltage = readBatteryVoltage();
  int batteryPercentage = voltageToPercentage(batteryVoltage);
  
  Serial.print("Battery Voltage: ");
  Serial.print(batteryVoltage);
  Serial.print(" V, Battery Percentage: ");
  Serial.print(batteryPercentage);
  Serial.println(" %");

  delay(1000); // Read every second
}
