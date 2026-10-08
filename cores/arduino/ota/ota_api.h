/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OTA_SKETCH_FILENAME CONFIG_OTA_SKETCH_UPDATE_PATH CONFIG_OTA_SKETCH_TEMP_PATH_POSTFIX
#define OTA_LOADER_FILENAME CONFIG_OTA_LOADER_UPDATE_PATH CONFIG_OTA_LOADER_TEMP_PATH_POSTFIX

/*
 * This function is used to make a sketch ota file ready to be applied on the next reboot.
 * Under the hood this function currently renames the ota file to what the loader is going to be
 * looking for. This avoids unwanted ota starting when the ota file is not ready yet
 */
int ota_sketch_ready();

/*
 * This function is used to trigger an ota procedure for the sketch. You need to mark the sketch as
 * ready with `ota_sketch_ready` in order for it to be applied.
 */
int ota_sketch_start();

/*
 * This function is used to make a loader ota file ready to be applied on the next reboot.
 * Under the hood this function currently renames the ota file to what the bootloader is going to be
 * looking for. This avoids unwanted ota starting when the ota file is not ready yet
 */
int ota_loader_ready();
/*
 * This function is used to trigger an ota procedure for the loader. You need to mark the loader as
 * ready with `ota_loader_ready` in order for it to be applied.
 */
int ota_loader_start();

#ifdef __cplusplus
}
#endif
