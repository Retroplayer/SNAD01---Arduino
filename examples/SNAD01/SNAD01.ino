#include <SNAD01.h>

SNAD01 adc(8, 9, 10); // DIO, CLK, START
const float referenceVolts = 3.27f; // measure the voltage on REF

void setup() {
  adc.begin(SNAD01::ExternalReference, 0xFF, 0x00);
  Serial.begin(9600);
  Serial.println(F("Send a for analog readings, d for digital readings."));
}

void loop() {
  if (!Serial.available()) return;
  const char command = Serial.read();
  if (command != 'a' && command != 'd') return;

  // Digital reads require digital attributes. Analog-configured bits read zero.
  adc.begin(SNAD01::ExternalReference, command == 'a' ? 0xFF : 0x00, 0);
  const uint8_t inputs = command == 'd' ? adc.getDigital() : 0;
  for (uint8_t channel = 0; channel < 8; ++channel) {
    Serial.print(F("Channel "));
    Serial.print(channel);
    Serial.print(F(" = "));
    if (command == 'a') {
      const uint8_t raw = adc.Convert(channel);
      // SNAD01C CH7 has a 1/6 input divider; verify your chip revision.
      const float scale = channel == 7 ? 6.0f : 1.0f;
      Serial.println(raw * (referenceVolts / 256.0f) * scale, 3);
    } else {
      Serial.println((inputs >> channel) & 1);
    }
  }
}
