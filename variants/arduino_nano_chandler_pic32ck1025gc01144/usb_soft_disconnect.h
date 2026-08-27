/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Drop the USB D+ pull-up before a warm reset.
 *
 * NVIC_SystemReset() does not reset USBHS, so a reset taken while enumerated
 * leaves SOFTCONN asserted, the host sees no detach/attach edge, and the next
 * image comes up running but invisible on USB.
 *
 * usbd_disable() cannot be used instead: it blocks forever if a USB bus reset is
 * in flight, and it deadlocks the system workqueue. Poking SOFTCONN directly is
 * safe from any context.
 */

#pragma once

#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/sys_io.h>
#include <zephyr/sys/util.h>

/* USBHS_POWER, offset 0x1001, SOFTCONN at bit 6 (see the SoC's usbhs.h). */
#define USB_USBHS_POWER_ADDR (DT_REG_ADDR(DT_NODELABEL(zephyr_udc0)) + 0x1001U)
#define USB_USBHS_POWER_SOFTCONN BIT(6)

/*
 * The host needs to see SE0 for longer than its disconnect debounce before the
 * warm reset takes the bus away.
 */
#define USB_DISCONNECT_SETTLE_US 20000U

static inline void usb_soft_disconnect(void)
{
	uint8_t power = sys_read8(USB_USBHS_POWER_ADDR);

	sys_write8(power & ~USB_USBHS_POWER_SOFTCONN, USB_USBHS_POWER_ADDR);
	__DSB();

	k_busy_wait(USB_DISCONNECT_SETTLE_US);
}
