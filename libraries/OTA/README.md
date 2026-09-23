# OTA

Downloads and installs firmware updates on an Arduino board running the
Zephyr core.

**Sketch only** — the examples fetch a sketch binary over plain HTTP and
write it to the loader's staging area.  On the next boot the Zephyr loader
installs it in place of the running sketch.

**Loader + sketch** — the examples fetch a full binary (loader + sketch)
and write it to the OTA partition on QSPI.  On the next boot the mbed
bootloader writes the entire payload to internal flash, updating both the
loader and the sketch in one shot.

## Requirements

- The loader must be built with `CONFIG_OTA=y`.
- Run `FlashFormat` (in `libraries/Storage/examples/`) once before using OTA.
  It creates the MBR partition table and formats the FAT filesystems on QSPI
  that both the Zephyr loader and the mbed bootloader depend on.

## Producing the update images

**Sketch only** — take the `.elf-zsk.bin` from the sketch build.

**Loader + sketch** — take the `.elf-zsk.bin-bundle.bin` from the sketch
build.  The build system produces it automatically; it is the raw loader
binary + 0xFF padding + sketch, without the OTA header.  The mbed
bootloader validates the file by checking that the first word is a valid
ARM initial stack pointer; an OTA header would fail that check.

Serve the file as `UPDATE.bin` with any HTTP server:

```
cp <sketch>.elf-zsk.bin UPDATE.bin          # sketch only
cp <sketch>.elf-zsk.bin-bundle.bin UPDATE.bin  # loader + sketch
python3 -m http.server 8000
```

## Examples

- **OTAEthernet** — sketch update over Ethernet (DHCP).
- **OTAWiFi** — sketch update over WiFi (edit `arduino_secrets.h`).
- **OTAFullEthernet** — loader + sketch update over Ethernet.
- **OTAFullWiFi** — loader + sketch update over WiFi.
