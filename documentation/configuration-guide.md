Board Configuration Guide
=========================

This document defines practical support levels for ArduinoCore-Zephyr and explains how to reach each level.

The support level definitions
-----------------------------

### Level 0 - Basic GPIO runtime

Arduino GPIO APIs are usable with **numeric pins**.
All boards are expected to satisfy this level **once ArduinoCore-Zephyr is enabled**.

### Level 1 — Blinky-ready

The Blinky sample works if the board provides a built-in LED.

### Level 2 — Digital pin definitions are available

Arduino-style digital pin definitions `D0`, `D1`, … are available.

### Level 3 — Common bus definitions are available

Common buses (`Serial`, `Wire`, and `SPI`) are available for use.

### Level 4 — PWM and ADC are available for use

Channel provisioning and pin association are provided to make PWM and ADC available.

### Level 5 — Supports board-specific features

Board-specific peripherals and features are supported.



Making a configuration for your board
-------------------------------------

A well-configured board equipped with an Arduino connector provides **Level 3** support without additional configurations.

**Level 4** support is required to cover most standard Arduino use cases.
Reaching **Level 4 and above** typically requires a board-/variant-specific overlay, typically delivered as snippets.

This guide targets Level 4, which supports standard Arduino functions.


### Configuration sources

#### Board-provided devicetree (Zephyr source tree)

A well-configured board DTS typically includes:

- An Arduino-compatible connector definition,
  labeled as `arduino_header` (or other supported connectors)
- Optionally, connector mapping nodes for PWM/ADC pin association and channel derivation (commonly `arduino_pwm` and `arduino_adc`)
- Bus defaults via node labels such as `arduino_serial`, `arduino_i2c`, `arduino_spi`
- A built-in LED via the `led0` alias (recommended)

These are the “works out of the box” ingredients.

#### Overlays

Define nodes/properties under `/zephyr,user` to configure GPIO, ADC, PWM, I2C, and SPI.
Use this when there is no connector definition or when you want to overwrite it.


Configuration point by level
----------------------------

### Level 1

If your board has an onboard LED, you can support it in one of the following ways (**lowest precedence rule first**):

1. Define the `led0` alias for the LED node.

   You can usually find a definition like this in the board devicetree.
   If a `led0` alias exists, no additional configuration is needed.

    ```dts
    / {
        leds {
            compatible = "gpio-leds";

            led: led {
                gpios = <&gpio0 13 GPIO_ACTIVE_HIGH>;
            };
        };

        aliases {
            led0 = &led;
        };
    };
    ```

2. Define `/zephyr,user/builtin-led-gpios`

   Define the `builtin-led-gpios` property to explicitly select the built-in LED GPIO.

    ```dts
    / {
        zephyr,user {
            builtin-led-gpios = <&gpio0 25 0>;
        };
    };
    ```

3. Manually define `LED_BUILTIN` in the board-specific `variant.h`

   As a last resort, you can define `LED_BUILTIN` directly in variant.h.

    ```c
    #define LED_BUILTIN 13
    ```


### Level 2

Two ways to reach it (**lowest precedence rule first**):

1. **Define an Arduino connector**

   If you have an Arduino header definition like the one below,
   `gpio-map` can be used to generate Arduino-style digital pin names (`D0`, `D1`, …).
   The leftmost value in each map entry is the connector pin identifier (connector-dependent).

    ```dts
    / {
        arduino_header: connector {
                compatible = "arduino-header-r3";
                #gpio-cells = <2>;
                gpio-map-mask = <0xffffffff 0xffffffc0>;
                gpio-map-pass-thru = <0 0x3f>;
                gpio-map = <ARDUINO_HEADER_R3_A0 0 &ioport0 14 0>,
                           /* -- snip -- */
                           <ARDUINO_HEADER_R3_D15 0 &ioport1 0 0>;
        };
    };
    ```

2. **Define `/zephyr,user/digital-pin-gpios`**

   Set the list of GPIOs to use in `/zephyr,user/digital-pin-gpios`.
   They will be assigned in the order you set them: `D0`, `D1`, `D2`, ...

    ```dts
    / {
        zephyr,user {
            digital-pin-gpios = <&gpio0 0 0>,
                                <&gpio0 1 0>,
                                /* -- snip -- */
                                <&gpio1 5 0>;
        };
    };
    ```

Pin number behavior differs between `/zephyr,user/digital-pin-gpios` and connector-derived (`gpio-map`) configuration.
For detailed rules, examples, and precedence between the two methods, see `documentation/configuration-reference.md`, Section D (`Pin mapping`).


### Level 3

Two ways to reach it (**lowest precedence rule first**):

