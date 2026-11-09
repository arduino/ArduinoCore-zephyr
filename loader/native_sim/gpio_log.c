/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Print the level of emulated GPIO outputs when they are written or
 * configured, so that tests can follow what a sketch does with its pins.
 * gpio_emul also calls the registered callbacks on input events and on
 * pin configuration: only pins currently configured as outputs are
 * printed.
 * gpio_emul does not call the callbacks when a pin configured as both input
 * and output is written, so such pins are turned into plain outputs, as
 * with drivers that cannot do both.
 */

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/init.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#define GPIO_EMUL_DEV(node_id) DEVICE_DT_GET(node_id),

static const struct device *const gpio_emul_devs[] = {
	DT_FOREACH_STATUS_OKAY(zephyr_gpio_emul, GPIO_EMUL_DEV)};

static struct gpio_callback gpio_log_cbs[ARRAY_SIZE(gpio_emul_devs)];

static void gpio_log_handler(const struct device *port, struct gpio_callback *cb,
							 gpio_port_pins_t pins) {
	gpio_port_pins_t outputs = 0;
	gpio_port_value_t values = 0;

	ARG_UNUSED(cb);

	/* gpio_emul does not call the callbacks when a pin configured as both input
	 * and output is written, so such pins are turned into plain outputs.
	 */
	for (gpio_pin_t pin = 0; pin < GPIO_MAX_PINS_PER_PORT; pin++) {
		gpio_flags_t flags;

		if ((pins & BIT(pin)) == 0U || gpio_emul_flags_get(port, pin, &flags) != 0) {
			continue;
		}
		if ((flags & (GPIO_INPUT | GPIO_OUTPUT)) == (GPIO_INPUT | GPIO_OUTPUT)) {
			(void)gpio_pin_configure(port, pin, flags & ~GPIO_INPUT);
			/* printed by the above configuration callback, not here */
			pins &= ~BIT(pin);
		}
	}

	if (gpio_port_get_direction(port, pins, NULL, &outputs) != 0) {
		return;
	}
	pins &= outputs;

	if (gpio_emul_output_get_masked(port, pins, &values) != 0) {
		return;
	}

	for (gpio_pin_t pin = 0; pins != 0U; pin++, pins >>= 1) {
		if ((pins & 1U) != 0U) {
			printk("[gpio] %s pin %u = %u\n", port->name, pin,
				   (unsigned int)((values >> pin) & 1U));
		}
	}
}

static int gpio_log_init(void) {
	for (size_t i = 0; i < ARRAY_SIZE(gpio_emul_devs); i++) {
		if (!device_is_ready(gpio_emul_devs[i])) {
			continue;
		}
		gpio_init_callback(&gpio_log_cbs[i], gpio_log_handler, UINT32_MAX);
		(void)gpio_add_callback(gpio_emul_devs[i], &gpio_log_cbs[i]);
	}

	return 0;
}

SYS_INIT(gpio_log_init, APPLICATION, 99);
