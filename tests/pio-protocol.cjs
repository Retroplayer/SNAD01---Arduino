// Execute the checked-in assembled PIO program in rp2040js, not a mock driver.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { RP2040 } = require('rp2040js');
const source = fs.readFileSync(path.join(__dirname, '../SNAD01.pio.h'), 'utf8');
const instructions = [...source.split('snad01_program_instructions[] = {')[1]
  .split('};')[0].matchAll(/0x([a-f0-9]+),/g)].map(m => parseInt(m[1], 16));
assert(instructions.length <= 32);
const chip = new RP2040();
const pio = chip.pio[0], sm = pio.machines[0];
for (const pin of [8, 9, 10]) chip.gpio[pin].ctrl = 6; // PIO0
pio.instructions.set(instructions);
chip.gpio[8].padValue |= 0x40; // input enable, as gpio_init/pio_gpio_init configure
sm.execCtrl = (instructions.length - 1) << 12; // wrap top, bottom=0
sm.shiftCtrl = 0; // shift left, no auto push/pull
sm.pinCtrl = (1 << 29) | (1 << 26) | (1 << 20) | (8 << 15) | (9 << 10) | (10 << 5) | 8;
pio.pinDirections = (1 << 9) | (1 << 10);
pio.pinValues = 1 << 10;
let count = 0;
function transfer(bits, writeCount, waitCount, readCount, response = 0, sleep = false) {
  // Same three-word wire format consumed by SNAD01PIO::transfer.
  sm.writeFIFO(((writeCount - 1) << 27) | (waitCount << 22) | (readCount << 17));
  sm.writeFIFO((0x80000000 | (bits << (31 - writeCount))) >>> 0);
  sm.writeFIFO(((0xe000 | (sleep ? 0 : 1)) << 16) >>> 0);
  let clocks = 0, sent = '', samples = 0, steps = 0, highSince = 0, lowSince = 0;
  while (sm.rxFIFO.empty && ++steps < 1000) {
    const before = pio.pinValues;
    const reading = (instructions[sm.pc] >>> 13) === 2;
    if (reading) {
      assert.equal(pio.pinDirections & (1 << 8), 0, 'DIO released before IN');
      assert.equal(pio.pinValues & (1 << 9), 0, 'sample with CLK low');
      assert.equal(clocks, writeCount + waitCount + samples + 1, 'sample position');
      chip.gpio[8].setInputValue(!!((response >> (7 - samples)) & 1));
      samples++;
    }
    const cycle = sm.cycles;
    sm.step();
    const after = pio.pinValues;
    if (!(before & (1 << 9)) && (after & (1 << 9))) {
      assert.equal(after & (1 << 10), 0, 'START asserted for every clock');
      assert(cycle - lowSince >= 5, 'minimum low half-cycle');
      highSince = cycle;
      clocks++;
      if (pio.pinDirections & (1 << 8)) sent += (after & (1 << 8)) ? '1' : '0';
      else assert(clocks > writeCount, 'DIO released only after command');
    }
    if ((before & (1 << 9)) && !(after & (1 << 9))) {
      assert(cycle - highSince >= 5, 'minimum high half-cycle');
      lowSince = cycle;
    }
  }
  assert(steps < 1000, 'completion without deadlock');
  assert.equal(sm.readFIFO(), response);
  assert.equal(sent, bits.toString(2).padStart(writeCount, '0'));
  assert.equal(clocks, writeCount + waitCount + readCount);
  assert.equal(samples, readCount);
  assert.equal(pio.pinDirections & (1 << 8), 0);
  assert.equal(pio.pinValues & (1 << 9), 0);
  assert.equal(!!(pio.pinValues & (1 << 10)), !sleep);
  assert(sm.txFIFO.empty);
  count++;
}
for (let control = 0; control < 16; control++) transfer((3 << 4) | control, 7, 0, 0);
for (let mask = 0; mask < 256; mask++) {
  transfer((1 << 8) | mask, 11, 0, 0);
  transfer((2 << 8) | mask, 11, 0, 0);
  transfer(5, 3, 0, 8, mask);
  for (let channel = 0; channel < 8; channel++) transfer((4 << 3) | channel, 6, 2, 8, mask);
}
transfer(0, 3, 8, 0, 0, true);
// A forced SET acknowledges sleep without clocking DIO; next frame still works.
sm.executeInstruction(0xe001);
transfer((3 << 4) | 1, 7, 0, 0);
transfer((4 << 3) | 7, 6, 2, 8, 0xa5);
console.log(`PIO emulator: ${count} transactions passed (registers, all channels/bytes, digital, sleep/wake, timing).`);
