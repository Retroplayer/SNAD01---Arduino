/* Original by Scott McDonnell (2013), based on Tod E. Kurt's SoftI2CMaster. */
#include <Arduino.h>
#include "SNAD01.h"

SNAD01::SNAD01(uint8_t dio, uint8_t clk, uint8_t start)
    : dio_(dio), clk_(clk), start_(start), control_(0), sleeping_(false) {}

void SNAD01::begin(uint8_t control, uint8_t analogMask, uint8_t wakeupMask) {
  // Set output latches before enabling drivers to avoid a START glitch.
  digitalWrite(start_, HIGH);
  pinMode(start_, OUTPUT);
  digitalWrite(clk_, LOW);
  pinMode(clk_, OUTPUT);
  releaseData();
  sleeping_ = false;
  control_ = control & 0xF0;
  // Both input pull resistors enabled is prohibited; disable both if requested.
  if ((control_ & 0xC0) == 0xC0) control_ &= 0x30;
  writeRegister(3, control_ >> 4, 4);
  writeRegister(1, analogMask, 8);
  writeRegister(2, wakeupMask, 8);
}

void SNAD01::init(uint8_t control, uint8_t analogMask, uint8_t wakeupMask) {
  begin(control, analogMask, wakeupMask);
}

void SNAD01::releaseData() {
  pinMode(dio_, INPUT);
  digitalWrite(dio_, LOW); // disable host pull-up; ADC drives read data
}

void SNAD01::startTransaction() {
  wakeUp();
  digitalWrite(start_, LOW);
  delayMicroseconds(HalfClockUs);
  digitalWrite(dio_, LOW);
  pinMode(dio_, OUTPUT);
}

void SNAD01::endTransaction() {
  digitalWrite(start_, HIGH);
  releaseData();
  delayMicroseconds(HalfClockUs);
}

void SNAD01::clock() {
  digitalWrite(clk_, HIGH);
  delayMicroseconds(HalfClockUs);
  digitalWrite(clk_, LOW);
  delayMicroseconds(HalfClockUs);
}

void SNAD01::writeBits(uint8_t value, uint8_t count) {
  while (count) {
    --count;
    digitalWrite(dio_, (value >> count) & 1);
    delayMicroseconds(HalfClockUs);
    clock();
  }
}

uint8_t SNAD01::readByte() {
  uint8_t value = 0;
  for (uint8_t i = 0; i < 8; ++i) {
    // Datasheet example clocks HIGH then LOW before sampling DIO.
    clock();
    value = (value << 1) | (digitalRead(dio_) != LOW);
  }
  return value;
}

void SNAD01::writeRegister(uint8_t command, uint8_t value, uint8_t count) {
  startTransaction();
  writeBits(command, 3);
  writeBits(value, count);
  endTransaction();
}

uint8_t SNAD01::Convert(uint8_t channel) {
  if (channel > 7) return 0;
  startTransaction();
  writeBits(4, 3);
  writeBits(channel, 3);
  releaseData();
  clock(); // two acquisition/turnaround clocks (datasheet section 6.4)
  clock();
  const uint8_t result = readByte();
  endTransaction();
  return result;
}

uint8_t SNAD01::getDigital() {
  startTransaction();
  writeBits(5, 3);
  releaseData();
  const uint8_t result = readByte();
  endTransaction();
  return result;
}

void SNAD01::PWRDN() {
  if (sleeping_) return;
  // Preserve PH/PL, but disable RF/MB for low standby current (section 6.6).
  writeRegister(3, (control_ & 0xC0) >> 4, 4);
  startTransaction();
  writeBits(0, 3);
  releaseData();
  // Follow the vendor example: eight additional clocks after the command.
  for (uint8_t i = 0; i < 8; ++i) clock();
  sleeping_ = true; // START must remain LOW throughout power-down
}

bool SNAD01::wakeupPending() const {
  return sleeping_ && digitalRead(dio_) != LOW;
}

void SNAD01::wakeUp() {
  if (!sleeping_) return;
  sleeping_ = false;
  endTransaction();
  writeRegister(3, control_ >> 4, 4);
}
