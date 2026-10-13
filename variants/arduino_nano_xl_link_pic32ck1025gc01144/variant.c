/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <cmsis_core.h>

#include "recovery_cookie.h"
#include "usb_soft_disconnect.h"

/* Absolute slot addresses must agree with the MCUboot overlay; mismatches are
 * caught by BUILD_ASSERTs in boot/arduino/hooks/hooks.c. */
#define SLOT_ABS(label) \
	(DT_REG_ADDR(DT_GPARENT(DT_NODELABEL(label))) + DT_REG_ADDR(DT_NODELABEL(label)))

BUILD_ASSERT(SLOT_ABS(slot0_partition) == 0x0C000000,
	     "slot0 moved: keep this overlay and the MCUboot board overlay in sync");
BUILD_ASSERT(SLOT_ABS(slot1_partition) == 0x0C080000,
	     "slot1 moved: keep this overlay and the MCUboot board overlay in sync");

/* Arm the MCUboot double-reset cookie and reset into USB DFU recovery.
 * Plain C so the loader can compile it (loader/CMakeLists.txt builds variant.c only). */
void _on_1200_bps(void) {
	RECOVERY_COOKIE = RECOVERY_DOUBLE_RESET_MAGIC;

	/* Flush the write out of any store buffer before the core is reset. */
	__DSB();

	usb_soft_disconnect();

	NVIC_SystemReset();
}
