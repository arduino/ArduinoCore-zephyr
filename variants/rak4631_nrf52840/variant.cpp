/*
 * Copyright (c) 2026 RAKwireless Technology Limited
 * Author: sercan.erat@rakwireless.com
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/retention/bootmode.h>
#include <zephyr/sys/reboot.h>

/* MCUboot sees the boot mode after the reset and enters serial recovery. */
void _on_1200_bps() {
	(void)bootmode_set(BOOT_MODE_TYPE_BOOTLOADER);
	sys_reboot(SYS_REBOOT_COLD);
}
