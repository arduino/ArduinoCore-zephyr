# Infineon PSOC™ Edge Zephyr-based Arduino Core
[![Default branch status](https://github.com/Infineon/ArduinoCore-zephyr/actions/workflows/package_core.yml/badge.svg?branch=main&event=push)](https://github.com/Infineon/ArduinoCore-zephyr/actions/workflows/package_core.yml)

> [!IMPORTANT]
> This project is a work in progress.
> It does not yet cover the full Arduino API or all PSOC™ Edge features.

This repository contains the Zephyr-based Arduino core for:

- [Infineon KIT-PSE84-AI (PSOC™ Edge E84)](https://www.infineon.com/cms/en/product/evaluation-boards/kit-pse84-ai/)

You can install the core through the Boards Manager in Arduino IDE, or via the Arduino CLI.
After installing the core, the **loader has to be flashed** and your first sketch can be uploaded. Steps are provided for both, [Arduino IDE](#installation--usage-in-arduino-ide) and [Arduino CLI](#installation--usage-in-arduino-cli).

## Installation & Usage in Arduino IDE

### Installation

Use Arduino IDE 2.x and install the platform through Arduino's Boards Manager.

1. Open Arduino IDE.
2. Navigate to *'File > Preferences'*.
3. In Additional boards manager URLs, add:

```text
https://github.com/Infineon/ArduinoCore-zephyr/releases/latest/download/package_infineon_pse84_index.json
```

4. Open *'Boards Manager'* (left side menu).
5. Search for *'PSOC Edge'* and install `Infineon PSOC Edge Boards`. This may take a moment.

### Usage

1. Select the PSOC™ Edge board and the correct serial port: `Tools > Board > Infineon PSOC Edge Boards > Infineon KIT-PSE84-AI (PSOC Edge E84)` and `Tools > Port`.
2. Flash the loader: `Tools > Burn Bootloader`.
3. Open or write a sketch and click *Upload*.

## Installation & Usage in Arduino CLI

### Installation

Simply use the following command:

```bash
arduino-cli core install infineon:zephyr_pse84 --additional-urls https://github.com/Infineon/ArduinoCore-zephyr/releases/latest/download/package_infineon_pse84_index.json
```

### Usage

The FQBN for the Infineon KIT-PSE84-AI (PSOC Edge E84) is `infineon:zephyr_pse84:kit_pse84_ai`.

1. Flash the loader:

```bash
arduino-cli burn-bootloader -b infineon:zephyr_pse84:kit_pse84_ai
```

2. Compile and upload a sketch:

```bash
arduino-cli compile -b infineon:zephyr_pse84:kit_pse84_ai MySketch
arduino-cli upload -b infineon:zephyr_pse84:kit_pse84_ai -p <port> MySketch
```

## Arduino API Documentation for PSOC™ Edge

### Pinout

![Pinout](doc/KIT_PSE84_AI_Pinout.svg)

### Macros

For the exact macro definitions, refer to [`variants/kit_pse84_ai_pse846gps2dbzc4a_m33/variant.h`](variants/kit_pse84_ai_pse846gps2dbzc4a_m33/variant.h).
Here is a general explanation:

- Every exposed GPIO pin from the pinout has a macro identical to the documentation name, e.g. `P17_3`, `P16_0`, `P15_3`, etc.
- Digital pins have a macro in the format `Dx`, where `x` is the index of the arduino pin in the range [0, 47], e.g. `D0`, `D15`, `D47`, etc.
- Analog pins have a macro in the format `Ax`, where `x` is the index of the arduino pin in the range [0, 15], e.g. `A0`, `A1`, `A15`, etc.
- Onboard LED pin name macros are:
  - `LED_BUILTIN       `: `P10_7`
  - `LED_BUILTIN_1     `: `P10_7`
  - `LED_BUILTIN_2     `: `P10_5`
  - `LED_BUILTIN_ACTIVE`: `HIGH`
  - `LED_RED           `: `P20_6`
  - `LED_GREEN         `: `P20_4`
  - `LED_BLUE          `: `P20_5`
  - `BTN_BUILTIN       `: `P7_0` (`SW1`)
- I2C pins are defined as `SDA`/`SCL` and `SDA1`/`SCL1` for the internal (`Wire`), and external (`Wire1`) I2C bus respectively.
- Expansion header pins include the macros as printed on the board: `SERIAL_INTx` where `x` is the index in the range [0, 3], e.g. `SERIAL_INT0`.

### PSOC™ Edge Arduino API Coverage

> [!IMPORTANT]
> Not all Arduino API calls are currently supported. Check the tracking document before relying on a specific API.

Refer to the [API coverage tracking document](./api_support_tracking.md) for the implementation status and known limitations of each Arduino API call.

## Known Scope And Limits

- This is an early PSOC™ Edge port focused on enabling core Arduino workflows on Zephyr.
- API compatibility is incomplete and may change between releases.
- Some subsystems compile but are not yet fully validated on KIT-PSE84-AI.

## Contributing And Feedback

Feedback and contributions are highly encouraged.

### Feedback

- Report bugs and request features in [Issues](/../../issues)
- Ask questions and discuss roadmap in [Discussions](/../../discussions)

When reporting issues, please include:

- Board and host OS
- Core version
- Minimal sketch to reproduce
- Full build/upload logs

### Contributing
Contributions are welcome through [Pull Requests](/../../pulls). Please refer to [CONTRIBUTING.md](./CONTRIBUTING.md) for more information.

## Troubleshooting

### Common Issues

#### `llext` Undefined Symbol Errors

If upload succeeds but execution fails with an `Undefined symbol` error, the sketch is using a symbol not exported by the loader image for this build.

- Rebuild and flash the bootloader for this board.
- If needed, extend exported symbols in loader integration sources and rebuild.

---

#### **Q: I get an OS crash, like `<err> os: ***** USAGE FAULT *****`**
**A:** This is usually due to a buffer overflow or coding error in the user's own code. However, since the project is still in beta 🧪, a [good bug report](/../../issues) could help identify any issues in our code.