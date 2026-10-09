# Flash the Portenta C33 ESP32-C3 coprocessor

This sketch turns the **Arduino Portenta C33** into a USB-to-UART bridge so a
host flashing tool can put its onboard ESP32-C3 into the ROM bootloader and
write new firmware. It is useful when updating or restoring the coprocessor
firmware; it is not a Wi-Fi example. Keep the sketch running while `esptool` or
`espflash` writes the image.

The bridge currently applies to the Portenta C33, whose board configuration
provides the USB serial, coprocessor UART, and boot/reset GPIOs used by the
sketch. On boards without those device-tree properties, the sketch builds as a
no-op and does not provide a flashing bridge.

Pre-built firmware binaries for the ESP32 coprocessor can be found [here](https://github.com/arduino/esp-hosted-firmware/releases).

Each release contains both the legacy firmware (v0.0.5) and the latest stable version.

## How it works

The [SerialBridge sketch](./SerialBridge.ino) uses the board's USB port as a bridge to the ESP32. The board firmware mirrors the `DTR` and `RTS` control signals onto the coprocessor boot/reset lines, allowing the flashing tool to enter the ESP32 UART bootloader and send the binary through the bridge serial link without any special wiring.

## Procedure

1. Open the [SerialBridge sketch](./SerialBridge.ino).
2. Upload it to the Arduino board using Arduino IDE or arduino-cli.
3. Once uploaded, leave the sketch running: it acts as the bridge between USB and the ESP32.
4. Connect the board to the PC and use an ESP flasher to flash the ESP32 firmware.

## Flash with esptool

Official binary releases can be found in the [espressif/esptool GitHub repository](https://github.com/espressif/esptool/releases).

Alternatively, you can download and install esptool using Python's package manager, pip. For further details on installation, please refer to the [official documentation](https://docs.espressif.com/projects/esptool/en/latest/esp32/installation.html).

Replace `<PORT>` with the board's serial port (for example, `/dev/ttyACM0` or `COM3`):

```bash
esptool --chip esp32c3 --port <PORT> --baud 230400 --before=default_reset --after=hard_reset --no-stub write_flash --flash_mode dio --flash_freq 80m 0x0 ./ESP32-C3/portenta_c33-v1.0.0.0.3.bin
```

To revert to the legacy version:

```bash
esptool --chip esp32c3 --port <PORT> --baud 230400 --before=default_reset --after=hard_reset --no-stub write_flash --flash_mode dio --flash_freq 80m 0x0 ./ESP32-C3/portenta_c33-v0.0.5.bin
```

## Flash with espflash

Official binary releases can be found in the [official GitHub repository](https://github.com/esp-rs/espflash/releases).

```bash
espflash write-bin 0x0 ./ESP32-C3/portenta_c33-v1.0.0.0.3.bin
```

For the legacy version:

```bash
espflash write-bin 0x0 ./ESP32-C3/portenta_c33-v0.0.5.bin
```

> Use the latest version for normal updates; the legacy image is intended only for rollback.
