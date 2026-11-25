# OTA

Downloads a firmware update over the network and installs it on an Arduino
board running the Zephyr core.  Two kinds of update:

- **Sketch only** — downloads the sketch binary and stores it on QSPI.  On
  the next boot the Zephyr loader replaces the running sketch with it.
- **Loader + sketch** (STM32H7 boards: GIGA R1, Portenta H7, Opta, Nicla
  Vision) — downloads a bundle with the Zephyr loader and the sketch.  On the
  next boot the Arduino bootloader (the one shipped with the board, also used
  by the mbed core) writes it to internal flash.  Use it when the core
  version changed, since a sketch must run on the loader it was built for.

This is a pull model: the sketch downloads the update when it decides to.
Nothing appears in the IDE port list, and the board only updates when your
sketch runs the OTA code.

## Requirements

- The loader must be built with `CONFIG_OTA=y` (the default on the boards
  above).
- Run `FlashFormat` (in `libraries/Storage/examples/`) once before using OTA.
  It creates the MBR partition table and formats the FAT filesystems on QSPI
  that both the Zephyr loader and the bootloader depend on.

## Quick start

1. Build the update, e.g. the Blink example, with **Sketch > Export Compiled
   Binary** (or `arduino-cli compile -e`).  The update files land in
   `<sketch folder>/build/<fqbn>/` (see below).
2. Start an HTTP server that serves that folder (see below).
3. Open the OTA example and set:
   - `OTA_FULL`: `0` for a sketch-only update, `1` for loader + sketch;
   - `server`: the IP address of your computer (not the board).  The
     computer and the board must be on the same network, and the
     computer's firewall must allow incoming connections on port 8000;
   - `path`: the update file name, if it is not Blink;
   - the WiFi credentials in `arduino_secrets.h` (OTAWiFi only).
4. Upload the OTA example over USB and open the Serial Monitor.  It
   downloads the update, reboots, and the board then runs Blink.

The downloaded sketch replaces the running one.  **A sketch without OTA code
cannot be updated over the air again**, so put the download code of the
example in your own sketch, behind a trigger of your choice (a button, a
Serial command, an MQTT message, ...).  Don't download the sketch that is
already running from `setup()`: it would update itself again on every boot.

## Update files

Every build writes these files to the build folder, named after the sketch:

| File | Use |
|---|---|
| `<sketch>.ino.elf-zsk.bin` | sketch-only update (`ota_sketch_download()`) |
| `<sketch>.ino.elf-zsk.bin-bundle.bin` | loader + sketch update (`ota_loader_download()`) |
| `*.ota`, `*.lzss.ota` | Arduino Cloud OTA format, **not** used by these examples |

Serve the build folder directly and request `/<sketch>.ino.elf-zsk.bin` (or
`-bundle.bin`): no copying or renaming, just rebuild and trigger the update.

### Build with export, so the folder is known

Build with **Sketch > Export Compiled Binary** in the IDE, or with `-e`
(`--export-binaries`) on the command line:

```
arduino-cli compile -e -b arduino:zephyr_main:giga <sketch folder>
```

The build output is then copied to a fixed folder inside the sketch,
`<sketch folder>/build/<fqbn with ':' replaced by '.'>/`, e.g.
`MySketch/build/arduino.zephyr_main.giga/`.  Start the HTTP server on that
folder once and leave it running; every export overwrites the files in place.

A plain **Verify** (or `arduino-cli compile` without `-e`) builds in a
temporary folder whose name is a hash, e.g. on Windows
`C:\Users\<you>\AppData\Local\arduino\sketches\<hash>\`.  It is printed on
the `OTA:` line of the build output (enable **File > Preferences > Show
verbose output during compile** if it is not shown), and it changes when the
sketch path or the board changes.

## Serving the files

Any HTTP server works, as long as it returns the file with a
`Content-Length` header.  With Python 3:

```
python3 -m http.server 8000 --directory "<build folder>"
```

On Windows the command is often `python` or `py` instead of `python3`.
Without `--directory`, the server serves the folder it is started from.

## Library API

```cpp
#include <ota_download.h>

long ota_sketch_download(Client &client, const char *host, uint16_t port, const char *path);
long ota_loader_download(Client &client, const char *host, uint16_t port, const char *path);
```

Both download over plain HTTP into the staging file and return the number of
bytes, or a negative errno (see `src/ota_download.h`).  Then apply the update:

```cpp
ota_sketch_ready();   // or ota_loader_ready()
ota_sketch_start();   // or ota_loader_start(); reboots, does not return
```

`ota_*_ready()` only renames the staging file to the name the loader (or
bootloader) looks for, so an interrupted download is never installed.

## Notes

- `millis()` counts from the loader's boot, not from the sketch start.
  After a sketch-only update the loader spends a few seconds installing it
  before the sketch runs, so a guard like
  `while (!Serial && millis() < 5000)` may already have expired.  Measure
  from the start of `setup()` instead:
  `unsigned long t = millis(); while (!Serial && millis() - t < 5000) {}`.
- The download is plain HTTP, with no authentication and no signature.
  Use it only on a network you trust.

## Examples

- **OTAWiFi** — sketch or loader + sketch update over WiFi (`OTA_FULL`).
- **OTAEthernet** — sketch or loader + sketch update over Ethernet, DHCP
  (`OTA_FULL`).
