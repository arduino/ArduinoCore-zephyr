# Flash the Wi-Fi coprocessor firmware

This sketch turns the board into a USB-to-UART bridge, so a host flashing tool
can put the onboard Wi-Fi coprocessor into its ROM bootloader and write new
firmware. Use it to update or restore the coprocessor firmware; it is not a
Wi-Fi example. Keep the sketch running while `esptool` or `espflash` writes the
image.

Supported boards:

| Board | Coprocessor | Firmware |
|---|---|---|
| Portenta C33 | ESP32-C3 | [ESP32-C3 (Portenta C33)](#portenta-c33-esp32-c3) |
| Nano XL Link (Connector carrier) | ESP8266 (ESP-AT) | [ESP8266 (Nano XL Link Connector)](#nano-xl-link-connector-esp8266) |

On boards without the required device-tree properties, the sketch builds as a
no-op and does not provide a flashing bridge.

## How it works

The [SerialBridge sketch](./SerialBridge.ino) uses the board's USB port as a
bridge to the coprocessor UART. It mirrors the USB serial `DTR` and `RTS`
control lines onto the coprocessor boot and reset pins (`DTR` → boot strap,
`RTS` → reset), like the auto-reset circuit of a USB-serial adapter, and follows
the baud rate set by the host. The flashing tool can therefore enter the ROM
bootloader and send the binary without any special wiring.

The board provides the connections through these `zephyr,user` devicetree
properties: `cdc-acm-serial`, `coprocessor-serial`, `coprocessor-boot-gpios`
and `coprocessor-reset-gpios`.

## Procedure

1. Open the [SerialBridge sketch](./SerialBridge.ino).
2. Upload it to the board using Arduino IDE or arduino-cli.
3. Once uploaded, leave the sketch running: it acts as the bridge between USB
   and the coprocessor.
4. Flash the coprocessor firmware with the commands for your board below.
5. Upload your own sketch again when done.

esptool binary releases can be found in the
[espressif/esptool GitHub repository](https://github.com/espressif/esptool/releases).
Alternatively, install it with Python's package manager, pip; see the
[official documentation](https://docs.espressif.com/projects/esptool/en/latest/esp32/installation.html).

In the commands below, replace `<PORT>` with the board's serial port (for
example, `/dev/ttyACM0` or `COM3`).

## Portenta C33 (ESP32-C3)

Pre-built firmware binaries for the ESP32 coprocessor can be found
[here](https://github.com/arduino/esp-hosted-firmware/releases). Each release
contains both the legacy firmware (v0.0.5) and the latest stable version.

### Flash with esptool

```bash
esptool --chip esp32c3 --port <PORT> --baud 230400 --before=default_reset --after=hard_reset --no-stub write_flash --flash_mode dio --flash_freq 80m 0x0 ./ESP32-C3/portenta_c33-v1.0.0.0.3.bin
```

To revert to the legacy version:

```bash
esptool --chip esp32c3 --port <PORT> --baud 230400 --before=default_reset --after=hard_reset --no-stub write_flash --flash_mode dio --flash_freq 80m 0x0 ./ESP32-C3/portenta_c33-v0.0.5.bin
```

### Flash with espflash

Official binary releases can be found in the
[official GitHub repository](https://github.com/esp-rs/espflash/releases).

```bash
espflash write-bin 0x0 ./ESP32-C3/portenta_c33-v1.0.0.0.3.bin
```

For the legacy version:

```bash
espflash write-bin 0x0 ./ESP32-C3/portenta_c33-v0.0.5.bin
```

> Use the latest version for normal updates; the legacy image is intended only for rollback.

## Nano XL Link Connector (ESP8266)

The Connector carrier's ESP8266 module runs Espressif's ESP-AT firmware on
sercom1 (`Serial2` while WiFi is unused), with its reset line on D7. The WiFi
library requires **AT 1.7 or later**: with an older firmware (modules have
shipped with AT 1.3.0.0), or without a module, `WiFi.status()` and
`WiFi.begin()` report `WL_NO_SHIELD`. The module has 1 MB of flash; the image that fits it is
**AT 1.7.5**, the prebuilt "512+512" AT binary of the
[ESP8266 NONOS SDK v3.0.5](https://github.com/espressif/ESP8266_NONOS_SDK/tree/v3.0.5/bin).
Newer ESP-AT 2.x packages need 2 MB and do not fit.

Download these files from the SDK's `v3.0.5` tag:

- [`bin/boot_v1.7.bin`](https://github.com/espressif/ESP8266_NONOS_SDK/raw/v3.0.5/bin/boot_v1.7.bin)
- [`bin/at/512+512/user1.1024.new.2.bin`](https://github.com/espressif/ESP8266_NONOS_SDK/raw/v3.0.5/bin/at/512%2B512/user1.1024.new.2.bin)
- [`bin/esp_init_data_default_v08.bin`](https://github.com/espressif/ESP8266_NONOS_SDK/raw/v3.0.5/bin/esp_init_data_default_v08.bin)
- [`bin/blank.bin`](https://github.com/espressif/ESP8266_NONOS_SDK/raw/v3.0.5/bin/blank.bin)

### Flash with esptool

```bash
esptool --chip esp8266 --port <PORT> --baud 115200 --before default-reset --after hard-reset \
    write-flash --flash-mode dout --flash-freq 40m --flash-size 1MB \
    0x00000 boot_v1.7.bin \
    0x01000 user1.1024.new.2.bin \
    0x7e000 blank.bin \
    0xfc000 esp_init_data_default_v08.bin \
    0xfe000 blank.bin
```

The addresses are the 8 Mbit "512+512" layout from the SDK's
`bin/at/README.md`. Every region should report `Hash of data verified`; the
whole write takes about 30 seconds.

To check the module identity first:

```bash
esptool --chip esp8266 --port <PORT> --baud 115200 --before default-reset --after hard-reset flash-id
```

> Run **one** esptool command per sketch start. On this board a second esptool
> session usually fails to connect (`No serial data received`); press the reset
> button, or upload the sketch again, before the next one.
