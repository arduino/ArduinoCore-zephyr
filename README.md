# PSOC™ Arduino Core for Zephyr on Infineon PSOC™ Edge E84

> [!IMPORTANT]
> This project is a work in progress.
> It does not yet cover the full Arduino API surface or all PSOC™ Edge features.
>
> [![Default branch status](https://github.com/Infineon/ArduinoCore-zephyr/actions/workflows/package_core.yml/badge.svg?branch=main&event=push)](https://github.com/Infineon/ArduinoCore-zephyr/actions/workflows/package_core.yml)

This repository contains the Zephyr-based Arduino core port for:

- Infineon KIT-PSE84-AI (PSOC™ Edge E84)

## ⚙️ Install In Arduino IDE

Use Arduino IDE 2.x and install the platform through Boards Manager.

1. Open Arduino IDE.
2. Navigate to *'File > Preferences'*.
3. In Additional boards manager URLs, add:

```text
https://github.com/Infineon/ArduinoCore-zephyr/releases/latest/download/package_infineon_pse84_index.json
```

4. Open *'Boards Manager'* (left side menu).
5. Search for *'PSOC Edge'* and install `Infineon PSOC Edge Boards`. This may take a moment.

Alternatively, to install the core using the command line, run the following command with the Arduino CLI:

```bash
arduino-cli core install infineon:zephyr_pse84 --additional-urls https://github.com/Infineon/ArduinoCore-zephyr/releases/latest/download/package_infineon_pse84_index.json
```
(TODO untested)

## 🏗️ First Use

To get started with your board:
6. Select the correct platform and port (e.g. `Tools > Board: ... > Infineon PSOC Edge Boards > Infineon KIT-PSE84-AI (PSOC Edge E84)` and `Tools > COM`).
7. Run the `Tools > Burn Bootloader` option from the IDE/CLI.

Once the bootloader is flashed, you can upload your first sketch.


## Current Arduino API Coverage

### Pinout

![Pinout](doc/temporaryPinout.PNG)
TODO fix image

### Macros

For the exact macro definitions, refer to `variants/kit_pse84_ai_pse846gps2dbzc4a_m33/variant.h`.
Here is a general explanation:

- Every exposed GPIO pin from the pinout has a macro identical to the documentation name, e.g. `P17_3`, `P16_0`, `P15_3`, etc.
- Digital pins have a macro in the format `Dx`, where `x` is the index of the arduino pin in the range [0, 47], e.g. `D0`, `D15`, `D47`, etc.
- Analog pins have a macro in the format `Ax`, where `x` is the index of the arduino pin in the range [0, 15], e.g. `A0`, `A1`, `A15`, etc.
- Onboard LED pin name macros are:
  - `LED_BUILTIN`: `P10_7`
  - `LED_BUILTIN_1`: `P10_7`
  - `LED_BUILTIN_2`: `P10_5`
  - `LED_BUILTIN_ACTIVE`: `HIGH`
  - `LED_RED`: `P20_6`
  - `LED_GREEN`: `P20_4`
  - `LED_BLUE`: `P20_5`
  - `BTN_BUILTIN`: `P7_0` (`SW1`)
  - TODO possibly adjust/correct (especially onboard LEDX naming convention, or e.g. SW1)
- I2C pins are defined as `SDA`/`SCL` and `SDA1`/`SCL1` for the internal (`Wire`), and external (`Wire1`) I2C bus respectively.
- Expansion header pins include the macros as printed on the board: `SERIAL_INTx` where `x` is the index in the range [0, 3], e.g. `SERIAL_INT0`.

### PSE84 Arduino API Support Tracking

Please refer to the [linked document](./api_support_tracking.md).

## Known Scope And Limits

- This is an early PSOC™ Edge port focused on enabling core Arduino workflows on Zephyr.
- API compatibility is incomplete and may change between releases.
- Some subsystems compile but are not yet fully validated on KIT-PSE84-AI.

## Contributing And Feedback

Feedback and collaboration are highly encouraged.

- Report bugs and request features in [Issues](/../../issues)
- Submit fixes through [Pull Requests](/../../pulls)
- Ask questions and discuss roadmap in [Discussions](/../../discussions)

When reporting issues, please include:

- Board and host OS
- Core version
- Minimal sketch to reproduce
- Full build/upload logs

## Development Notes

This core uses Zephyr plus a board-specific loader. Sketches are built as loadable artifacts and executed by the preflashed loader firmware.

- Loader and boot integration: [loader](loader)
- Core implementation: [cores/arduino](cores/arduino)
- PSOC™ Edge E84 variant files: [variants/kit_pse84_ai_pse846gps2dbzc4a_m33](variants/kit_pse84_ai_pse846gps2dbzc4a_m33)

Key implementation areas in this repository:

- Board definition and upload tooling: [boards.txt](boards.txt)
- Variant for KIT-PSE84-AI: [variants/kit_pse84_ai_pse846gps2dbzc4a_m33](variants/kit_pse84_ai_pse846gps2dbzc4a_m33)
- Core implementation: [cores/arduino](cores/arduino)

If you are extending support, start by validating small samples first (for example blinky, hello, threads) and then move to subsystem-specific libraries.

## 🔧 Troubleshooting

### Common Issues

<!-- #### Sketch Does Not Start

- Build and upload in `Debug` mode, then open serial output and run `sketch` from the Zephyr shell.
- For hard faults or early crashes, capture logs through the board debug/UART path.

--- -->

#### `llext` Undefined Symbol Errors

If upload succeeds but execution fails with an `Undefined symbol` error, the sketch is using a symbol not exported by the loader image for this build.

- Rebuild and flash the bootloader for this board.
- If needed, extend exported symbols in loader integration sources and rebuild.

---

#### **Q: My Sketch doesn't start (Serial doesn't appear)**
**A:** Connect a USB-to-UART adapter to the default UART (eg. TX0/RX0 on Giga, TX/RX on Nano) and read the error message (with the sketch compiled in `Default` mode). If you don't own a USB-to-UART adapter, compile the sketch in `Debug` mode; this will force the shell to wait until you open the Serial Monitor. Then, run `sketch` command and *probably* you'll be able to read the error (if generated by `llext`). For OS crashes, the USB-to-UART adapter is the only way to collect the crash.

---

#### **Q: I did it and I get the error: `<err> llext: Undefined symbol with no entry in symbol table ...`**
**A:** This means you are trying to use a Zephyr function which has not yet been exported. Open `llext_exports.c`, add the function you need and recompile/upload the loader.

---

#### **Q: I want to use a Zephyr subsystem which is not compiled in**
**A:** Open the `.conf` file for your board, add the required `CONFIG_`, recompile/upload the loader.

---

#### **Q: I get an OS crash, like `<err> os: ***** USAGE FAULT *****`**
**A:** This is usually due to a buffer overflow or coding error in the user's own code. However, since the project is still in beta 🧪, a [good bug report](#-bug-reporting) could help identify any issues in our code.

---

#### **Q: I get an out of memory error**
**A:** Since collecting bug reports is very important at this time, we are keeping Zephyr's shell enabled to allow loading a full sketch (which requires a large stack). Adjust your board's `.conf` file to reduce the stack size if your platform doesn't have enough RAM.

---

#### **Q: I get a compilation error during library detection, but the mentioned libraries are installed**
**A:** If you have installed the core locally in your Arduino sketchbook, make sure the Zephyr core is installed in a folder named `zephyr` (`${sketchbook}/hardware/arduino-git/zephyr`).
See the [Using the Core in Arduino IDE/CLI](#using-the-core-in-arduino-idecli) section for further details on the installation.

---

#### **Q: Wi-Fi is not working, or I get `Communication with WiFi module failed!` in the Serial Monitor**
**A:** You are probably missing the Wi-Fi firmware, or the firmware is corrupted. Boards should come with the Wi-Fi firmware already flashed, but in case Wi-Fi is not working run the [`FlashFormat`](libraries/Storage/examples/FlashFormat/FlashFormat.ino) sketch to restore the firmware.

## 🧢 Under the hood

Unlike traditional Arduino implementations, where the final output is a standalone binary loaded by a bootloader, this core generates a freestanding `elf` file. This file is dynamically loaded by a precompiled Zephyr firmware, referred to as the `loader`.

The `loader` is responsible for managing the interaction between your sketches and the underlying Zephyr system. After the initial bootloader installation, the `loader` takes over the sketch loading process automatically.

To ensure flexibility, the `loader` project is designed to be generic. Any necessary modifications for specific boards should be made in the corresponding "DTS overlay" or a special "fixup" file, using appropriate guards to maintain compatibility.

The behavior of the `loader` can be adjusted through the `Mode` menu of the IDE:
- `Standard`: The sketch is loaded automatically.
- `Debug`: The user must type `sketch` in Zephyr's shell, which is accessible via the default Serial.

The most important components of this project are:

* [Zephyr based loader](/loader)
* [LLEXT](https://docs.zephyrproject.org/latest/services/llext/index.html)
* [Actual core](/cores/arduino) with [variants](/variants) and the usual [platform](/platform.txt) and [boards](/boards) files
* [ArduinoCore-API](https://github.com/arduino/ArduinoCore-API)
* [zephyr-sketch-tool](/tools/zephyr-sketch-tool)

## 🏃 Shortcut: using the Core in Arduino IDE/CLI without installing Zephyr

> [!TIP]
>
> If you are only interested in developing features in the [core](/cores/arduino)
> or [libraries](/libraries), and do not want to set up a full Zephyr build
> environment, you can use the [`sync-zephyr-artifacts`](/extra/sync-zephyr-artifacts) 
> utility to download a pre-built version of the files needed to compile
> sketches and flash the loader.
>
> To do so, after cloning this repo, compile the `sync-zephyr-artifacts`
> utility via `go build` and run it as `sync-zephyr-artifacts .` to retrieve
> the precompiled files for the current revision of the core. 
>
> Next, follow the instructions in [Using the Core in Arduino IDE/CLI](#using-the-core-in-arduino-idecli)
> or [Using the Core in the Arduino App Lab](#using-the-core-in-the-arduino-app-lab) 
> to start using the core in your preferred Arduino software.
> Remember to [update the loader on your board](#flash-the-loader) as well.

## 🛠️ Setup a Zephyr build environment

> [!WARNING]  
> If you checked out this repo before 0.3.2 was released, please note that
> development has switched to the `main` branch; the old `arduino` branch will
> be removed in the short future. Please follow this
> [migration guide](https://github.com/arduino/ArduinoCore-zephyr/issues/163)
> to update your local branches and outstanding PRs.

In this section, we’ll guide you through setting up your environment to work on and update the Zephyr core.

Shell scripts are available to simplify the installation process (Windows is not directly supported at the moment 😔, but there are some tricks - see below).

### Pre-requirements
Before running the installation script, ensure that Python, `pip` and `venv` are installed on your system. The script will automatically install `west` and manage the necessary dependencies.

#### On Ubuntu or similar apt-based distros
```bash
sudo apt install python3-pip python3-setuptools python3-venv build-essential git cmake ninja-build zstd jq rsync
```
#### On macOS
Make sure you have Homebrew installed. Then run:

```bash
# Install Xcode Command Line Tools (needed for compilers and make)
xcode-select --install

# Install required tools and libraries
brew install python cmake ninja zstd jq git
```
Note: Homebrew’s Python installation already includes `pip`, `setuptools` and `venv`.

#### On Windows
Building natively on Windows is not currently supported; however, it is possible to setup and build the loader using [WSL](https://learn.microsoft.com/windows/wsl/about). Once you have that installed, you will need to follow these instructions as if you had Ubuntu.

There are two strategies to set up the sources for building the loader on Windows:
1) Install the sources in the native Windows filesystem (NTFS, FAT32, etc) and within WSL, cd to the root directory where you installed your sources, like: `/mnt/d/github/ArduinoCore-zephyr`.
2) Install the sources within the WSL file system, like: `~/git/ArduinoCore-zephyr`

There are pros and cons to both strategies:
1) Builds on the native Windows file system are relatively very slow, but once done, you can use the results directly within the Arduino IDE. 
2) Builds on WSL's file system are a lot faster, however, you need to copy the resulting build back to somewhere in your Windows directory structure. Use this location in the Arduino IDE as mentioned below in the [Using the Core in Arduino IDE/CLI](#using-the-core-in-arduino-idecli) section.

After `bootstrap.sh` has completed, you may also have to update the `cores\arduino\api` link to the path of the ArduinoCore-API's `api` folder.

### Clone the repository
```bash
mkdir my_new_zephyr_folder && cd my_new_zephyr_folder
git clone https://github.com/arduino/ArduinoCore-zephyr
```

### Run the ```bootstrap``` script
```bash
cd ArduinoCore-zephyr
./extra/bootstrap.sh
```

This will take care of installing `west`, the Zephyr build tool. It will then
download all packages required for a Zephyr build in addition to the toolchains
in the Zephyr SDK.

> [!NOTE]
> This core is validated with version v0.16.8 of the SDK. Compatibility with later versions has not been tested yet.

## 🛠️ Regenerate the compiled core files

### Build the Loader

The loader is compiled for each board by running the `./extra/build.sh` script.
The target can be specified either with the Arduino board name (as defined in
boards.txt), or with the Zephyr board name and any additional arguments that
may be required by the Zephyr build system.

For example, to build for the Arduino Portenta H7, you can use either the
Arduino name:
```bash
./extra/build.sh portentah7
```

or the Zephyr board target:

```bash
./extra/build.sh arduino_portenta_h7//m7
```

The firmwares will be copied to the [firmware](/firmware) folder, and the
associated variant will be updated.

### Flash the Loader

To flash the loader, run:

```bash
west flash -d build/<variant-name>
```

The `<variant-name>` appears in the build output when you run the build script. For example:

```bash
% ./extra/build.sh portentah7

Build target: arduino_portenta_h7@1.0.0//m7
Build variant: arduino_portenta_h7_stm32h747xx_m7
-- west build: generating a build system
...
```

In this case, you would flash with:
```bash
west flash -d build/arduino_portenta_h7_stm32h747xx_m7
```

This can also be performed via the "Burn bootloader" action in the IDE if the core is properly installed, as detailed below.

### Using the Core in Arduino IDE/CLI

After running the `bootstrap.sh` script, you can create a symlink from the
`ArduinoCore-zephyr` folder to `${sketchbook}/hardware/arduino-git/zephyr`,
or manually copy the whole folder to that location.

> [!IMPORTANT]
> Make sure the final location of the `boards.txt` file is exactly
> `${sketchbook}/hardware/arduino-git/zephyr/boards.txt`.
>
> The `zephyr` folder name above is used as part of the FQBN architecture
> (`packager:architecture:board`) and is considered for library
> compatibility checks.
> Using a different architecture folder name may result in:
> - an unsupported FQBN (e.g. `arduino-git:wrong_zephyr_folder:unoq`)
> - architecture-specific libraries (e.g. `architectures=zephyr`) not being selected

Once this is done, your development folder will appear in the IDE/CLI package
list as `arduino-git:zephyr`, and the Fully Qualified Board Name (FQBN) to use
will be `arduino-git:zephyr:name_from_boards_txt`.

Remember to also install and/or update the officially published core in the IDE Board Manager to get the latest tools and dependencies. 
[⚙️ Installation](#️-installation).

### Using the Core in the Arduino App Lab

> [!WARNING] 
> Arduino App Lab expects a hardcoded FQBN (`arduino:zephyr:unoq` for the UNO Q), so the [technique used for Arduino IDE/CLI](#using-the-core-in-arduino-idecli) **does not** work. A small workaround is required.

1. Disable the release core

```bash
mv ~/.arduino15/packages/arduino/hardware/zephyr ~/.arduino15/packages/arduino/hardware/zephyr.disable
```
This ensures the installation of the development version of the core will be used instead of the release installation.

2. Install your custom core

Place your custom core in the following path:
```bash
~/Arduino/hardware/arduino/zephyr
```

3. Flash the new loader

Build and flash the loader from your custom core to overwrite the release version.
For example, on UNO Q:
```bash
arduino-cli burn-bootloader -b arduino:zephyr:unoq -P jlink
```

#### Revert to release core

If you want to use the official core again, change the directory names so the one in `~/.arduino15` is `zephyr`, and the one in your sketchbook's hardware folder is `zephyr.disable`:
```bash
mv ~/.arduino15/packages/arduino/hardware/zephyr.disable ~/.arduino15/packages/arduino/hardware/zephyr
mv ~/Arduino/hardware/arduino/zephyr ~/Arduino/hardware/arduino/zephyr.disable
```

## 🚀 Adding a new target

> [!TIP]
>
> While Zephyr supports a lot of different hardware targets, only the few
> currently used by the Arduino core are installed by default. To add the
> support for every Zephyr target to your workspace, run the following
> commands:
>
> ```bash
> . venv/bin/activate
> west config -d manifest.project-filter
> west sdk install --version 0.17.0
> west update
> ```

To add a new board that is already supported by mainline Zephyr with the target `$your_board`, follow these steps:

* Get the variant name from your board by running `extra/get_variant_name.sh $your_board`.
* Create a folder in the [`variants/`](/variants) directory with the same name as the variant for your new board.
* Create the DTS `<variant>.overlay` and Kconfig `<variant>.conf` files in that directory.

  The overlay must include:
  * A flash partition called `user_sketch`, typically located near the end of the flash.
  * A `zephyr,user` section containing the description for GPIOs, Analog, UART, SPI and I2C devices. Feel free to leave some fields empty in case Zephyr support is missing. This will result in some APIs not being available at runtime (eg. `analogWrite` if PWM section is empty).

  The Kconfig file must include any board-specific options required by this target.
* Build the Loader: run `./extra/build.sh $your_board` (with any additional arguments as required) and start debugging the errors. :grin:
* Update the `boards.txt`: add an entry for your board, manually filling the required fields.

  Make sure to set:
   * `build.zephyr_target` and `build.zephyr_args` to the arguments used in the `build.sh` call;
   * `build.zephyr_hals` to the (space-separated list of) HAL modules required by the board;
   * `build.variant` to the variant name identified above.
* Implement touch support: if your board supports the "1200bps touch" method, implement `_on_1200_bps` in a file located inside the variant folder of your board.
