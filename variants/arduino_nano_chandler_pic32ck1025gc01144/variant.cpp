/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

/* _on_1200_bps() now lives in variant.c: it has to be plain C so the loader
 * builds it too (loader/CMakeLists.txt compiles only variant.c), and the loader
 * is exactly the case that matters - no sketch running, host sends 1200-bps touch. */

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
