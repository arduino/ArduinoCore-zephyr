Board Configuration Reference
=============================

A. Devicetree node: `/zephyr,user`
----------------------------------


### Pin mapping under `/zephyr,user`

#### `digital-pin-gpios`
- **Location:** `/zephyr,user`
- **Type:** `phandle-array` (GPIO specifiers)
- **Meaning:** Ordered Arduino digital pin list.
- **Affects:** Defines `D0`, `D1`, … from the list order.
- **Precedence:** higher than connector-derived digital namespace (`gpio-map`).
- **Example**
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

#### `adc-pin-gpios`
- **Location:** `/zephyr,user`
- **Type:** `phandle-array` (GPIO specifiers)
- **Meaning:** Ordered list of GPIO pins treated as “ADC-capable pins” for Arduino mapping.
- **Affects:** ADC pin association (Arduino-visible analog pins ↔ ADC channels).
- **Precedence:** overrides connector `io-channel-map` (if present).
- **Notes:**
  - Association only; does not provision ADC channels (see `io-channels`)
  - The list index defines the Arduino analog pin index (e.g., index 0 corresponds to A0)
    and associates with the corresponding channel entry by index.
- **Example**
    ```dts
    / {
        zephyr,user {
            adc-pin-gpios = <&gpio0 14 0>,
                            /* -- snip -- */
                            <&gpio0 21 0>;
        };
    };
    ```

#### `pwm-pin-gpios`
- **Location:** `/zephyr,user`
- **Type:** `phandle-array` (GPIO specifiers)
- **Meaning:** Ordered list of GPIO pins treated as “PWM-capable pins” for Arduino mapping.
- **Affects:** PWM pin association (Arduino-visible PWM pins ↔ PWM channels).
- **Precedence:** overrides connector `pwm-map` (if present).
- **Notes:**
  - Association only; does not provision PWM channels (see `pwms`)
  - The list index defines the Arduino PWM-capable pin index and associates with the
    corresponding channel entry by index.
- **Example**
    ```dts
    / {
        zephyr,user {
            pwm-pin-gpios = <&gpio0 3 0>,
                            /* -- snip -- */
                            <&gpio0 11 0>;
        };
    };
    ```

#### `builtin-led-gpios`
- **Location:** `/zephyr,user`
- **Type:** `phandle-array` (GPIO specifiers)
- **Meaning:** Explicit built-in LED GPIO.
- **Affects:** Derivation of `LED_BUILTIN` when `variant.h` does not override it.
- **Precedence:** higher than board `led0` alias, lower than `variant.h` `LED_BUILTIN`.
- **Notes:**
  - If `LED_BUILTIN` is defined in `variant.h`, this property is ignored for that purpose.
- **Example**
    ```dts
    / {
        zephyr,user {
            builtin-led-gpios = <&gpio0 25 0>;
        };
    };
    ```


### Bus node lists

#### `serials`
- **Location:** `/zephyr,user`
- **Type:** phandle list (UART devices)
- **Meaning:** List of UART devices backing Arduino Serial instances.
- **Affects:** `Serial`, `Serial1`, …
- **Precedence:** overrides board default bus label (commonly `arduino_serial`).
- **Example**
    ```dts
    / {
        zephyr,user {
                serials = <&uart0>, <&uart1>;
        };
    };
    ```

#### `i2cs`
- **Location:** `/zephyr,user`
- **Type:** phandle list (I2C controller devices)
- **Meaning:** List of I2C devices backing Arduino Wire instances.
- **Affects:** `Wire`, `Wire1`, …
- **Precedence:** overrides board default bus label (commonly `arduino_i2c`).
- **Example**
    ```dts
    / {
        zephyr,user {
                i2cs = <&i2c0>, <&i2c1>;
        };
    };
    ```

#### `spis`
- **Location:** `/zephyr,user`
- **Type:** phandle list (SPI controller devices)
- **Meaning:** List of SPI devices backing Arduino SPI instances.
- **Affects:** `SPI`, `SPI1`, …
- **Precedence:** overrides board default bus label (commonly `arduino_spi`).
- **Example**
    ```dts
    / {
        zephyr,user {
                spis = <&spi0>, <&spi1>;
        };
    };
    ```


### Channel provisioning

#### `io-channels`
- **Location:** `/zephyr,user`
- **Type:** `io-channels` phandle-array specifiers
- **Meaning:** List of ADC controller/channel specifiers used by ArduinoCore-Zephyr.
- **Affects:** ADC channel provisioning (ADC availability and channel list).
- **Precedence:** higher than connector-derived ADC channel list (from `io-channel-map`).
- **Notes:**
  - Without provisioning via `/zephyr,user/io-channels`, ADC channels may still be available
    if they are derived from connector maps (see `io-channel-map`).
  - **ADC still requires per-channel configuration under the ADC device node**
    (gain/reference/acquisition time/etc.), regardless of how the channel list is obtained.
- **Example:**
    ```dts
    / {
        zephyr,user {
            io-channels = <&adc 2>,
                          /* -- snip -- */
                          <&adc 1>;
        };
    };
    ```


