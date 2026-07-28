/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ota_api.h"
#include <zephyr/fs/fs.h>
#include <zephyr/sys/reboot.h>
#include <stm32h7xx_ll_rtc.h>
#include <stm32_backup_domain.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ota_loader, CONFIG_OTA_LOG_LEVEL);

#define OTA_MAGIC_BOOT   0x07AA
// #define OTA_STORAGE_TYPE 0xA4   // QSPI_FLASH | FATFS | MBR
#define OTA_STORAGE_TYPE ((1 << 2) | (1 << 5) | (1 << 7))
#define OTA_MBR_PART     2

int ota_loader_ready() {
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
			return ret;
		}
	}

	stm32_backup_domain_enable_access();
	LL_RTC_BAK_SetRegister(RTC, RTC_BKP_DR0, OTA_MAGIC_BOOT);
	LL_RTC_BAK_SetRegister(RTC, RTC_BKP_DR1, OTA_STORAGE_TYPE);
	LL_RTC_BAK_SetRegister(RTC, RTC_BKP_DR2, OTA_MBR_PART);
	LL_RTC_BAK_SetRegister(RTC, RTC_BKP_DR3, entry.size);
	stm32_backup_domain_disable_access();

	LOG_INF("OTA marked as ready");

	return 0;
}

int ota_loader_start() {
	LOG_INF("OTA started, reboot in progress");

	sys_reboot(SYS_REBOOT_COLD);
	return 0; // unreachable
}
