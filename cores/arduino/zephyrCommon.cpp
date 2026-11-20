/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 * Copyright (c) 2022 Dhruva Gole
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <Arduino.h>
#include "wiring_private.h"

using namespace zephyr::arduino;

void yield(void) {
	k_yield();
}

unsigned long micros(void) {
#ifdef CONFIG_TIMER_HAS_64BIT_CYCLE_COUNTER
	return k_cyc_to_us_floor32(k_cycle_get_64());
#else
	return k_cyc_to_us_floor32(k_cycle_get_32());
#endif
}

unsigned long millis(void) {
	return k_uptime_get_32();
}

const struct device *digitalPinToPortDevice(pin_size_t pinNumber) {
	RETURN_ON_INVALID_PIN(pinNumber, nullptr);

	return local_gpio_port(pinNumber);
}

int digitalPinToPinIndex(pin_size_t pinNumber) {
	RETURN_ON_INVALID_PIN(pinNumber, -1);

	return local_gpio_pin(pinNumber);
}
