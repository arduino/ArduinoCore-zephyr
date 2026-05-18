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

int ota_sketch_ready();
int ota_sketch_start();

#ifdef __cplusplus
}
#endif
