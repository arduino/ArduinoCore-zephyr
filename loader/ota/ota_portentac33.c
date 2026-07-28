/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ota_api.h"

#include <zephyr/fs/fs.h>
#include <zephyr/sys/reboot.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ota_loader, CONFIG_OTA_LOG_LEVEL);

int ota_loader_ready() {
	// C33: SFU checks for UPDATE.BIN.OTA on every boot — no action needed
	struct fs_dirent entry;
	int ret;

	if (fs_stat(OTA_LOADER_FILENAME, &entry) < 0) {
		LOG_WRN("tried to perform an ota without binary present");
		return -1;
	}

	if (sizeof(CONFIG_OTA_LOADER_TEMP_PATH_POSTFIX) > 1) {
		ret = fs_rename(OTA_LOADER_FILENAME, CONFIG_OTA_LOADER_UPDATE_PATH);

		if (ret < 0) {
			LOG_ERR("unable to rename ota file: %d", ret);
		}
	}

	LOG_INF("OTA marked as ready");
	return ret;
}

int ota_loader_start() {
	sys_reboot(SYS_REBOOT_COLD);

	return 0; // unreachable
}
