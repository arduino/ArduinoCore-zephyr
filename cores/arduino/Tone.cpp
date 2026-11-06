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

namespace {

#if CONFIG_ARDUINO_MAX_TONES < 0
#if ZARD_DT_HAS_USER_PINS
#define MAX_TONE_PINS DT_PROP_LEN(DT_PATH(zephyr_user), digital_pin_gpios)
#elif defined(ZARD_CONNECTOR)
#define MAX_TONE_PINS DT_PROP_LEN(DT_NODELABEL(ZARD_CONNECTOR), gpio_map)
#else
#define MAX_TONE_PINS 1
#endif
#else
#define MAX_TONE_PINS CONFIG_ARDUINO_MAX_TONES
#endif

#define TOGGLES_PER_CYCLE 2ULL

static struct pin_timer {
	struct k_timer timer;
	uint32_t count{0};
	pin_size_t pin{invalid_pin_number};
	bool infinity{false};
	bool timer_initialized{false};
	struct k_spinlock lock{};
} arduino_pin_timers[MAX_TONE_PINS];

K_MUTEX_DEFINE(timer_cfg_lock);

void tone_expiry_cb(struct k_timer *timer);

/* Callers must hold timer_cfg_lock while using this helper. */
static struct pin_timer *find_pin_timer(pin_size_t pinNumber, bool active_only) {
	for (size_t i = 0; i < ARRAY_SIZE(arduino_pin_timers); i++) {
		k_spinlock_key_t key = k_spin_lock(&arduino_pin_timers[i].lock);

		if (arduino_pin_timers[i].pin == pinNumber) {
			k_spin_unlock(&arduino_pin_timers[i].lock, key);
			return &arduino_pin_timers[i];
		}

		k_spin_unlock(&arduino_pin_timers[i].lock, key);
	}

	if (active_only) {
		return nullptr;
	}

	for (size_t i = 0; i < ARRAY_SIZE(arduino_pin_timers); i++) {
		k_spinlock_key_t key = k_spin_lock(&arduino_pin_timers[i].lock);

		if (arduino_pin_timers[i].pin == invalid_pin_number) {
			arduino_pin_timers[i].pin = pinNumber;
			k_spin_unlock(&arduino_pin_timers[i].lock, key);
			return &arduino_pin_timers[i];
		}

		k_spin_unlock(&arduino_pin_timers[i].lock, key);
	}

	return nullptr;
}

void tone_expiry_cb(struct k_timer *timer) {
	struct pin_timer *pt = CONTAINER_OF(timer, struct pin_timer, timer);
	k_spinlock_key_t key = k_spin_lock(&pt->lock);
	const struct device *port = local_gpio_port(pt->pin);

	if (pt->count == 0 && !pt->infinity) {
		if (port) {
			gpio_pin_set(port, local_gpio_pin(pt->pin), 0);
		}

		k_timer_stop(timer);
		pt->count = 0;
		pt->infinity = false;
		pt->pin = invalid_pin_number;
	} else {
		if (port) {
			gpio_pin_toggle(port, local_gpio_pin(pt->pin));
		}
		pt->count--;
	}

	k_spin_unlock(&pt->lock, key);
}

} // anonymous namespace

void tone(pin_size_t pinNumber, unsigned int frequency, unsigned long duration) {
	RETURN_ON_INVALID_PIN(pinNumber);

	k_spinlock_key_t key;
	uint64_t toggles_count;
	struct pin_timer *pt;
	k_timeout_t timeout;

	if (k_is_in_isr()) {
		return;
	}

	k_mutex_lock(&timer_cfg_lock, K_FOREVER);

	pt = find_pin_timer(pinNumber, false);

	if (pt == nullptr) {
		k_mutex_unlock(&timer_cfg_lock);
		return;
	}

	if (!pt->timer_initialized) {
		k_timer_init(&pt->timer, tone_expiry_cb, NULL);
		pt->timer_initialized = true;
	}

	pinMode(pinNumber, OUTPUT);
	k_timer_stop(&pt->timer);

	toggles_count = ((uint64_t)duration * frequency / (MSEC_PER_SEC / TOGGLES_PER_CYCLE));
	if (frequency == 0 || (toggles_count == 0 && duration != 0)) {
		key = k_spin_lock(&pt->lock);
		pt->count = 0;
		pt->infinity = false;
		pt->pin = invalid_pin_number;
		k_spin_unlock(&pt->lock, key);

		gpio_pin_set(local_gpio_port(pinNumber), local_gpio_pin(pinNumber), 0);

		k_mutex_unlock(&timer_cfg_lock);
		return;
	}

	timeout = K_NSEC(NSEC_PER_SEC / (TOGGLES_PER_CYCLE * frequency));
	if (timeout.ticks == 0) {
		timeout.ticks = 1;
	}

	key = k_spin_lock(&pt->lock);
	pt->infinity = (duration == 0);
	pt->count = min(toggles_count, UINT32_MAX);
	pt->pin = pinNumber;
	k_spin_unlock(&pt->lock, key);

	gpio_pin_set(local_gpio_port(pinNumber), local_gpio_pin(pinNumber), 1);

	k_timer_start(&pt->timer, timeout, timeout);

	k_mutex_unlock(&timer_cfg_lock);
}

void noTone(pin_size_t pinNumber) {
	RETURN_ON_INVALID_PIN(pinNumber);

	struct pin_timer *pt;
	k_spinlock_key_t key;

	if (k_is_in_isr()) {
		return;
	}

	k_mutex_lock(&timer_cfg_lock, K_FOREVER);

	pt = find_pin_timer(pinNumber, true);

	if (pt == nullptr) {
		k_mutex_unlock(&timer_cfg_lock);
		return;
	}

	key = k_spin_lock(&pt->lock);
	k_timer_stop(&pt->timer);
	pt->count = 0;
	pt->infinity = false;
	pt->pin = invalid_pin_number;
	k_spin_unlock(&pt->lock, key);

	gpio_pin_set(local_gpio_port(pinNumber), local_gpio_pin(pinNumber), 0);

	k_mutex_unlock(&timer_cfg_lock);
}
