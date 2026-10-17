#include "Arduino.h"
#include "SNAD01.h"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

struct Frame { std::string sent; unsigned clocks = 0; unsigned reads = 0; };
static std::vector<Frame> frames;
static int pins[16] = {}, modes[16] = {};
static bool active = false;
static unsigned calls = 0, readIndex = 0;
static uint8_t response = 0;
void digitalWrite(uint8_t pin, uint8_t value) {
  ++calls;
  if (pin == 10 && pins[pin] != value) {
    if (value == LOW) { frames.push_back(Frame()); active = true; }
    else active = false;
  }
  if (pin == 9 && value == HIGH && pins[pin] == LOW && active) {
    ++frames.back().clocks;
    if (modes[8] == OUTPUT) frames.back().sent += pins[8] ? '1' : '0';
  }
  pins[pin] = value;
}
void pinMode(uint8_t pin, uint8_t mode) { ++calls; modes[pin] = mode; }
int digitalRead(uint8_t pin) {
  assert(pin == 8 && modes[pin] == INPUT);
  assert(pins[9] == LOW); // sample after complete pulse
  Frame &f = frames.back();
  if (f.sent.substr(0, 3) == "100") assert(f.clocks == 9 + f.reads);
  if (f.sent == "101") assert(f.clocks == 4 + f.reads);
  ++f.reads;
  return (response >> (7 - (readIndex++ % 8))) & 1;
}
void delayMicroseconds(unsigned int us) { assert(us >= 5); }
static void queue(uint8_t value) { response = value; readIndex = 0; }
static void idle() { assert(pins[10] == HIGH && pins[9] == LOW && modes[8] == INPUT); }
int main() {
  SNAD01 adc(8, 9, 10);
  assert(calls == 0); // safe global construction
  adc.init(0x90, 0xA5, 0x12);
  assert(frames.size() == 3);
  assert(frames[0].sent == "0111001");
  assert(frames[1].sent == "00110100101");
  assert(frames[2].sent == "01000010010");
  idle();
  for (uint8_t ch = 0; ch < 8; ++ch) {
    for (unsigned value = 0; value < 256; ++value) {
      queue(value);
      assert(adc.Convert(ch) == value);
      const Frame &f = frames.back();
      std::string channel;
      for (int bit = 2; bit >= 0; --bit) channel += ((ch >> bit) & 1) ? '1' : '0';
      assert(f.sent == "100" + channel);
      assert(f.clocks == 16 && f.reads == 8); // 3 command, 3 channel, 2 wait, 8 data
      idle();
    }
  }
  unsigned before = calls;
  assert(adc.Convert(8) == 0 && adc.Convert(255) == 0 && calls == before);
  for (unsigned value = 0; value < 256; ++value) {
    queue(value);
    assert(adc.getDigital() == value);
    assert(frames.back().sent == "101" && frames.back().clocks == 11);
    idle();
  }
  adc.PWRDN();
  assert(frames[frames.size()-2].sent == "0111000"); // preserve pull-up
  assert(frames.back().sent == "000" && frames.back().clocks == 11);
  assert(pins[10] == LOW && modes[8] == INPUT);
  before = calls;
  adc.PWRDN();
  assert(calls == before);
  queue(0); assert(!adc.wakeupPending());
  queue(0x80); assert(adc.wakeupPending());
  adc.wakeUp();
  assert(frames.back().sent == "0111001");
  idle();
  adc.PWRDN(); queue(0x69);
  assert(adc.Convert(2) == 0x69); // implicit wake restores reference
  assert(frames[frames.size()-2].sent == "0111001");
  adc.begin(0xF0, 0, 0);
  assert(frames[frames.size()-3].sent == "0110011"); // prohibited pull pair removed
  std::cout << "Protocol tests passed: all 8 channels x 256 ADC values, 256 digital values, init, invalid channels, sleep/wakeup.\n";
}
