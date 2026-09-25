# Arduino core test configuration

Configuration for the shared
[`arduino-core-tests`](https://github.com/Infineon/arduino-core-tests) suite
(vendored at [`arduino_core_tests`](arduino_core_tests)) lives
in a single file, `tests/test_config.h`, exactly where that submodule's own
Makefile expects it (`../../tests/test_config.h`, relative to the submodule).

One file, with a `#if defined(ARDUINO_<BOARD>)` block per board.
`arduino-cli` already defines `ARDUINO_{build.board}` for every compile, so
the right block is selected automatically.

## Adding a board

1. Add a `#if defined(ARDUINO_<BOARD>)` block to `tests/test_config.h`, using
   the board's `build.board` value from `boards.txt` (e.g. `KIT_PSE84_AI`).
2. Define the pins required by the selected tests, using Arduino pin numbers
   (e.g. `D0`), not MCU port/pin names.
3. Verify each Arduino pin against the board's variant overlay
   (`variants/<variant>/*.overlay`, `zephyr,user` / `digital-pin-gpios`) and
   the board schematic or official pinout.
4. Document any required jumpers or external connections next to the block.

For example, `test_digitalio_single` requires two physically connected GPIOs:

```c
#if defined(ARDUINO_<BOARD>)
#define TEST_PIN_DIGITAL_IO_OUTPUT <output pin>
#define TEST_PIN_DIGITAL_IO_INPUT <input pin>
#endif
```

## Running a test

```bash
make -C tests/arduino_core_tests FQBN=<fqbn> UNITY_PATH=Unity \
  TESTS=-DTEST_DIGITALIO_SINGLE test_digitalio_single compile
```

To flash and run on a single connected board, add `PORT=` and
`ENABLE_SYNC=0`:

```bash
make -C tests/arduino_core_tests FQBN=<fqbn> UNITY_PATH=Unity PORT=<port> \
  TESTS=-DTEST_DIGITALIO_SINGLE ENABLE_SYNC=0 test_digitalio_single
```

`ENABLE_SYNC` defaults to `1`: tests named `*_connected2_*` (e.g.
`test_wire_connected2_masterpingpong`) need two boards and wait for each
other's handshake over serial, so leave it enabled for those. Tests named
`*_single` (or loopback tests like `test_spi_connected1_loopback`) only use
one board; without `ENABLE_SYNC=0` they hang forever printing
`synchronising with host...`, since nothing ever answers the handshake.