#### `pwms`
- **Location:** `/zephyr,user`
- **Type:** PWM phandle-array specifiers
- **Meaning:** List of PWM controller/channel specifiers used by ArduinoCore-Zephyr.
- **Affects:** PWM channel provisioning (PWM availability and channel list) and per-channel
               parameters (e.g., period/polarity).
- **Precedence:** higher than connector-derived PWM channel list (from `pwm-map`) and overrides
                  the core’s defaults.
- **Notes:**
  - Without provisioning via `/zephyr,user/pwms`, PWM channels may still be available if they are
    derived from connector maps (see `pwm-map`).
- **Example:**
    ```dts
    / {
        zephyr,user {
            pwms = <&pwm1 1 255 PWM_POLARITY_NORMAL>,
                   /* -- snip -- */
                   <&pwm2 3 255 PWM_POLARITY_NORMAL>;
        };
    };
    ```


B. Board devicetree constructs (board-provided defaults)
--------------------------------------------------------

### Node label: `arduino_header` (Arduino connector node)

- **Location:** board devicetree node label
- **Type:** label pointing to a connector node (`gpio-map` container)
- **Meaning:** Connector node that models the Arduino header pinout.
- **Affects:** Defines `D0`, `D1`, … when `/zephyr,user/digital-pin-gpios` is absent.
- **Precedence:** Used only if `/zephyr,user/digital-pin-gpios` is absent.
- **Notes:**
  - `gpio-map`: left = connector pin id, right = target GPIO.
  - Currently only `arduino_header` (compatible = "arduino-header-r3") is supported.
- **Example:**
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


### Node label: `arduino_adc` (ADC connector node)

- **Location:** board devicetree node label
- **Type:** label pointing to a connector node (`io-channel-map` container)
- **Meaning:** Connector node that models ADC-capable Arduino pins.
- **Affects:** Provides ADC pin association and/or ADC channel provisioning via `io-channel-map` when `/zephyr,user/adc-pin-gpios` and `/zephyr,user/io-channels` are absent.
- **Precedence:**
  - Lower than `/zephyr,user/adc-pin-gpios` (association)
  - Lower than `/zephyr,user/io-channels` (provisioning)
- **Notes:**
  - `io-channel-map`: left = connector pin id, right = target ADC controller/channel.
  - ADC still requires per-channel configuration under the ADC device node,
    regardless of how the channel list is obtained.
- **Example:**
    ```dts
    / {
        arduino_adc: analog-connector {
            compatible = "arduino,uno-adc";
            #io-channel-cells = <1>;
            io-channel-map = <ARDUINO_HEADER_R3_A0 &adc0 9>,
                             /* -- snip -- */
                             <ARDUINO_HEADER_R3_A5 &adc0 22>;
        };
    };
    ```


### Node label: `arduino_pwm` (PWM connector node)

- **Location:** board devicetree node label
- **Type:** label pointing to a connector node (`pwm-map` container)
- **Meaning:** Connector node that models PWM-capable Arduino pins.
- **Affects:** Provides PWM pin association and/or PWM channel provisioning via `pwm-map` when `/zephyr,user/pwm-pin-gpios` and `/zephyr,user/pwms` are absent.
- **Precedence:**
  - Lower than `/zephyr,user/pwm-pin-gpios` (association)
  - Lower than `/zephyr,user/pwms` (provisioning/parameters)
