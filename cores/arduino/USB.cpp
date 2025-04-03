/*
 * Copyright (c) 2024 Arduino SA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/uart/cdc_acm.h>
#include <zephyr/usb/usb_device.h>
#include <SerialUSB.h>

#if ((DT_NODE_HAS_PROP(DT_PATH(zephyr_user), cdc_acm)) &&                                          \
	 (CONFIG_USB_CDC_ACM || CONFIG_USBD_CDC_ACM_CLASS))
const struct device *const usb_dev =
	DEVICE_DT_GET(DT_PHANDLE_BY_IDX(DT_PATH(zephyr_user), cdc_acm, 0));

void __attribute__((weak)) _on_1200_bps() {
	NVIC_SystemReset();
}

void arduino::SerialUSB_::baudChangeHandler(const struct device *dev, uint32_t rate) {
	(void)dev; // unused
	if (rate == 1200) {
		usb_disable();
		_on_1200_bps();
	}
}

#if defined(CONFIG_USB_DEVICE_STACK_NEXT)
int arduino::SerialUSB_::usb_disable() {
	return usbd_disable(Serial._usbd);
}

void arduino::SerialUSB_::usbd_next_cb(struct usbd_context *const ctx, const struct usbd_msg *msg) {
	if (usbd_can_detect_vbus(ctx)) {
		if (msg->type == USBD_MSG_VBUS_READY) {
			usbd_enable(ctx);
		}

		if (msg->type == USBD_MSG_VBUS_REMOVED) {
			usbd_disable(ctx);
		}
	}

	if (msg->type == USBD_MSG_CDC_ACM_LINE_CODING) {
		uint32_t baudrate;
		uart_line_ctrl_get(Serial.uart, UART_LINE_CTRL_BAUD_RATE, &baudrate);
		Serial.baudChangeHandler(nullptr, baudrate);
	}
}

int arduino::SerialUSB_::enable_usb_device_next(void) {
	int err;

	_usbd = usbd_init_device(arduino::SerialUSB_::usbd_next_cb);
	if (_usbd == NULL) {
		return -ENODEV;
	}

	if (!usbd_can_detect_vbus(_usbd)) {
		err = usbd_enable(_usbd);
		if (err) {
			return err;
		}
	}
	return 0;
}
#endif /* defined(CONFIG_USB_DEVICE_STACK_NEXT) */

void arduino::SerialUSB_::begin(unsigned long baudrate, uint16_t config) {
	if (!started) {
#ifndef CONFIG_USB_DEVICE_STACK_NEXT
		usb_enable(NULL);
#ifndef CONFIG_CDC_ACM_DTE_RATE_CALLBACK_SUPPORT
#warning "Can't read CDC baud change, please enable CONFIG_CDC_ACM_DTE_RATE_CALLBACK_SUPPORT"
#else
		cdc_acm_dte_rate_callback_set(usb_dev, SerialUSB_::baudChangeHandler);
#endif
#else
		enable_usb_device_next();
#endif
		ZephyrSerial::begin(baudrate, config);
		started = true;
	}
}

arduino::SerialUSB_::operator bool() {
	uart_line_ctrl_get(uart, UART_LINE_CTRL_DTR, &dtr);
	return dtr;
}

size_t arduino::SerialUSB_::write(const uint8_t *buffer, size_t size) {
	if (!Serial) {
		return 0;
	}
	return arduino::ZephyrSerial::write(buffer, size);
}

void arduino::SerialUSB_::flush() {
	if (!Serial) {
		return;
	}
	arduino::ZephyrSerial::flush();
}

arduino::SerialUSB_ Serial(usb_dev);
#endif
