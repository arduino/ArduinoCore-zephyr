# Manual API test sketches

Standalone Arduino sketches that verify PSE84 (KIT-PSE84-AI) core APIs directly
on hardware. Each sketch exercises one API group from
[`api_support_tracking.md`](../../api_support_tracking.md), prints `PASS`/`FAIL`
per function plus a summary line, and repeats every few seconds over the console
`Serial`.

## Layout

One sketch per group, each in its own folder matching its `.ino` name:

    tests/manual_tests/time/time.ino
    tests/manual_tests/digital_io/digital_io.ino
    tests/manual_tests/advanced_io/advanced_io.ino
    tests/manual_tests/advanced_io_shiftout/advanced_io_shiftout.ino
    tests/manual_tests/math/math.ino
    tests/manual_tests/characters/characters.ino
    tests/manual_tests/random/random.ino
    tests/manual_tests/bits_bytes/bits_bytes.ino
    tests/manual_tests/interrupts/interrupts.ino
    tests/manual_tests/serial/serial.ino
    tests/manual_tests/stream/stream.ino
    tests/manual_tests/i2c_controller/i2c_controller.ino
    tests/manual_tests/i2c_target/i2c_target.ino

## Build and flash

    arduino-cli compile --fqbn arduino-git:zephyr:kit_pse84_ai tests/manual_tests/<name>
    arduino-cli upload  -p /dev/ttyACM0 --fqbn arduino-git:zephyr:kit_pse84_ai tests/manual_tests/<name>

## Read the results

Output goes to the console `Serial` (uart2, P6_7 TX / P6_5 RX) at 115200 baud:

    stty -F /dev/ttyACM0 115200 raw -echo
    cat /dev/ttyACM0

Each cycle prints, for example:

    === Time API manual test ===
    PASS millis_advances_100ms
    ...
    4 Tests 0 Failures - OK

## Notes

- `serial` feeds input from the host to test the read-family methods:

      printf 'Hello,World;-42 3.14\n' > /dev/ttyACM0

- `stream` needs a self-loopback jumper on the board: `Serial1 TX (P9_2)` to
  `Serial1 RX (P9_3)`.
- `digital_io` needs a jumper from `D16 (P9_3)` to `D17 (P9_2)`. These pins are
    also used by `Serial1`; do not use `Serial1` during this test.
- `advanced_io` uses the same `D16`-to-`D17` jumper for tone and pulse tests.
    `shiftIn()` tests `D20` with its internal pull-up and pull-down. Do not use
    Serial1 during this test. `pulseInLong()` is reported unavailable because the
    core currently has no implementation.
- `advanced_io_shiftout` captures `shiftOut()` data on D18/D19 using a GPIO
    interrupt; no external wiring is needed.
- `i2c_controller` and `i2c_target` form a two-board loopback. Flash
    `i2c_controller` on one board and `i2c_target` on the other, then connect
    both boards' `Wire` bus (SCB0 on the P8 header): SCL (P8_0) to SCL, SDA
    (P8_1) to SDA, and GND to GND. Add external pull-ups (2.2k-4.7k) from SDA
    and SCL to 1.8V, since the I2C pins are open-drain with internal pull-ups
    disabled. The controller board prints PASS/FAIL; the target address is
    0x30. `setWireTimeout()`, `clearWireTimeoutFlag()`, and
    `getWireTimeoutFlag()` are not implemented in this core.
