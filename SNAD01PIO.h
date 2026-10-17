/* RP2040 PIO backend for the Sonix SNAD01. See README for revision limits. */
#ifndef SNAD01_PIO_H
#define SNAD01_PIO_H

#include "SNAD01.h"
#if defined(ARDUINO_ARCH_RP2040)
#include <hardware/pio.h>

// For the Earle Philhower Arduino-Pico core. Pins are RP2040 GPIO numbers.
// Independent implementation: existing SNAD01 remains the portable backend.
class SNAD01PIO {
public:
  enum Control {
    ExternalReference = SNAD01::ExternalReference,
    InternalReference = SNAD01::InternalReference,
    PullDown = SNAD01::PullDown,
    PullUp = SNAD01::PullUp
  };
  SNAD01PIO(uint8_t dio, uint8_t clk, uint8_t start);
  ~SNAD01PIO();
  SNAD01PIO(const SNAD01PIO &) = delete;
  SNAD01PIO &operator=(const SNAD01PIO &) = delete;

  // False if GPIOs are invalid/duplicated, or no PIO resources are available.
  bool begin(uint8_t control = ExternalReference,
             uint8_t analogMask = 0xFF, uint8_t wakeupMask = 0);
  bool init(uint8_t control, uint8_t analogMask, uint8_t wakeupMask);
  void end();
  bool ready() const { return pio_ != nullptr; }
  uint8_t Convert(uint8_t channel);
  uint8_t getDigital();
  void PWRDN();
  bool wakeupPending() const;
  void wakeUp();

private:
  uint8_t dio_, clk_, start_, control_;
  bool sleeping_;
  PIO pio_;
  uint sm_, offset_;
  uint8_t transfer(uint16_t bits, uint8_t count, uint8_t wait,
                   uint8_t read, bool keepSelected = false);
  void writeControl(uint8_t control);
};
#endif
#endif
