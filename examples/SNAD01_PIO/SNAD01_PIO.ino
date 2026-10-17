#include <SNAD01PIO.h>

// Pico GPIO numbers. Any three distinct exposed GPIOs may be used.
SNAD01PIO adc(8, 9, 10); // DIO, CLK, START

void setup() {
  Serial.begin(115200);
  if (!adc.begin(SNAD01PIO::ExternalReference, 0xFF, 0)) {
    // Invalid pin assignment, or PIO instruction memory/state machines busy.
    while (true) {
      Serial.println("Could not initialize SNAD01 PIO");
      delay(1000);
    }
  }
}

void loop() {
  for (uint8_t channel = 0; channel < 8; ++channel) {
    Serial.print(channel);
    Serial.print(": ");
    Serial.println(adc.Convert(channel));
  }
  delay(100);
}
