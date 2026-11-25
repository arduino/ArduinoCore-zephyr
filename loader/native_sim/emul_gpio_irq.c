/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Interrupt source for native_sim: --gpio_irq=<list> drives the input levels
 * of emulated GPIOs at given simulated times, so that the handlers attached
 * with attachInterrupt() run. The list is comma-separated <pin>@<ms>:<level>
 * items, e.g. 2@100:1,2@250:0: <pin> is an Arduino pin number, <ms> the time
 * after the loader starts (non-decreasing along the list), <level> 0 or 1.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <cmdline.h>
#include <posix_board_if.h>
#include <posix_native_task.h>

#define MAX_EVENTS 64

static const struct gpio_dt_spec pins[] = {
	DT_FOREACH_PROP_ELEM_SEP(DT_PATH(zephyr_user), digital_pin_gpios, GPIO_DT_SPEC_GET_BY_IDX,
							 (, ))};

struct irq_event {
	const struct gpio_dt_spec *pin;
	uint32_t ms;
	bool level;
};

static char *irq_arg;
static struct irq_event events[MAX_EVENTS];
static size_t n_events;
static size_t next_event;
static uint32_t prev_ms;
static struct k_timer irq_timer;

/* Fire the next event now: the input is set, the callbacks of the pin run */
static void fire_next(void) {
	const struct irq_event *ev = &events[next_event];

	gpio_emul_input_set(ev->pin->port, ev->pin->pin, ev->level);
	prev_ms = ev->ms;
	next_event++;
}

/* Fire the events due now, and arm the timer for the next one that is not */
static void run_events(void) {
	while (next_event < n_events) {
		const uint32_t delta = events[next_event].ms - prev_ms;

		if (delta > 0) {
			k_timer_start(&irq_timer, K_MSEC(delta), K_NO_WAIT);
			return;
		}
		fire_next();
	}
}

static void irq_timer_expiry(struct k_timer *timer) {
	ARG_UNUSED(timer);

	fire_next();
	run_events();
}

static void gpio_irq_options(void) {
	static struct args_struct_t gpio_irq_options[] = {
		{
			.option = "gpio_irq",
			.name = "events",
			.type = 's',
			.dest = (void *)&irq_arg,
			.descript = "Input events <pin>@<ms>:<level>,... on Arduino pins, in simulated time",
		},
		ARG_TABLE_ENDMARKER,
	};

	native_add_command_line_opts(gpio_irq_options);
}

NATIVE_TASK(gpio_irq_options, PRE_BOOT_1, 1);

/* Parse the whole list into events; -1 if it is malformed */
static int parse_events(const char *arg) {
	const char *p = arg;
	long last_ms = 0;

	while (true) {
		char *end;
		long pin;
		long ms;
		long level;

		pin = strtol(p, &end, 0);
		if (end == p || *end != '@' || pin < 0 || (size_t)pin >= ARRAY_SIZE(pins)) {
			return -1;
		}
		p = end + 1;

		ms = strtol(p, &end, 0);
		if (end == p || *end != ':' || ms < last_ms) {
			return -1;
		}
		p = end + 1;

		level = strtol(p, &end, 0);
		if (end == p || (level != 0 && level != 1)) {
			return -1;
		}
		p = end;

		if (n_events == MAX_EVENTS) {
			return -1;
		}
		events[n_events].pin = &pins[pin];
		events[n_events].ms = (uint32_t)ms;
		events[n_events].level = level != 0;
		n_events++;
		last_ms = ms;

		if (*p == '\0') {
			return 0;
		}
		if (*p != ',') {
			return -1;
		}
		p++;
	}
}

static int gpio_irq_init(void) {
	if (irq_arg == NULL) {
		return 0;
	}

	if (parse_events(irq_arg) != 0) {
		printk("gpio_irq: bad argument '%s', expected <pin>@<ms>:<level>,...\n", irq_arg);
		posix_exit(1);
	}

	k_timer_init(&irq_timer, irq_timer_expiry, NULL);
	run_events();
	return 0;
}

SYS_INIT(gpio_irq_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
