# Adding custom boards/variants

- Boards already supported by Zephyr can be added to the variants folder as outlined in this documentation.
- Custom boards can first be added by following the [official Zephyr porting guide](https://docs.zephyrproject.org/latest/hardware/porting/board_porting.html).
Once completed, continue here by adding a variant for your custom board.

## Supported Boards/variants

- [X] Arduino Nano ble sense 33
- [X] Arduino Nano ble 33
- [X] Arduino Nano 33 iot
- [X] Beagleconnect Freedom

## Planned Support: (TODO)
- [ ] Particle Xenon
- [ ] Arduino mkrzero
- [ ] TI-CC3220SF LaunchXL
- [ ] nrf52840dk_nrf52840

## How to add board variants

This module uses the board name (supplied at build time by the `-b
arduino_nano_33_ble` flag) to correctly map Arduino pin names/numbers to the
target board. To add board support:

1. This project is structured in a way so as to isolate the variants from the core API. Thus, whenever a new board
needs to be added it needs to be done in the `variants/` folder.
Add a folder inside of the variants folder that matches the name of your board.
2. Add an overlay file that matches the name of the board.
3. Add a `variant.h` file.

An example of this structure is shown below.

```tree
variants/
├── arduino_nano_33_ble
│   ├── arduino_nano_33_ble.overlay
│   ├── variant.h
```

## Guide to Writing Overlays

### DeviceTree Overlay files for Arduino boards

The Arduino API requires pin mapping definitions to use Arduino-style pin numbers
(pin numbers printed on the board, not GPIO numbers).
When `digital-pin-gpios` is not defined and pin mapping is derived from connector `gpio-map`,
Arduino API `Pin number` uses global GPIO numbering (see `configuration-reference.md`, Section D).
The pin-mapping node is under the `zephyr,user` node of DTS.
`digital-pin-gpios` defines digital input/output pins, i.e. D0, D1, ...,
`adc-pin-gpios` defines analog input pins, i.e. A0, A1, etc.
`pwm-pin-gpios` defines PWM pins.
Each pin specifies in the form of a GPIO cell.
Usually, it is in the form of `<[port] [pin-number] [flag]>`.
You can also use the Arduino header node definition here.

> [!NOTE]
> For phandle-array properties like these (`digital-pin-gpios`, `adc-pin-gpios`,
> `pwm-pin-gpios`, `serials`, `i2cs`, etc.), both `<&dev1 &dev2>` (multiple entries in a
> single bracket group) and `<&dev1>, <&dev2>` (separate bracket groups joined by commas)
> are valid DTS syntax. However, `<&dev1, &dev2>` is **invalid**.

### Overlays using previously-defined Arduino headers

When an Arduino header exists in a board's in-tree DTS file it can easily be
used to create the necessary overlay file. Assign the relevant mapping using the
Arduino header label (usually either `&arduino_header` or `&arduino_nano_header`
and the connector pin identifier from `gpio-map`. The second number is used to add GPIO flags and may
safely be left as zero.

For example, creating an overlay file for the Nordic nRF52840 Development Kit
uses [the Arduino header definitions](https://github.com/zephyrproject-rtos/zephyr/blob/6f8ee2cdf7dd4d746de58909204ea0ce156d5bb4/boards/arm/nrf52840dk_nrf52840/nrf52840dk_nrf52840.dts#L74-L101), beginning with the first digital pin:

```
/ {
	zephyr,user {
		digital-pin-gpios = <&arduino_header 6 0>, /* Digital */
			    <&arduino_header 7 0>,
			    ...
			    <&arduino_header 19 0>,
			    <&arduino_header 0 0>, /* Analog */
			    <&arduino_header 1 0>,
			    ...
			    <&arduino_header 5 0>,
			    <&arduino_header 20 0>, /* SDA */
			    <&arduino_header 21 0>, /* SCL */
			    <&gpio0 13 GPIO_ACTIVE_LOW>; /* LED0 */
	};
};
```

### Configure Serial devices

The `serials` node defines the Serial devices to use.
It instantiates `Serial` with the UART device contained in the node.
Also instantiates `Serial1`, `Serial2`, ... `SerialN` with the devices
after the first when the array contains more than one device.

If the `serials` node is not defined, use the node labeled `arduino_serial`.
Boards with Arduino-shield style connectors usually label `arduino_serial` for
UART port exposed in header or frequently used UART port.

If even `arduino_serial` is not defined, it uses the stub implementation
that redirects to printk().

The following example instantiates `Serial` and `Serial1` with each `uart0` and `uart1`.

```
/ {
       zephyr,user {
               serials = <&uart0>, <&uart1>;
       };
};
```

### Configure I2C devices

The `i2cs` node defines the I2C devices to use.
It instantiates `Wire` with the i2c device contained in the node.
Also instantiates `Wire1`, `Wire2`, ... `WireN` with the devices
after the first when the array contains more than one device.

If the `i2cs` node is not defined, use the node labeled `arduino_i2c`.
Boards with Arduino-shield style connectors usually label `arduino_i2c`
to i2c exposed in the connector.

The following example instantiates `Wire` and `Wire1` with each `i2c0` and `i2c1`.

```
/ {
       zephyr,user {
               i2cs = <&i2c0 &i2c1>;
       };
};
```

### Configure Builtin-LED

The `builtin-led-gpios` node defines the Builtin-LED.
This node defines the `LED_BUILTIN` value by looking up the `digital-pin-gpios`
array to find the index of the pin.

The node is phandle-array, which uses the format same as `digital-pin-gpios`.

It sets the digital pin number to `LED_BUILTIN` if the pin
defined in `builtin-led-gpios` is found in `digital-pin-gpios`.

If the `builtin-led-gpios` is not defined, use the node aliased as `led0`
to define `LED_BUILTIN`.

`LED_BUILTIN` is not defined if neither node is found or `LED_BUILTIN` is already defined.

When `/zephyr,user/digital-pin-gpios` is absent, the `led0` alias sets
`LED_BUILTIN` to the **global GPIO-based Pin number**, which matches the
numeric pin interpretation in that mode.
See `configuration-reference.md`, Section B (`Alias: led0`) and
Section D (`Pin mapping` / `Global GPIO numbering`).

For example, in the case of the 13th digital pin connected to the onboard LED,
define `builtin-led-gpios` as follows.

```
/ {
	zephyr,user {
		builtin-led-gpios = <&arduino_nano_header 13 0>;
	};
};
```

### Overlays from scratch

You can see in the example above that there is no mapping for `LED0` in the
board's Arduino header definition so it has been added using the Zephyr `gpios`
syntax (port, pin, flags). When creating an overlay file that doesn't have an
Arduino header defined, you should follow this syntax for adding all pins.

### You control the pin mapping

Zephyr [chooses to map Arduino headers beginning with the Analog
pins](https://docs.zephyrproject.org/latest/build/dts/api/bindings/gpio/arduino-header-r3.html),
but the overlay file example above begins with the digital pins. This is to
match user
expectations that issuing `pinMode(0,OUTPUT);` should control digital pin 0 (and
not pin 6). In the same way, the Analog 0 pin was mapped to D14 as this is
likely what a shield made for the Arduino Uno R3 header would expect.

Ultimately the mapping is completely up to you and should match the needs of the
sketch you will be compiling.

## Creating the pinmap header file

It is recommended that you copy an existing pinmap file from one of the board
folders inside of the variants folder. For the most part, this header file will
not change from board to board.

One example of a change that you may find useful is mapping additional pins. For
example, the LEDs on the nRF52840 are not connected to any of the Arduino header
pins. To define a built-in LED for this board, a 22nd pin definition was added.

Your pinmap header file must be added to the variant.h file by adding three
lines using this format:

```c
#ifdef CONFIG_BOARD_NRF52840DK_NRF52840
#include "nrf52840dk_nrf52840.h"
#endif // CONFIG_BOARD_NRF52840DK_NRF52840
```
