/*
 * Copyright (c) 2024 Arduino SA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <zephyrSerial.h>

#if defined(CONFIG_USB_DEVICE_STACK_NEXT)
#include <zephyr/usb/usbd.h>
extern "C" struct usbd_context *usbd_init_device(usbd_msg_cb_t msg_cb);
#endif

namespace arduino {

class SerialUSB_ : public ZephyrSerial {

public:
	SerialUSB_(const struct device *dev) : ZephyrSerial(dev) {
	}

	void begin(unsigned long baudrate, uint16_t config);

	void begin(unsigned long baudrate) {
		begin(baudrate, SERIAL_8N1);
	}

	operator bool() override;
	size_t write(const uint8_t *buffer, size_t size) override;

	size_t write(const uint8_t data) override {
		return write(&data, 1);
	}

	void flush() override;

protected:
	uint32_t dtr = 0;
	uint32_t baudrate;
	static void baudChangeHandler(const struct device *dev, uint32_t rate);

private:
	bool started = false;

#if defined(CONFIG_USB_DEVICE_STACK_NEXT)
	struct usbd_context *_usbd;
	int enable_usb_device_next();
	static void usbd_next_cb(struct usbd_context *const ctx, const struct usbd_msg *msg);
	static int usb_disable();
#endif
};
} // namespace arduino

#if (DT_NODE_HAS_PROP(DT_PATH(zephyr_user), cdc_acm) &&                                            \
	 (CONFIG_USB_CDC_ACM || CONFIG_USBD_CDC_ACM_CLASS))
extern arduino::SerialUSB_ Serial;
#endif
