/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 * Copyright (c) 2024 Ayush Singh <ayush@beagleboard.org>
 * Copyright (c) 2026 TOKITA Hiroshi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <Arduino.h>
#include "wiring_private.h"

using namespace zephyr::arduino;

unsigned long pulseIn(pin_size_t pinNumber, uint8_t state, unsigned long timeout) {
	RETURN_ON_INVALID_PIN(pinNumber, LOW);

	struct k_timer timer;
	int64_t start, end, delta = 0;
	const struct gpio_dt_spec *spec = &arduino_pins[pinNumber];

	if (!gpio_is_ready_dt(spec)) {
		return 0;
	}

	k_timer_init(&timer, NULL, NULL);
	k_timer_start(&timer, K_USEC(timeout), K_NO_WAIT);

	while (gpio_pin_get_dt(spec) == state && k_timer_status_get(&timer) == 0)
		;
	if (k_timer_status_get(&timer) > 0) {
		goto cleanup;
	}

	while (gpio_pin_get_dt(spec) != state && k_timer_status_get(&timer) == 0)
		;
	if (k_timer_status_get(&timer) > 0) {
		goto cleanup;
	}

	start = k_uptime_ticks();
	while (gpio_pin_get_dt(spec) == state && k_timer_status_get(&timer) == 0)
		;
	if (k_timer_status_get(&timer) > 0) {
		goto cleanup;
	}
	end = k_uptime_ticks();

	delta = k_ticks_to_us_floor64(end - start);

cleanup:
	k_timer_stop(&timer);
	return (unsigned long)delta;
}
