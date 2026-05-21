/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 * Copyright (c) 2022 Dhruva Gole
 * Copyright (c) 2026 TOKITA Hiroshi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <Arduino.h>
#include "wiring_private.h"

using namespace zephyr::arduino;

/*
 *  The ACTIVE_HIGH flag is set so that A low physical
 *  level on the pin will be interpreted as value 0.
 *  A high physical level will be interpreted as value 1
 */
void pinMode(pin_size_t pinNumber, PinMode pinMode) {
	RETURN_ON_INVALID_PIN(pinNumber);

	if (pinMode == INPUT) { // input mode
		gpio_pin_configure_dt(&arduino_pins[pinNumber], GPIO_INPUT | GPIO_ACTIVE_HIGH);
	} else if (pinMode == INPUT_PULLUP) { // input with internal pull-up
		gpio_pin_configure_dt(&arduino_pins[pinNumber],
							  GPIO_INPUT | GPIO_PULL_UP | GPIO_ACTIVE_HIGH);
	} else if (pinMode == INPUT_PULLDOWN) { // input with internal pull-down
		gpio_pin_configure_dt(&arduino_pins[pinNumber],
							  GPIO_INPUT | GPIO_PULL_DOWN | GPIO_ACTIVE_HIGH);
	} else if (pinMode == OUTPUT) { // output mode
		gpio_pin_configure_dt(&arduino_pins[pinNumber], GPIO_OUTPUT_LOW | GPIO_ACTIVE_HIGH);
	}
}

void digitalWrite(pin_size_t pinNumber, PinStatus status) {
	RETURN_ON_INVALID_PIN(pinNumber);

	gpio_pin_set_dt(&arduino_pins[pinNumber], status);
}

PinStatus digitalRead(pin_size_t pinNumber) {
	RETURN_ON_INVALID_PIN(pinNumber, LOW);

	return (gpio_pin_get_dt(&arduino_pins[pinNumber]) == 1) ? HIGH : LOW;
}
