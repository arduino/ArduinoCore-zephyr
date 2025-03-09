#include <Arduino.h>
#include "zephyrInternal.h"
#include "PinNames.h"

// duplicate of one in zephyr common
static const struct gpio_dt_spec arduino_pins[] = { DT_FOREACH_PROP_ELEM_SEP(
  DT_PATH(zephyr_user), digital_pin_gpios, GPIO_DT_SPEC_GET_BY_IDX, (, )) };

static const struct gpio_dt_spec arduino_ports[] = { DT_FOREACH_PROP_ELEM_SEP(
  DT_PATH(zephyr_user), port_pin_gpios, GPIO_DT_SPEC_GET_BY_IDX, (, )) };


void pinMode(PinName pinNumber, PinMode mode) {
  // See if the pinName maps to a real pin number if so use the pin number version
  int pin_index = PinNameToIndex(pinNumber);
  if (pin_index != -1) {
    // this will grab any settings out of the device tree.
    pinMode((pin_size_t)pin_index, mode);
    return;
  }

  if (pinNumber & DUAL_PAD) return;

  if (mode == INPUT) {  // input mode
    gpio_pin_configure(arduino_ports[pinNumber >> 4].port, pinNumber & 0xf,
                       GPIO_INPUT | GPIO_ACTIVE_HIGH);
  } else if (mode == INPUT_PULLUP) {  // input with internal pull-up
    gpio_pin_configure(arduino_ports[pinNumber >> 4].port, pinNumber & 0xf,
                       GPIO_INPUT | GPIO_PULL_UP | GPIO_ACTIVE_HIGH);
  } else if (mode == INPUT_PULLDOWN) {  // input with internal pull-down
    gpio_pin_configure(arduino_ports[pinNumber >> 4].port, pinNumber & 0xf,
                       GPIO_INPUT | GPIO_PULL_DOWN | GPIO_ACTIVE_HIGH);
  } else if (mode == OUTPUT) {  // output mode
    gpio_pin_configure(arduino_ports[pinNumber >> 4].port, pinNumber & 0xf,
                       GPIO_OUTPUT_LOW | GPIO_ACTIVE_HIGH);
  }
}

void digitalWrite(PinName pinNumber, PinStatus status) {
  if (pinNumber & DUAL_PAD) return;
  gpio_pin_set(arduino_ports[pinNumber >> 4].port, pinNumber & 0xf, status);
}


PinStatus digitalRead(PinName pinNumber) {
  if (pinNumber & DUAL_PAD) return LOW;
  return (gpio_pin_get(arduino_ports[pinNumber >> 4].port, pinNumber & 0xf) == 1) ? HIGH : LOW;
}

int analogRead(PinName pinNumber) {
  // Not sure what to do here, does anyone do something like:
  // analogRead(PA_0c); or analogRead(PC2_ALT0)?
  // first pass only support ones on pins.
  if (pinNumber & DUAL_PAD) {
    switch (pinNumber & 0xf) {
    case 0: return analogRead(A0);
    case 1: return analogRead(A1);
    case 2: return analogRead(A2);
    case 3: return analogRead(A3);
    default: return -1;
    }

  }
  int pin_index = PinNameToIndex(pinNumber);
  if (pin_index != -1) {
    return analogRead(pin_index);
  }
  return -1;
}


void analogWrite(PinName pinNumber, int value) {
  // first pass only support ones on pins.
  if (pinNumber & DUAL_PAD) return;
  int pin_index = PinNameToIndex(pinNumber);
  if (pin_index != -1) {
    analogWrite(pin_index, value);
  }
}



int PinNameToIndex(PinName P) {
  if (P & DUAL_PAD) return -1;
  for (size_t i = 0; i < ARRAY_SIZE(arduino_pins); i++) {
    if ((arduino_ports[P >> 4].port == arduino_pins[i].port) && ((P & 0xf) == arduino_pins[i].pin)) {
      return i;
    }
  }
  return -1;
}

PinName digitalPinToPinName(pin_size_t P) {
  if (P < ARRAY_SIZE(arduino_pins)) {
    // Convert the pins port into an index into our port's list
    for (uint8_t port_index = 0; port_index < ARRAY_SIZE(arduino_ports); port_index++) {
      if (arduino_pins[P].port == arduino_ports[port_index].port) {
        return (PinName)((port_index << 4) | arduino_pins[P].pin);
      }
    }
  }
  return PinName::NC;
}
