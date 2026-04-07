# Dynamic Pinmux Design - ArduinoCore-zephyr

## What Is Pinmux / Dynamic Pin Control?

On MCUs, each physical pad can usually serve multiple alternate functions (GPIO, UART, SPI, PWM, ADC, etc.).
Pinmux (pin multiplexing) is the mechanism that selects which peripheral currently owns that pad.

Dynamic pin control means we can switch pad ownership at runtime, so the same physical pin can be reused by different peripherals at different times.

Why this matters in Arduino-like APIs:

- Users expect to call, for example, `analogRead(A0)` and later `digitalWrite(A0, HIGH)` on the same pin.
- Users expect to call `analogWrite(Dx, value)` on one PWM pin without disturbing other PWM-capable pins on the same timer block.
- The core must safely remap pins without requiring users to manually manage pinctrl states.

This is especially important for channel-based peripherals such as ADC and PWM, where one hardware block owns multiple channels/pads but user APIs usually target one pin at a time.

The solution is:

- defer peripheral init/pinctrl ownership until runtime,
- keep a custom pinctrl state (`"arduino"`) containing peripheral pads,
- when one channel is requested, configure only that pin from the custom state.

## Zephyr States Used by the Core

```text
PINCTRL_STATE_DEFAULT    = 0
PINCTRL_STATE_SLEEP      = 1
PINCTRL_STATE_PRIV_START = 2
```

Custom state name arduino is defined in devicetree and mapped to PINCTRL_STATE_ARDUINO.

Typical mapping:

- default: peripheral operational state (SPI/I2C/UART normal routing)
- sleep: optional low-power/disconnected state (not used)
- arduino: custom channel list used for per-pin restore (mainly ADC/PWM/DAC)

## Runtime Infrastructure in zephyrCommon.cpp

The core builds a runtime map between devices and pinctrl configs. 

Selection rule for nodes included in the map is node is zephyr,deferred-init.
Because the Zephyr logging UART is intentionally not marked with zephyr,deferred-init for easier debugging, it is added to the pinctrl map via a dedicated mapping entry.

The map is then used by APIs:

- init_dev_apply_pinctrl(dev)
- init_dev_apply_channel_pinctrl(dev, state_pin_idx)

If a device has no known pinctrl config in the map, APIs return -ENOTSUP.

## Dynamic Pinmux APIs and Runtime Flow

### init_dev_apply_pinctrl(dev)

Purpose: apply the whole pinctrl PINCTRL_STATE_DEFAULT state to a device.

Used for full peripheral remux, to restore the default state.

SPI (SPI.begin), I2C (Wire.begin) and UART (ZephyrSerial::begin) calls init_dev_apply_pinctrl(dev), without taking care of device readiness/init.

Behavior details:

- initializes the device on first use if needed,
- applies PINCTRL_STATE_DEFAULT from the device pinctrl config.

### init_dev_apply_channel_pinctrl(dev, state_pin_idx)

Purpose: apply only one pin from the device arduino state.

Used for channel-based peripherals (ADC/PWM/DAC) in the analogRead and analogWrite functions, allowing a single channel can be remuxed without touching sibling channels.

Behavior details:

- initializes the device on first use if needed,
- resolves PINCTRL_STATE_ARDUINO,
- applies only one pin entry (state_pin_idx), with bounds checking.

Implementation pattern:

- resolve analog/pwm index from pin,
- compute a per-device ordinal index with state_pin_index_from_spec_index(arduino_adc/arduino_pwm, idx),
- call init_dev_apply_channel_pinctrl(dev, per_dev_idx),

How state_pin_index_from_spec_index works:

- spec_idx is the global position in the zephyr,user array (io-channels for ADC or pwms for PWM). For example, A1/D1 maps to spec_idx = 1 (second entry);
- state_pin_index_from_spec_index(...) scans previous entries [0..spec_idx-1] and counts only those that reference the same device (i.e. the same PWM/ADC controller instance, such as pwm4);
- the resulting count is the per-device ordinal (0, 1, 2, ...) used as state_pin_idx in the device arduino pinctrl state.

Example (GIGA PWM list in zephyr,user):

