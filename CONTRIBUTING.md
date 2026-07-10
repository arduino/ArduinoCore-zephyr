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

### Development Workflows

Pushing to the `main` branch of this repository automatically triggers a workflow that builds and publishes a new release. Three development paths are available — choose the one that fits your setup:

#### Fork-based (no local toolchain required)

Fork this repository and push changes to your fork's `main` branch. GitHub Actions will build and publish a release automatically. You can then add the released JSON index URL to your Arduino IDE to test.

Best for: quick feature work or experimentation without installing any toolchain.

#### Local setup — without Zephyr

Clone the repository and use the [`sync-zephyr-artifacts`](#-shortcut-using-the-core-in-arduino-idecli-without-installing-zephyr) utility to download pre-built loader binaries. Allows you to work on core and library code without a full Zephyr environment.

Best for: changes to [cores/arduino](cores/arduino) or [libraries](libraries) that do not require rebuilding the loader.

→ See: [Shortcut: using the Core without installing Zephyr](#-shortcut-using-the-core-in-arduino-idecli-without-installing-zephyr)

#### Local setup — full Zephyr

Set up a complete Zephyr build environment to rebuild the loader from source. Faster iteration than the fork-based workflow once set up.

Best for: loader changes, new board support, or Kconfig/DTS work.

→ See: [Setup a Zephyr build environment](#️-setup-a-zephyr-build-environment)

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
