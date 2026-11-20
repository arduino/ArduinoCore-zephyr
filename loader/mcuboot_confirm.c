/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * MCUboot swap modes: confirm a running image that MCUboot cannot revert.
 *
 * MCUboot reverts an unconfirmed image only when its trailer has copy_done
 * set, which every swap writes. A trailer with the magic but without
 * image_ok and copy_done is left by a copy (bootstrap, or a permanent USB DFU
 * update through the image check hook) interrupted between its last trailer
 * writes. MCUboot boots that image for good, but it reads as not confirmed:
 * the trial window would reboot it forever and OTA would refuse to start.
 */

#if defined(CONFIG_MCUBOOT_IMG_MANAGER) &&                                                         \
	(defined(CONFIG_MCUBOOT_BOOTLOADER_MODE_SWAP_USING_OFFSET) ||                                  \
	 defined(CONFIG_MCUBOOT_BOOTLOADER_MODE_SWAP_USING_MOVE) ||                                    \
	 defined(CONFIG_MCUBOOT_BOOTLOADER_MODE_SWAP_SCRATCH))

#include <zephyr/dfu/mcuboot.h>
#include <zephyr/init.h>
#include <zephyr/storage/flash_map.h>
#include <bootutil/bootutil_public.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(mcuboot_confirm);

static int mcuboot_confirm_unrevertable(void) {
	struct boot_swap_state state;

	if (boot_is_img_confirmed()) {
		return 0;
	}

	if (boot_read_swap_state_by_id(PARTITION_ID(slot0_partition), &state) != 0 ||
		state.copy_done == BOOT_FLAG_SET) {
		/* A test image: the trial window applies. */
		return 0;
	}

	LOG_WRN("Image cannot be reverted, confirming it");
	(void)boot_write_img_confirmed();

	return 0;
}

/* Before the watchdog thread starts the trial window and before the sketch. */
SYS_INIT(mcuboot_confirm_unrevertable, APPLICATION, 0);

#endif