```dts
pwms = <&pwm1 3 PWM_HZ(12000000) PWM_POLARITY_NORMAL>,
       <&pwm2 4 PWM_HZ(500) PWM_POLARITY_NORMAL>,
       <&pwm2 3 PWM_HZ(500) PWM_POLARITY_NORMAL>,
       <&pwm8 1 PWM_HZ(500) PWM_POLARITY_NORMAL>,
       <&pwm3 2 PWM_HZ(500) PWM_POLARITY_NORMAL>,
       <&pwm4 2 PWM_HZ(500) PWM_POLARITY_NORMAL>,
       <&pwm3 1 PWM_HZ(500) PWM_POLARITY_NORMAL>,
       <&pwm4 3 PWM_HZ(500) PWM_POLARITY_NORMAL>,
       <&pwm4 4 PWM_HZ(500) PWM_POLARITY_NORMAL>,
       <&pwm1 1 PWM_HZ(5000) PWM_POLARITY_NORMAL>,
       <&pwm8 2 PWM_HZ(500) PWM_POLARITY_NORMAL>,
       <&pwm1 2 PWM_HZ(5000) PWM_POLARITY_NORMAL>,
       <&pwm12 1 PWM_HZ(500) PWM_POLARITY_NORMAL>,
       <&pwm1 1 PWM_HZ(5000) PWM_POLARITY_NORMAL>;
```

If analogWrite() resolves to spec_idx = 8 (9th entry, `&pwm4 4`):

- device = pwm4
- previous pwm4 entries are at spec_idx 5 and 7
- count of previous same-device entries = 2
- state_pin_idx = 2 (zero-based)

Then init_dev_apply_channel_pinctrl(pwm4, 2) selects the 3rd entry of pwm4 `pinctrl-2`.

```dts
pwm4: pwm {
    status = "okay";
    zephyr,deferred-init;
    pinctrl-0 = <>;
    pinctrl-1 = <&analog_pd13 &analog_pb8 &analog_pb9>;
    pinctrl-2 = <&tim4_ch2_pd13 &tim4_ch3_pb8 &tim4_ch4_pb9>;
    pinctrl-names = "default", "sleep", "arduino";
};
```

In this example, the selected pin is `tim4_ch4_pb9`.

Effect:

- only the requested ADC pad or PWM pad is remuxed to analog function.

## Devicetree Requirements

### For SPI/I2C/UART nodes

Use deferred init and provide usual default/sleep states.

```dts
&spiX {
    status = "okay";
    zephyr,deferred-init;
    pinctrl-0 = <...>;  /* default */
    pinctrl-1 = <...>;  /* sleep or empty */
    pinctrl-names = "default", "sleep";
};
```

Same pattern applies to i2cX and uartX.

Recommendation: keep the UART selected as `zephyr,log-uart` out of deferred-init.
If it is marked as deferred-init, early error logs generated during system startup or sketch load may not be visible.

### For channel-based ADC/PWM/DAC nodes

Use deferred init and keep channel pads in pinctrl-2 and name it arduino.

```dts
&periphN {
    status = "okay";
    zephyr,deferred-init;
    pinctrl-0 = <>;
    pinctrl-1 = <>;
    pinctrl-2 = <...>;  /* ordered channel pads */
    pinctrl-names = "default", "sleep", "arduino";
};
```

## Ordering Contract

For each ADC/PWM device, ordering must match between:

- zephyr,user spec arrays (io-channels or pwms), and
- that device arduino pin list (pinctrl-2).

state_pin_index_from_spec_index() assumes this ordinal alignment to map a logical channel entry to the correct pin in the arduino state.

## Known limitation - Nordic nRF

On Nordic nRF targets, switching a pin from PWM back to another peripheral may leave PWM still affecting that pin unless PWM is explicitly stopped first.

The reason is architectural: in the nRF pinctrl/driver model, pin routing is programmed per peripheral by writing that peripheral’s PSEL registers. Reconfiguring the new peripheral (i.e. SPI) updates SPI’s PSEL, but it does not automatically clear PWM’s PSEL ownership for the same physical pin.
In our current core flow, we do not keep global runtime ownership tracking across peripherals for each pin, so we cannot reliably force-release the previous owner in all cases.

This limitation is relevant only when the same pin was previously configured/driven by PWM and is now being reassigned to another peripheral.  

Workaround:
Before re-enabling a different peripheral function on a PWM shared pin, explicitly stop PWM on that pin first:

```
// Example sequence: PWM -> SPI on the same pin
analogWrite(Dx, value);
...
analogWrite(Dx, 0);
SPI.begin();
...
```
