/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <Arduino.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>

LOG_MODULE_REGISTER(variant, LOG_LEVEL_WRN);

/* _on_1200_bps() is in variant.c (plain C, shared with the loader). */

#if DT_HAS_COMPAT_STATUS_OKAY(espressif_esp_at)
/*
 * Connector carrier: the ESP8266 AT module sits on an external connector and
 * may be missing, so its device is zephyr,deferred-init (connector.overlay):
 * WiFi.status() initializes it on first use. Only the network interface is
 * prepared here, and only for a module that answers with a supported firmware;
 * otherwise the interface stays unusable and WiFi reports WL_NO_SHIELD.
 */
#define ESP_AT_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(espressif_esp_at)

/* Oldest ESP-AT firmware the driver works with (it needs AT+CIPRECVMODE). */
static constexpr int espAtMinMajor = 1;
static constexpr int espAtMinMinor = 7;
static constexpr int64_t espAtProbeTimeoutMs = 1000;

/*
 * Read "AT version:<major>.<minor>..." with AT+GMR, talking to the module's
 * UART directly while the ESP-AT driver is not attached to it. Returns 0 on
 * success, -ENODEV if the module does not answer.
 */
static int espAtProbeVersion(int &major, int &minor) {
    static constexpr char prefix[] = "AT version:";
    const struct device *uart = DEVICE_DT_GET(DT_BUS(ESP_AT_NODE));
    unsigned char c;
    char line[48];
    size_t len = 0;

    if (!device_is_ready(uart)) {
        return -ENODEV;
    }

    while (uart_poll_in(uart, &c) == 0) {
    }
    for (const char *p = "AT+GMR\r\n"; *p != '\0'; p++) {
        uart_poll_out(uart, *p);
    }

    /* Busy-poll: the UART has no buffer while the driver is not attached. */
    const int64_t deadline = k_uptime_get() + espAtProbeTimeoutMs;
    while (k_uptime_get() < deadline) {
        if (uart_poll_in(uart, &c) != 0) {
            continue;
        }
        if (c != '\n') {
            if (c != '\r' && len < sizeof(line) - 1) {
                line[len++] = (char)c;
            }
            continue;
        }
        line[len] = '\0';
        len = 0;
        if (strncmp(line, prefix, sizeof(prefix) - 1) != 0) {
            continue;
        }
        const char *v = line + sizeof(prefix) - 1;
        major = 0;
        minor = 0;
        while (*v >= '0' && *v <= '9') {
            major = major * 10 + (*v++ - '0');
        }
        if (*v++ != '.') {
            return -EINVAL;
        }
        while (*v >= '0' && *v <= '9') {
            minor = minor * 10 + (*v++ - '0');
        }
        return 0;
    }

    return -ENODEV;
}

/*
 * net_if_init() skipped the module's interface at boot (device not ready) and
 * Zephyr has no call to initialize it later, so do what init_iface() in
 * subsys/net/ip/net_if.c does. This depends on net_if internals: recheck it on
 * Zephyr updates.
 */
static void espAtPrepareIface() {
    const struct device *dev = DEVICE_DT_GET(ESP_AT_NODE);
    int major;
    int minor;
    int ret;

    if (device_is_ready(dev)) {
        return;
    }

    ret = espAtProbeVersion(major, minor);
    if (ret != 0) {
        LOG_WRN("ESP-AT module not responding (%d)", ret);
        return;
    }
    if (major < espAtMinMajor || (major == espAtMinMajor && minor < espAtMinMinor)) {
        LOG_ERR("ESP-AT firmware %d.%d, %d.%d or later required", major, minor, espAtMinMajor,
                espAtMinMinor);
        return;
    }

    struct net_if *iface = net_if_lookup_by_dev(dev);
    if (iface == nullptr) {
        return;
    }

    if (IS_ENABLED(CONFIG_NET_NATIVE_IPV4)) {
        net_if_flag_set(iface, NET_IF_IPV4);
    }
    if (IS_ENABLED(CONFIG_NET_NATIVE_IPV6)) {
        net_if_flag_set(iface, NET_IF_IPV6);
    }
    k_mutex_init(&iface->lock);
    k_mutex_init(&iface->tx_lock);
    static_cast<const struct net_if_api *>(dev->api)->init(iface);
}
#endif

void initVariant(void) {
#if DT_HAS_COMPAT_STATUS_OKAY(espressif_esp_at)
    espAtPrepareIface();
#endif

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
