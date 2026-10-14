/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/uart/cdc_acm.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/sys/reboot.h>
#include <SerialUSB.h>

// extern "C" so the same symbol is shared by the (C) loader and (C++) core;
// variants provide a strong override in variant.c, which loader/CMakeLists.txt
// compiles into the loader as well as into the sketch.
extern "C" void __attribute__((weak)) _on_1200_bps() {
	sys_reboot(SYS_REBOOT_COLD);
}

#if ZARD_BOARD_HAS_SERIALUSB
const struct device *const usb_dev = DEVICE_DT_GET(ZARD_SERIALUSB_PHANDLE);


/*
 * Enter the bootloader off the USB callback context: both the 1200-bps touch and
 * the DFU run-time DETACH arrive in a control-transfer context, where acting
 * inline races the in-flight transfer.
 *
 * Deliberately does not disable USB - usbd_disable() blocks forever against the
 * bus reset dfu-util issues right after DETACH, deadlocking the system workqueue
 * that also runs Zephyr's runtime_detach_work. _on_1200_bps() drops the pull-up
 * and resets the SoC anyway.
 */
static void on_1200_touch_handler(struct k_work *work) {
	ARG_UNUSED(work);
	_on_1200_bps();
}
static K_WORK_DELAYABLE_DEFINE(on_1200_touch_work, on_1200_touch_handler);

// The work item is statically initialised once (above); re-initialising a
// pending item would corrupt the work queue.
static void schedule_bootloader_reset() {
	k_work_schedule(&on_1200_touch_work, K_MSEC(100));
}

void arduino::SerialUSB_::baudChangeHandler(const struct device *dev, uint32_t rate) {
	(void)dev; // unused
	if (rate == 1200) {
		schedule_bootloader_reset();
	}
}

int arduino::SerialUSB_::usb_disable() {
	// To avoid Cannot perform port reset: 1200-bps touch: setting DTR to OFF: protocol error
	k_sleep(K_MSEC(100));
	return usbd_disable(SerialUSB._usbd);
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
		uart_line_ctrl_get(SerialUSB.uart, UART_LINE_CTRL_BAUD_RATE, &baudrate);
		SerialUSB.baudChangeHandler(nullptr, baudrate);
	}

#if defined(CONFIG_USBD_DFU)
	if (msg->type == USBD_MSG_DFU_APP_DETACH) {
		// Host requested DFU detach (e.g. `dfu-util -e`): reboot into bootloader.
		schedule_bootloader_reset();
	}
#endif
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
	int old_dtr = dtr;
	int ret = uart_line_ctrl_get(uart, UART_LINE_CTRL_DTR, &dtr);
	ret = dtr && (ret == 0);
	if (ret && old_dtr == 0) {
		k_sleep(K_MSEC(1));
	}
	return ret;
}

size_t arduino::SerialUSB_::write(const uint8_t *buffer, size_t size) {
	if (!SerialUSB) {
		return 0;
	}
	return arduino::ZephyrSerial::write(buffer, size);
}

void arduino::SerialUSB_::flush() {
	if (!SerialUSB) {
		return;
	}
	arduino::ZephyrSerial::flush();
}

arduino::SerialUSB_ SerialUSB(usb_dev);

#if ZARD_FIRST_SERIAL_IS_SERIALUSB
extern arduino::SerialUSB_ Serial [[gnu::alias("SerialUSB")]];
#endif

#endif