- **Notes:** `pwm-map`: left = connector pin id, right = target PWM controller/channel.
- **Example:**
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
    };
    ```


### Node label: `arduino_serial` (default Serial bus)

- **Location:** board devicetree node label
- **Type:** label pointing to a UART controller node
- **Meaning:** Board default UART device for Arduino `Serial`.
- **Affects:** Selects the backend device for `Serial`.
- **Precedence:** Used only if `/zephyr,user/serials` is absent.
- **Notes:** This is a convention-based default; use `/zephyr,user/serials` to override
             or provide multiple UARTs (`Serial1`, …).
- **Example:**
    ```dts
    arduino_serial: &uart2 {};
    ```


### Node label: `arduino_i2c` (default Wire bus)

- **Location:** board devicetree node label
- **Type:** label pointing to an I2C controller node
- **Meaning:** Board default I2C controller for Arduino `Wire`.
- **Affects:** Selects the backend device for `Wire`.
- **Precedence:** Used only if `/zephyr,user/i2cs` is absent.
- **Notes:** This is a convention-based default; use `/zephyr,user/i2cs` to override
             or provide multiple buses (`Wire1`, …).
- **Example:**
    ```dts
    arduino_i2c: &iic1 {};
    ```


### Node label: `arduino_spi` (default SPI bus)

- **Location:** board devicetree node label
- **Type:** label pointing to an SPI controller node
- **Meaning:** Board default SPI controller for Arduino `SPI`.
- **Affects:** Selects the backend device for `SPI`.
- **Precedence:** Used only if `/zephyr,user/spis` is absent.
- **Notes:** This is a convention-based default; use `/zephyr,user/spis` to override
             or provide multiple SPI instances (`SPI1`, …).
- **Example:**
    ```dts
    arduino_spi: &spi1 {};
    ```



### Alias: `led0`
- **Location:** devicetree `/aliases`
- **Type:** alias pointing to an LED node
- **Meaning:** Board default built-in LED (Zephyr convention).
- **Affects:** Used to derive `LED_BUILTIN` if neither `variant.h` `LED_BUILTIN` nor
               `/zephyr,user/builtin-led-gpios` is provided.
- **Precedence:** lowest in the built-in LED chain.
- **Example:**
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

C. `variant.h` symbols (highest-precedence C/C++ overrides)
------------------------------------------------------------

### `LED_BUILTIN`
- **Location:** board-specific `variant.h`
- **Type:** C/C++ macro (integer Arduino pin number)
- **Meaning:** Explicit Arduino-visible LED pin number.
- **Affects:** `LED_BUILTIN` behavior regardless of devicetree LED selection.
- **Precedence:** highest for built-in LED pin numbering.
- **Example:**
    ```c
    #define LED_BUILTIN 13
    ```

D. Pin mapping
--------------

### Terminology

- **Pin symbol:** Arduino-visible symbolic name such as `D0`, `D1`, `A0`, and `A1`.
- **Pin number:** Numeric argument used by Arduino APIs such as `pinMode()` and `digitalWrite()`.
- **Port/pin:** Physical GPIO tuple represented as `(GPIO controller, local pin)` (for example `gpio1/pin3`).
- **Global GPIO numbering:** Linear numeric space computed from GPIO controller offsets plus local pin numbers.

### Using `/zephyr,user/digital-pin-gpios`

`D` symbols are assigned by list order. Pin number uses Arduino digital index (`D1=1`, `D2=2`, ...).

```dts
/ {
    zephyr,user {
        digital-pin-gpios = <&gpio0 10 0>, /* D0 */
                            <&gpio0 11 0>, /* D1 */
                            <&gpio0 12 0>; /* D2 */
    };
};
```

| Pin symbol | Pin number | Port/pin      |
| ---------- | ---------- | ------------- |
|       `D0` |        `0` | `gpio0/pin10` |
|       `D1` |        `1` | `gpio0/pin11` |
|       `D2` |        `2` | `gpio0/pin12` |

### Using `arduino_header` connector definition (`gpio-map`)

`D` symbols follow connector pin names. Pin number uses global GPIO numbering.

Global GPIO numbering is a single linear pin-number space across all GPIO controllers.
For a mapped GPIO `(port, pin)`, the value is calculated as:

`global_pin = port_offset + local_pin`

- `local_pin`: pin number inside that GPIO controller
- `port_offset`: sum of `ngpios` of controllers that come before that controller
  in the devicetree order used by the core

With one controller, Pin number is the same as the local GPIO pin.
With multiple controllers, pins on later controllers include the offset.

Example calculation in the fragment below:
- `D1 -> gpio0/pin5`: `0 + 5 = 5`
- `D2 -> gpio1/pin1`: `16 + 1 = 17` (because `gpio0` has `ngpios = 16`)

```dts
/ {
    gpio0: gpio@0 {
        gpio-controller;
        ngpios = <16>;
    };

    gpio1: gpio@1 {
        gpio-controller;
        ngpios = <32>;
    };

    arduino_header: connector {
        compatible = "arduino-header-r3";
        #gpio-cells = <2>;
        gpio-map-mask = <0xffffffff 0xffffffc0>;
        gpio-map-pass-thru = <0 0x3f>;
        gpio-map = <ARDUINO_HEADER_R3_D7 0 &gpio0 5 0>, /* D1 */
                   <ARDUINO_HEADER_R3_D8 0 &gpio1 1 0>; /* D2 */
    };
};
```

| Pin symbol | Pin number | Port/pin      |
| ---------- | ---------- | ------------- |
|       `D1` |        `5` |  `gpio0/pin5` |
|       `D2` |       `17` |  `gpio1/pin1` |


E. Derived behavior switches
---------------------------

### Dx availability (`D0`, `D1`, …)
- **Defined from** `/zephyr,user/digital-pin-gpios` when present
- **Otherwise defined from** connector `gpio-map` when present
- **Otherwise** not defined (numeric global pins may still work)

### ADC channel list source (provisioning)
- **Primary:** `/zephyr,user/io-channels`
- **Fallback:** derive from connector `io-channel-map` under `arduino_adc`

### ADC pin association source
- **Primary:** `/zephyr,user/adc-pin-gpios`
- **Fallback:** connector `io-channel-map` under `arduino_adc`

### PWM channel list source (provisioning)
- **Primary:** `/zephyr,user/pwms`
- **Fallback:** derive from connector `pwm-map` under `arduino_pwm`

### PWM pin association source
- **Primary:** `/zephyr,user/pwm-pin-gpios`
- **Fallback:** connector `pwm-map` under `arduino_pwm`
