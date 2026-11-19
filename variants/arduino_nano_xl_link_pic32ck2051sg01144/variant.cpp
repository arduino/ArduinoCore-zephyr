/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <Arduino.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

extern "C" void nano_xl_link_request_dfu(void);

/* The core's weak default has C++ linkage; the request itself is in variant.c,
 * shared with the loader. */
void _on_1200_bps() {
    nano_xl_link_request_dfu();
}

void initVariant(void) {
    /* Set leds inactive */
    const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), builtin_led_gpios, 1);
    const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), builtin_led_gpios, 2);
    const struct gpio_dt_spec led3 = GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), builtin_led_gpios, 3);
    if (!gpio_is_ready_dt(&led1) || !gpio_is_ready_dt(&led2) || !gpio_is_ready_dt(&led3)) {
        return;
    }

    gpio_pin_configure_dt(&led1, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led2, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led3, GPIO_OUTPUT_INACTIVE);
}