1. **Use board default buses via connector**

   If the board provides node labels such as `arduino_serial`, `arduino_i2c`, and `arduino_spi`,
   they will be used as defaults for `Serial`, `Wire`, and `SPI`.
   Use the conventional labels `arduino_serial`, `arduino_i2c`, and `arduino_spi`.

    ```dts
    arduino_i2c: &iic1 {};
    arduino_spi: &spi1 {};
    arduino_serial: &uart2 {};
    ```


2. **Define `/zephyr,user/serials`, `/zephyr,user/i2cs`, `/zephyr,user/spis`**

   Set the list of UART devices to use in `/zephyr,user/serials`.
   This will create Arduino `Serial`, `Serial1`, ... objects.
   The same applies to `i2cs` and `spis` for `Wire/Wire1...` and `SPI/SPI1...`.

    ```dts
    / {
        zephyr,user {
            serials = <&uart2>;
            i2cs = <&iic1>;
            spis = <&spi1>;
        };
    };
    ```

You can also add settings under `/zephyr,user` to fill in missing items in the devicetree.


### Level 4

To correctly configure PWM and ADC, two independent items must be addressed:

1. **Pin association**: mapping PWM/ADC input/output pins to GPIO numbers
2. **Channel provisioning**: PWM and ADC channel configuration

#### 1) Pin association

This is similar to configuring GPIO:

1) **Derive from connector maps**

   Provide connector mappings via the board’s connector definitions (e.g., `pwm-map` and `io-channel-map`)

    ```dts
    / {
        arduino_pwm: connector-pwm {
                compatible = "arduino-header-pwm";
                #pwm-cells = <3>;
                pwm-map = <ARDUINO_HEADER_R3_D2 0 0 &pwm1 0 0 0>,
                          /* -- snip -- */
                          <ARDUINO_HEADER_R3_D13 0 0 &pwm3 0 0 0>;
                pwm-map-mask = <0xffffffff 0x0 0x0>;
                pwm-map-pass-thru = <0x0 0xffffffff 0xffffffff>;
        };

        arduino_adc: analog-connector {
            compatible = "arduino,uno-adc";
            #io-channel-cells = <1>;
            io-channel-map = <ARDUINO_HEADER_R3_A0 &adc0 9>,
                             /* -- snip -- */
                             <ARDUINO_HEADER_R3_A5 &adc0 22>;
        };
    };
    ```

2) **Explicit lists under `/zephyr,user`**

   Set GPIO pin lists in `/zephyr,user/pwm-pin-gpios` and `/zephyr,user/adc-pin-gpios`.

    ```dts
    / {
        zephyr,user {
            pwm-pin-gpios = <&gpio0 3 0>,
                            /* -- snip -- */
                            <&gpio0 11 0>;

            adc-pin-gpios = <&gpio0 14 0>,
                            /* -- snip -- */
                            <&gpio0 21 0>;
        };
    };
    ```


#### 2) Channel provisioning

ArduinoCore-Zephyr needs a list of available PWM/ADC channels.
You can provide it in either of the following ways (**lowest precedence rule first**):

1) **Derive from connector maps**

   If the board defines `arduino_pwm` with `pwm-map` and/or `arduino_adc` with `io-channel-map`,
   ArduinoCore-Zephyr can derive the channel lists from those maps.
   In that case, the additional `/zephyr,user` properties are not required.
   In other words, all you need to do is set the ADC channel settings described below.

2) **Explicit lists under `/zephyr,user`**

   Set the PWM/ADC channels corresponding to the pins specified by `pwm-pin-gpios` and `adc-pin-gpios`.
   - PWM channels: `/zephyr,user/pwms`
   - ADC channels: `/zephyr,user/io-channels`

    ```dts
    / {
        zephyr,user {
            pwms = <&pwm1 1 255 PWM_POLARITY_NORMAL>,
                   /* -- snip -- */
                   <&pwm2 3 255 PWM_POLARITY_NORMAL>;

            io-channels = <&adc 2>,
                          /* -- snip -- */
                          <&adc 1>;
        };
    };
    ```

For ADC, you still need to define per-channel settings under each ADC device node.
See also the examples in `samples/drivers/adc/adc_dt`.

    ```dts
    &adc {
        #address-cells = <1>;
        #size-cells = <0>;

        channel@0 {
            reg = <0>;
            zephyr,gain = "ADC_GAIN_1_6";
            zephyr,reference = "ADC_REF_INTERNAL";
            zephyr,acquisition-time = <ADC_ACQ_TIME_DEFAULT>;
            zephyr,input-positive = <NRF_SAADC_AIN0>; /* P0.02 */
            zephyr,resolution = <10>;
        };
        /* -- snip -- */
    };
    ```


### Level 5

Level 5 supports special features of each board, and the way to achieve this varies by board.

It may require:
- adding definitions to `variant.h` or overlays,
- adding libraries to support the features, or
- adding features to Zephyr itself when necessary.
