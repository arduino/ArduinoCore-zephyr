/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/retention/bootmode.h>
#include <zephyr/sys/reboot.h>

/* Ask for USB DFU: the loader's own (CONFIG_LOADER_DFU_MODE) or MCUboot's.
 * Plain C: the loader builds variant.c too. */
void nano_xl_link_request_dfu(void) {
#ifdef CONFIG_LOADER_DFU_MODE
	(void)bootmode_set(CONFIG_LOADER_DFU_BOOT_MODE);
#else
	(void)bootmode_set(BOOT_MODE_TYPE_BOOTLOADER);
#endif
	sys_reboot(SYS_REBOOT_WARM);
}

/* The loader's override; sketches use variant.cpp's C++ one. */
void _on_1200_bps(void) {
	nano_xl_link_request_dfu();
}
