# Flash a coprocessor/radio module's firmware over USB

This sketch turns the board into a USB-to-UART bridge so a host flashing
tool (or a plain terminal) can talk to an onboard coprocessor/radio module,
such as the **Arduino Mezza**'s ST67W611M1A6BTR Wi-Fi/BLE module, and put it
into its UART ROM bootloader to write new firmware. It is useful when
updating or restoring the module's firmware; it is not a Wi-Fi example. Keep
the sketch running while the flashing tool writes the image, or use it as a
plain terminal bridge to talk to the module's AT command firmware.

Like [WiFi/examples/SerialBridge](../SerialBridge), this sketch relies only
on generic `zephyr,user` device-tree properties, so it works on any board
that declares them for its coprocessor; it is not hardcoded to a specific
board.

## How it works

At the module's CHIP_EN low-to-high transition (reset), it samples its BOOT
strap pin:

- BOOT **high**: the module starts its UART ROM bootloader, allowing new
  firmware to be downloaded.
- BOOT **low**: the module runs its current application firmware.

By default, the [WiFiProgrammer sketch](./WiFiProgrammer.ino) always drives
BOOT high at reset, so the module comes up ready to be flashed as soon as the
board powers up or resets, no further action needed. It then resets the
module (CHIP_EN low-to-high) and bridges the board's USB serial port to the
module's UART so a PC can either flash new firmware or talk to the module
directly.

Set the `WIFI_PROGRAMMER_BOOT_BUTTON` define at the top of the sketch to `1`
to instead let the board's USER button pick the boot mode at reset: hold it
down while resetting the board to enter the bootloader, leave it released to
run the module's current firmware normally. This requires a `button` alias
in the board's device tree.

## Required device-tree properties

This sketch needs the following properties under the board's `zephyr,user`
node (see an existing board, such as the Arduino Mezza, for a concrete
example):

- `cdc-acm-serial`: the USB CDC-ACM port connected to the host.
- `coprocessor-serial`: the UART connected to the coprocessor/radio module.
- `coprocessor-boot-gpios`: the module's BOOT strap pin.
- `coprocessor-reset-gpios`: the module's CHIP_EN/reset pin.

The coprocessor's UART is intentionally not exposed as one of the board's
numbered `Serial` objects (`Serial1`, `Serial2`, ...): it is reserved for
this bridge so it doesn't collide with sketches that use the module for
WiFi/BLE.

On boards that don't declare these properties, the sketch builds and runs as
a no-op.

## Procedure

1. Open the [WiFiProgrammer sketch](./WiFiProgrammer.ino).
2. If using the button-driven boot mode (`WIFI_PROGRAMMER_BOOT_BUTTON` set to
   `1`), press and hold the board's USER button, then upload the sketch (or
   reset the board) while still holding it; release the button once the
   upload/reset has completed. To instead boot the module's current firmware
   normally, leave the button released. With the default configuration, the
   module always comes up in its bootloader and no button press is needed.
3. Once uploaded, leave the sketch running: it acts as the bridge between
   USB and the radio module.
4. Connect the board to the PC and use the module's flashing tool to flash
   its firmware, or open a terminal at 115200 baud to talk to its AT command
   firmware.

For the Arduino Mezza, further instructions on obtaining and flashing the
module firmware can be found
[here](https://arduino.atlassian.net/wiki/spaces/FTHF/pages/6193348624/Mezza+-+WiFi+Module+getting+started).

> Do not use WiFi or BLE from a sketch while this one is running; it takes
> exclusive control of the module's CHIP_EN/BOOT pins and UART, which the
> WiFi driver otherwise manages on its own.
