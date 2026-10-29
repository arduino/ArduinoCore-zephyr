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

**Boards booted by MCUboot** — the sketch is part of the loader image, so
sketch and full updates are the same: the examples stream the image into
MCUboot's secondary slot and MCUboot installs it on the next boot.

## Requirements

- The loader must be built with `CONFIG_OTA=y`.
- Except on MCUboot boards: run `FlashFormat` (in `libraries/Storage/examples/`)
  once before using OTA. It creates the MBR partition table and formats the
  FAT filesystems on QSPI that both the Zephyr loader and the mbed bootloader
  depend on.

## API

```c
ota_write_begin(OTA_SKETCH, size);   /* or OTA_LOADER */
ota_write(buf, len);                 /* repeat for the whole payload */
ota_write_end();
ota_sketch_ready();                  /* or ota_loader_ready() */
ota_sketch_start();                  /* reboots and installs */
```

## Producing the update images

**Sketch only** — take the `.elf-zsk.bin` from the sketch build.

**Loader + sketch** — take the `.elf-zsk.bin-bundle.bin` from the sketch
build.  The build system produces it automatically; it is the raw loader
binary + 0xFF padding + sketch, without the OTA header.  The mbed
bootloader validates the file by checking that the first word is a valid
ARM initial stack pointer; an OTA header would fail that check.

**MCUboot boards** — take `<sketch>.ino.signed.elf-zsk.bin` from the sketch
build (loader + sketch in one image, signed with the board's key). Not the
`.signed.dfu.` copy: that one is padded for USB uploads.

Serve the file as `UPDATE.bin` with any HTTP server:

```
cp <sketch>.elf-zsk.bin UPDATE.bin          # sketch only
cp <sketch>.elf-zsk.bin-bundle.bin UPDATE.bin  # loader + sketch
cp <sketch>.ino.signed.elf-zsk.bin UPDATE.bin # MCUboot boards
python3 -m http.server 8000
```

## Examples

- **OTAEthernet** — sketch update over Ethernet (DHCP).
- **OTAWiFi** — sketch update over WiFi (edit `arduino_secrets.h`).
- **OTAFullEthernet** — loader + sketch update over Ethernet.
- **OTAFullWiFi** — loader + sketch update over WiFi.

## Failed updates on MCUboot boards

MCUboot swaps an update in as a test (swap-using-offset):

- A truncated download or a file that is not an MCUboot image is refused by
  `ota_write_end()` or `ota_sketch_ready()`; nothing changes.
- An image whose hash or signature does not verify is erased by MCUboot and
  the running sketch keeps booting.
- A valid image runs once as a test. The loader confirms it after
  `CONFIG_LOADER_CONFIRM_DELAY_MS` (10 s) from its lowest-priority thread,
  or the sketch confirms earlier with `ota_confirm()`. If the board resets
  before that (a crash, or the watchdog after a lockup) MCUboot swaps the
  previous image back.
- A USB upload (double-tap reset, or `arduino-cli upload`) overwrites the
  running image for good, without a swap; one that is broken or interrupted
  leaves the running sketch in place.
