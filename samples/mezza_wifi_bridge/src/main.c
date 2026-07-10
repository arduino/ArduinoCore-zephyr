/*
 * Copyright 2025 Google LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sample_usbd.h>

#include <stdio.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart/uart_bridge.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>

#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cdc_acm_bridge, LOG_LEVEL_INF);

const struct device *const uart_dev = DEVICE_DT_GET_ONE(zephyr_cdc_acm_uart);

static struct usbd_context *sample_usbd;

#define WIFI_BOOT_NODE DT_NODELABEL(wifi_boot_gpio)
#define WIFI_EN_NODE   DT_NODELABEL(wifi_chip_en_gpio)

#if DT_NODE_EXISTS(WIFI_BOOT_NODE) && DT_NODE_EXISTS(WIFI_EN_NODE)
#define WIFI_GPIO_SPEC_FROM_LOCAL_CELLS(node_id)                                                   \
	{                                                                                            \
		.port = DEVICE_DT_GET(DT_PARENT(node_id)),                                            \
		.pin = DT_PROP_BY_IDX(node_id, gpios, 0),                                             \
		.dt_flags = DT_PROP_BY_IDX(node_id, gpios, 1),                                        \
	}

static const struct gpio_dt_spec wifi_boot = WIFI_GPIO_SPEC_FROM_LOCAL_CELLS(WIFI_BOOT_NODE);
static const struct gpio_dt_spec wifi_en = WIFI_GPIO_SPEC_FROM_LOCAL_CELLS(WIFI_EN_NODE);
#endif

#define DEVICE_DT_GET_COMMA(node_id) DEVICE_DT_GET(node_id),

const struct device *uart_bridges[] = {
	DT_FOREACH_STATUS_OKAY(zephyr_uart_bridge, DEVICE_DT_GET_COMMA)
};

static void sample_msg_cb(struct usbd_context *const ctx, const struct usbd_msg *msg)
{
	LOG_INF("USBD message: %s", usbd_msg_type_string(msg->type));

	if (usbd_can_detect_vbus(ctx)) {
		if (msg->type == USBD_MSG_VBUS_READY) {
			if (usbd_enable(ctx)) {
				LOG_ERR("Failed to enable device support");
			}
		}

		if (msg->type == USBD_MSG_VBUS_REMOVED) {
			if (usbd_disable(ctx)) {
				LOG_ERR("Failed to disable device support");
			}
		}
	}

	if (msg->type == USBD_MSG_CDC_ACM_LINE_CODING ||
	    msg->type == USBD_MSG_CDC_ACM_CONTROL_LINE_STATE) {
		for (uint8_t i = 0; i < ARRAY_SIZE(uart_bridges); i++) {
			/* update all bridges, non valid combinations are
			 * skipped automatically.
			 */
			uart_bridge_settings_update(msg->dev, uart_bridges[i]);
		}
	}
}

int main(void)
{
	int err;

#if DT_NODE_EXISTS(WIFI_BOOT_NODE) && DT_NODE_EXISTS(WIFI_EN_NODE)
	if (!gpio_is_ready_dt(&wifi_boot) || !gpio_is_ready_dt(&wifi_en)) {
		LOG_ERR("Wi-Fi GPIO controller not ready");
		return -ENODEV;
	}

	//gpio_pin_configure_dt(&wifi_en, GPIO_OUTPUT);
	//gpio_pin_configure_dt(&wifi_boot, GPIO_OUTPUT);
	printk("Wi-Fi GPIOs configured, boot=%d, en=%d\n", wifi_boot.pin, wifi_en.pin);
	printk("Wi-Fi GPIOs configured done\n");
#endif

	sample_usbd = sample_usbd_init_device(sample_msg_cb);
	if (sample_usbd == NULL) {
		LOG_ERR("Failed to initialize USB device");
		return -ENODEV;
	}

	if (!usbd_can_detect_vbus(sample_usbd)) {
		err = usbd_enable(sample_usbd);
		if (err) {
			LOG_ERR("Failed to enable device support");
			return err;
		}
	}

	const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(button), gpios);
	gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (gpio_pin_get_dt(&button) == 1) {
		gpio_pin_set_dt(&wifi_boot, 1);
	}

	uint32_t dtr = 0U;
	while (dtr == 0U) {
		uart_line_ctrl_get(uart_dev, UART_LINE_CTRL_DTR, &dtr);
	}
	gpio_pin_set_dt(&wifi_en, 1);

	LOG_INF("USB device support enabled");

	k_sleep(K_FOREVER);

	return 0;
}
