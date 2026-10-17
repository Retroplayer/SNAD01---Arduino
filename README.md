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

## Raspberry Pi Pico PIO backend (Arduino)

On the `pico-pio` branch, `SNAD01PIO` offloads each complete wire transaction
(command, data direction, acquisition clocks and reads) to an RP2040 PIO state
machine. Arduino calls remain synchronous/blocking; this is not an asynchronous
or DMA acquisition API. CPU scheduling can delay transaction submission, but
the PIO engine generates the clock edges. There is no interrupt masking.

Use the **Raspberry Pi Pico/RP2040/RP2350 by Earle F. Philhower** Arduino board
package; this implementation is built and tested for the RP2040 Pico with core
**6.3.0**. The Arduino Mbed core is not supported by this backend. Select
**Raspberry Pi Pico** in Arduino IDE, install this library, and open
**SNAD01_PIO**. The generated PIO header is included, so the IDE does not need
to assemble `.pio` files during a normal sketch build.

```cpp
#include <SNAD01PIO.h>
SNAD01PIO adc(8, 9, 10); // RP2040 GPIO numbers: DIO, CLK, START

void setup() {
  if (!adc.begin(SNAD01PIO::ExternalReference, 0xFF, 0)) {
    // Invalid GPIO assignment or insufficient free PIO resources.
    while (true) delay(1000);
  }
}

void loop() {
  uint8_t sample = adc.Convert(0);
  // Use sample here.
}
```

Pins do not need to be adjacent. Use three distinct exposed GPIOs, not pins
already owned by another peripheral. Pico GPIO is **3.3 V and not 5 V tolerant**;
check the bust's DIO voltage and use suitable bidirectional level conversion
if necessary. Connect a common ground. As with the portable driver, the Pico
must have exclusive control of the ADC interface.

`SNAD01PIO` provides the same initialization, conversion, digital read and
sleep/wakeup method names as `SNAD01`; `begin()` and `init()` additionally return
success/failure. No PIO operation runs before successful initialization, and
reads before initialization return zero. Initialization validates GPIO numbers
0–29 and rejects duplicates; it cannot detect whether a GPIO is exposed or
occupied on your particular board. The original `SNAD01` class still works on
Pico through Arduino GPIO if you prefer the portable implementation.

Each initialized object claims one state machine and **27 instruction slots**
on PIO0 or PIO1, selecting whichever has capacity. Programs are not shared
between instances. `end()` (also called by the destructor) releases the state
machine/program and returns the pins to SIO: START high, CLK low, DIO input.
Objects cannot be copied. Calls and resource allocation must be serialized
across cores/interrupts, and the owning object must remain alive while in use.
Other software must not reconfigure its pins, PIO state machine or instruction
memory. Do not change the system clock after `begin()`; the divider is set
from the current clock at initial PIO allocation. Call `end()` then `begin()`
if the system clock changes.

The PIO instruction clock is 1 MHz, with at least 5 microseconds HIGH and LOW
per wire clock. Clock frequency varies by write/wait/read phase and stays below
100 kHz. A transaction sends three FIFO words and waits for one result/ACK.
DIO is released before acquisition and read clocks. Power-down retains START
low until explicit or implicit wakeup. The same C-datasheet/A-chip compatibility
limits described above apply; this backend does not establish new evidence
about A-revision behavior.

### Reproduce the Pico build

```sh
arduino-cli core update-index --additional-urls \
  https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
arduino-cli core install rp2040:rp2040@6.3.0 --additional-urls \
  https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
arduino-cli compile --fqbn rp2040:rp2040:rpipico --library "$PWD" \
  --build-path /tmp/snad01-pico-build examples/SNAD01_PIO
```

To edit the PIO program, regenerate its header with the `pioasm` shipped in
that board package (tested version 2.3.0), then rerun the build and emulator:

```sh
pioasm SNAD01.pio SNAD01.pio.h
npm --prefix tests ci
npm --prefix tests test
```

The emulator runs the actual assembled instructions using pinned `rp2040js`
1.2.0. It checks 2,835 consecutive transactions: every register mask, every ADC
channel and return byte, every digital byte, sleep/wake, bit order, turnaround,
clock counts, minimum half-cycle durations and DIO direction. This complements
the portable GPIO tests; neither substitutes for a logic analyzer and real ADC
measurements on the Elvis hardware.
