/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef CONFIG_USE_DFU_FADING_BLINK

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(fading_blink_led);

static const struct pwm_dt_spec pwm_led = PWM_DT_SPEC_GET(DT_ALIAS(fade_led));

#define FADE_DELAY_MS          10
#define FADE_STEPS             50
#define FADE_THREAD_STACK_SIZE 1024

K_THREAD_STACK_DEFINE(fade_led_stack, FADE_THREAD_STACK_SIZE);
static struct k_thread fade_led_thread;
static atomic_t fade_led_running;

static void blink_fade_led(void *arg0, void *arg1, void *arg2) {
	uint32_t pulse_width = 0;
	bool increasing = true;

	ARG_UNUSED(arg0);
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);

	if (!pwm_is_ready_dt(&pwm_led)) {
		LOG_ERR("Error: PWM device %s is not ready", pwm_led.dev->name);
		return;
	}

	uint32_t step_size = pwm_led.period / FADE_STEPS;

	while (atomic_get(&fade_led_running)) {
		int ret = pwm_set_pulse_dt(&pwm_led, pulse_width);
		if (ret < 0) {
			LOG_ERR("Error %d: failed to set pulse width", ret);
			break;
		}

		if (increasing) {
			pulse_width += step_size;

			if (pulse_width >= pwm_led.period) {
				pulse_width = pwm_led.period;
				increasing = false;
			}
		} else {
			if (pulse_width <= step_size) {
				pulse_width = 0;
				increasing = true;
			} else {
				pulse_width -= step_size;
			}
		}
		k_msleep(FADE_DELAY_MS);
	}

	int ret = pwm_set_pulse_dt(&pwm_led, 0);
	if (ret < 0) {
		LOG_ERR("Error %d: failed to turn off LED", ret);
	}
}

void fading_blink_led_start(void) {
	atomic_set(&fade_led_running, 1);
	k_thread_create(&fade_led_thread, fade_led_stack, K_THREAD_STACK_SIZEOF(fade_led_stack),
					blink_fade_led, NULL, NULL, NULL, 1, 0, K_NO_WAIT);
}

void fading_blink_led_stop(void) {
	atomic_clear(&fade_led_running);
	k_thread_join(&fade_led_thread, K_FOREVER);
}

#endif /* CONFIG_USE_DFU_FADING_BLINK */
