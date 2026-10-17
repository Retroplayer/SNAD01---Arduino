/* SONIX SNAD01 serial ADC library.
 * Original by Scott McDonnell (2013), based on SoftI2CMaster by Tod E. Kurt.
 */
#ifndef SNAD01_H
#define SNAD01_H

#include <stdint.h>

class SNAD01 {
public:
  // Control flags retain the original API's high-nibble representation.
  enum Control {
    ExternalReference = 0x10,
    InternalReference = 0x20,
    PullDown = 0x40,
    PullUp = 0x80
  };

  // Construction does not touch hardware; call init/begin from setup().
  SNAD01(uint8_t dio, uint8_t clk, uint8_t start);
  void begin(uint8_t control = ExternalReference,
             uint8_t analogMask = 0xFF, uint8_t wakeupMask = 0);
  void init(uint8_t control, uint8_t analogMask, uint8_t wakeupMask);
  uint8_t Convert(uint8_t channel); // 0..7; invalid channels return 0, no bus activity
  uint8_t getDigital();            // bit n corresponds to channel n
  void PWRDN();                    // mode 0; START stays LOW until wakeUp()
  bool wakeupPending() const;      // DIO HIGH signals a wakeup in mode 0
  void wakeUp();                   // end power-down and restore reference settings

private:
  uint8_t dio_, clk_, start_, control_;
  bool sleeping_;
  static const uint8_t HalfClockUs = 5; // <=100 kHz, including GPIO overhead
  void startTransaction();
  void endTransaction();
  void releaseData();
  void clock();
  void writeBits(uint8_t value, uint8_t count); // right-aligned, MSB first
  uint8_t readByte();
  void writeRegister(uint8_t command, uint8_t value, uint8_t count);
};
#endif
