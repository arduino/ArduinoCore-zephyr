/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#if defined(CONFIG_OTA_BACKEND_MCUBOOT) || defined(CONFIG_LOADER_DFU_MODE)

#include <zephyr/dfu/mcuboot.h>
#include <zephyr/drivers/flash.h>

#include "slot1_trailer.h"

off_t slot1_trailer_offset(const struct flash_area *fa) {
#if defined(CONFIG_MCUBOOT_UPDATE_FOOTER_SIZE) && (CONFIG_MCUBOOT_UPDATE_FOOTER_SIZE > 0)
	return (off_t)fa->fa_size - CONFIG_MCUBOOT_UPDATE_FOOTER_SIZE;
#else
	return boot_get_trailer_status_offset(fa->fa_size);
#endif
}

int slot1_erase_trailer(const struct flash_area *fa) {
	struct flash_pages_info page;
	off_t off = slot1_trailer_offset(fa);
	int rc;

	rc = flash_get_page_info_by_offs(flash_area_get_device(fa), fa->fa_off + off, &page);
	if (rc) {
		return rc;
	}

	off = page.start_offset - fa->fa_off;
	return flash_area_erase(fa, off, fa->fa_size - off);
}

#endif
