/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <cmsis_core.h>

#include "recovery_cookie.h"
#include "usb_soft_disconnect.h"

/*
 * The bootloader expresses these same slots as offsets into flash0 (see
 * extra/mcuboot/nano_chandler_bfm.overlay, which needs one flash device for
 * MCUboot and its slots); here they are offsets into flash1. Only the absolute
 * addresses have to agree, and nothing else checks that they do. The matching
 * assertions live in boot/arduino/hooks/hooks.c (mcuboot fork), so whichever side moves is
 * the side that fails to build.
 *
 * user_sketch is a window into the middle of slot0, not a region of its own, so
 * it is pinned to slot0 rather than to a literal.
 */
#define SLOT_ABS(label) \
	(DT_REG_ADDR(DT_GPARENT(DT_NODELABEL(label))) + DT_REG_ADDR(DT_NODELABEL(label)))

BUILD_ASSERT(SLOT_ABS(slot0_partition) == 0x0C000000,
	     "slot0 moved: keep this overlay and mcuboot.overlay in sync");
BUILD_ASSERT(SLOT_ABS(user_sketch) == 0x0C048000,
	     "user_sketch moved: keep this overlay and mcuboot.overlay in sync");
BUILD_ASSERT(SLOT_ABS(user_sketch) == SLOT_ABS(slot0_partition) + DT_REG_SIZE(DT_NODELABEL(slot0_partition)),
	     "user_sketch must start immediately after slot0");

/*
 * Enter MCUboot serial recovery. Called from the deferred work item in
 * cores/arduino/USB.cpp (sketch) or loader/main.c (loader) after a 1200-bps
 * touch.
 *
 * A plain reset is not enough on this board: the reset vector belongs to
 * MCUboot in BFM, which chainloads the loader unless it finds its double-reset
 * cookie set. Arming that cookie first makes the reset indistinguishable from
 * the user physically double-tapping reset, so MCUboot enters serial recovery
 * instead of booting on.
 *
 * This lives in variant.c rather than variant.cpp on purpose: loader/CMakeLists.txt
 * compiles only the plain-C variant.c into the loader, and the loader is where
 * this has to work when no sketch is running.
 */
void _on_1200_bps(void) {
	RECOVERY_COOKIE = RECOVERY_DOUBLE_RESET_MAGIC;

	/* Flush the write out of any store buffer before the core is reset. */
	__DSB();

	usb_soft_disconnect();

	NVIC_SystemReset();
}
