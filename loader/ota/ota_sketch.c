/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/fs/fs.h>
#include <zephyr/sys/reboot.h>

#include "ota_api.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ota_sketch, CONFIG_OTA_LOG_LEVEL);

int ota_sketch_ready() {
	struct fs_dirent entry;
	int ret;

	if (fs_stat(OTA_SKETCH_FILENAME, &entry) < 0) {
		LOG_WRN("tried to perform an ota without binary present");
		return -1;
	}

	if (sizeof(CONFIG_OTA_SKETCH_TEMP_PATH_POSTFIX) > 1) {
		ret = fs_rename(OTA_SKETCH_FILENAME, CONFIG_OTA_SKETCH_UPDATE_PATH);

		if (ret < 0) {
			LOG_ERR("unable to rename ota file: %d", ret);
		}
	}

	LOG_INF("OTA marked as ready");

	return ret;
}

int ota_sketch_start() {
	LOG_INF("OTA started, reboot in progress");

	sys_reboot(SYS_REBOOT_COLD);
	return 0;
}
