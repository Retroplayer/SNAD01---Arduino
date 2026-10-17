#include "SNAD01PIO.h"
#if defined(ARDUINO_ARCH_RP2040)
#include <hardware/clocks.h>
#include <pico/time.h>
#include <hardware/gpio.h>
#include <hardware/pio_instructions.h>
#include "SNAD01.pio.h"

SNAD01PIO::SNAD01PIO(uint8_t dio, uint8_t clk, uint8_t start)
    : dio_(dio), clk_(clk), start_(start), control_(0), sleeping_(false),
      pio_(nullptr), sm_(0), offset_(0) {}

SNAD01PIO::~SNAD01PIO() { end(); }

bool SNAD01PIO::begin(uint8_t control, uint8_t analogMask, uint8_t wakeupMask) {
  if (dio_ > 29 || clk_ > 29 || start_ > 29 || dio_ == clk_ ||
      dio_ == start_ || clk_ == start_) return false;
  if (!pio_) {
    // Claim without panicking; preserve the portable backend if PIO is busy.
    const PIO blocks[] = {pio0, pio1};
    for (PIO block : blocks) {
      if (!pio_can_add_program(block, &snad01_program)) continue;
      int candidate = pio_claim_unused_sm(block, false);
      if (candidate < 0) continue;
      pio_ = block;
      sm_ = static_cast<uint>(candidate);
      offset_ = pio_add_program(pio_, &snad01_program);
      break;
    }
    if (!pio_) return false;

    pio_sm_config config = snad01_program_get_default_config(offset_);
    sm_config_set_out_pins(&config, dio_, 1);
    sm_config_set_in_pins(&config, dio_);
    sm_config_set_set_pins(&config, start_, 1);
    sm_config_set_sideset_pins(&config, clk_);
    // All FIFO words are shifted left. Header is packed accordingly below.
    sm_config_set_out_shift(&config, false, false, 32);
    sm_config_set_in_shift(&config, false, false, 32);
    sm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / 1000000.0f);
    pio_sm_init(pio_, sm_, offset_, &config);
    const uint32_t outputs = (1u << clk_) | (1u << start_);
    pio_sm_set_pins_with_mask(pio_, sm_, 1u << start_,
                              outputs | (1u << dio_));
    pio_sm_set_pindirs_with_mask(pio_, sm_, outputs, outputs | (1u << dio_));
    // Configure latches/direction before handing pins to PIO.
    gpio_disable_pulls(dio_);
    pio_gpio_init(pio_, dio_);
    pio_gpio_init(pio_, clk_);
    pio_gpio_init(pio_, start_);
    pio_sm_set_enabled(pio_, sm_, true);
  }
  if (sleeping_) wakeUp();
  control_ = control & 0xF0;
  if ((control_ & 0xC0) == 0xC0) control_ &= 0x30;
  writeControl(control_);
  transfer((1u << 8) | analogMask, 11, 0, 0);
  transfer((2u << 8) | wakeupMask, 11, 0, 0);
  return true;
}

bool SNAD01PIO::init(uint8_t control, uint8_t analogMask, uint8_t wakeupMask) {
  return begin(control, analogMask, wakeupMask);
}

uint8_t SNAD01PIO::transfer(uint16_t bits, uint8_t count, uint8_t wait,
                           uint8_t read, bool keepSelected) {
  // Left-shifting OSR: the three 5-bit fields start at bits 31,26,21.
  const uint32_t header = (uint32_t(count - 1) << 27) |
                          (uint32_t(wait) << 22) | (uint32_t(read) << 17);
  const uint32_t payload = 0x80000000u | (uint32_t(bits) << (31 - count));
  pio_sm_put_blocking(pio_, sm_, header);
  pio_sm_put_blocking(pio_, sm_, payload);
  // OUT EXEC also shifts left, so place the instruction in the upper half.
  pio_sm_put_blocking(pio_, sm_,
      uint32_t(pio_encode_set(pio_pins, keepSelected ? 0 : 1)) << 16);
  return static_cast<uint8_t>(pio_sm_get_blocking(pio_, sm_));
}

void SNAD01PIO::writeControl(uint8_t control) {
  transfer((3u << 4) | (control >> 4), 7, 0, 0);
}

uint8_t SNAD01PIO::Convert(uint8_t channel) {
  if (!ready() || channel > 7) return 0;
  wakeUp();
  return transfer((4u << 3) | channel, 6, 2, 8);
}

uint8_t SNAD01PIO::getDigital() {
  if (!ready()) return 0;
  wakeUp();
  return transfer(5, 3, 0, 8);
}

void SNAD01PIO::PWRDN() {
  if (!ready() || sleeping_) return;
  writeControl(control_ & 0xC0);
  transfer(0, 3, 8, 0, true);
  sleeping_ = true;
}

bool SNAD01PIO::wakeupPending() const {
  return ready() && sleeping_ && gpio_get(dio_);
}

void SNAD01PIO::wakeUp() {
  if (!ready() || !sleeping_) return;
  // SM is stalled at PULL with CLK LOW. SET uses its configured START pin.
  pio_sm_exec(pio_, sm_, pio_encode_set(pio_pins, 1));
  busy_wait_us_32(5);
  sleeping_ = false;
  writeControl(control_);
}

void SNAD01PIO::end() {
  if (!ready()) return;
  pio_sm_set_enabled(pio_, sm_, false);
  pio_sm_set_pins_with_mask(pio_, sm_, 1u << start_,
                            (1u << start_) | (1u << clk_));
  // Return owned GPIOs to SIO with inactive START/CLK, released DIO.
  gpio_init(dio_);
  gpio_init(clk_);
  gpio_init(start_);
  gpio_put(start_, true);
  gpio_set_dir(start_, GPIO_OUT);
  gpio_set_dir(clk_, GPIO_OUT);
  pio_remove_program(pio_, &snad01_program, offset_);
  pio_sm_unclaim(pio_, sm_);
  pio_ = nullptr;
  sleeping_ = false;
}
#endif
