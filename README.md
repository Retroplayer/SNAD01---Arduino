# SNAD01 Arduino library

Bit-banged three-wire driver for the Sonix SNAD01 ADC, originally developed
for the Wowwee Elvis animatronic bust by Scott McDonnell, based on
SoftI2CMaster by Tod E. Kurt.

## Usage

Connect DIO, CLK and START to the pins passed to the constructor, provide a
common ground and the correct supply/reference voltages for your device.
Check host GPIO voltage compatibility. This library assumes it owns these
three signals; do not drive the bus concurrently from the bust's controller.

```cpp
#include <SNAD01.h>
SNAD01 adc(8, 9, 10); // DIO, CLK, START

void setup() {
  adc.begin(SNAD01::ExternalReference, 0xFF, 0x00);
}

void loop() {
  uint8_t sample = adc.Convert(0);
  // Use sample here.
}
```

Install this directory as an Arduino library, or pass it with Arduino CLI:

```sh
arduino-cli core install arduino:avr@1.8.6
arduino-cli compile --fqbn arduino:avr:uno --library "$PWD" examples/SNAD01
```

## Configuration and compatibility

The existing `init(control, analogMask, wakeupMask)`, `Convert(channel)`,
`getDigital()` and `PWRDN()` methods remain available. `begin()` is an alias
for initialization with defaults (external reference, all analog, no wakeup).
Call either initialization method from `setup()` before other operations:
construction now only stores pins, so global instances do not touch GPIO
before the Arduino runtime initializes.

Control values retain the original **high nibble** convention: `0x10` for
external reference, `0x20` for internal bandgap. Combine with `0x80` for
pull-up or `0x40` for pull-down using `|` (named flags are in `SNAD01`).
Requesting both pull resistors disables both, since that combination is
prohibited. The low nibble is ignored. Reference selection must match your
hardware; the library does not validate or measure the reference voltage.

Each analog-mask bit selects analog (1) or digital (0). Each wakeup-mask bit
enables wakeup (1) or disables it (0), effective only for digital channels.
`getDigital()` returns bit n for channel n; analog channels read as zero.
`Convert()` returns an unsigned 8-bit sample. Channels outside 0..7 return
zero without touching the bus; zero is also a valid measurement.

`PWRDN()` disables RF/MB while retaining pull settings, sends mode 0, releases
DIO, and leaves START LOW. `wakeupPending()` reports a HIGH wakeup indication.
Call `wakeUp()` to acknowledge it and restore the configured reference, or
make a conversion/digital read (which wakes automatically). After changing
reference or waking, allow analog/reference circuitry to settle as required
by your hardware before trusting measurements. The API is blocking and is
not safe for concurrent calls or use from interrupt handlers sharing the bus.

## Datasheet corrections and implementation

The supplied file named `SNAD01A.pdf` actually identifies itself as **SNAD01C,
version 1.5, July 31, 2003**. It explicitly notes revision differences in
standby behavior. This implementation follows that document, sections
6.1–6.6 and the vendor example routines; verify compatibility with the chip
in your bust, especially power-down and channel 7.

- ADC command: `100`, three channel bits, release DIO, two turnaround clocks,
  then eight data clocks, MSB first. The old code drove DIO during one of the
  turnaround clocks.
- Data is sampled after a complete HIGH/LOW clock pulse, matching the vendor
  example. Both half cycles have explicit 5 microsecond delays, so GPIO speed
  cannot push the interface above the documented 500 kHz limit.
- Power-down keeps START LOW and supplies eight additional clocks after the
  command, following the vendor example. RF/MB are cleared without discarding
  pull settings, and restored on wakeup.
- GPIO uses the portable Arduino API. Unused AVR register pointers, AVR-only
  includes, macros and dead declarations were removed. Each object now stores
  four pin/configuration bytes and a sleep flag; there is no heap allocation.
  This is a correctness and memory cleanup, not a claim of maximum throughput.
- The example switches channel attributes when selecting analog/digital reads.
  It uses `raw * VREF / 256` for nominal 8-bit LSB scaling, with a factor of six
  on CH7 per the supplied SNAD01C datasheet (not the old comment's 1/16).
  This estimates the lower edge of a quantization bin; it is not calibration.

## Tests

From the repository root, run the driver against a host GPIO recorder:

```sh
g++ -std=c++11 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -Itests -I. \
  SNAD01.cpp tests/protocol.cpp -o /tmp/snad01-tests
/tmp/snad01-tests
```

Tests exercise all 256 return values for all eight ADC channels, all digital
byte values, command ordering, bit order, clock counts and sampling positions,
GPIO direction, idle state, initialization, invalid channels, power-down,
wakeup and reference restoration. They do not simulate the analog circuitry
or prove electrical timing. Verify real ADC readings, reference settling,
CH7 divider and wakeup on hardware, ideally with a logic analyzer.
