/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This function is used to make a sketch ota file ready to be applied on the next reboot.
 * Under the hood this function currently renames the ota file to what the loader is going to be
 * looking for. This avoids unwanted ota starting when the ota file is not ready yet
 */
int ota_sketch_ready(void);

/*
 * This function is used to trigger an ota procedure for the sketch. You need to mark the sketch as
 * ready with `ota_sketch_ready` in order for it to be applied.
 */
int ota_sketch_start(void);

/*
 * This function is used to make a loader ota file ready to be applied on the next reboot.
 * Under the hood this function currently renames the ota file to what the bootloader is going to be
 * looking for. This avoids unwanted ota starting when the ota file is not ready yet
 */
int ota_loader_ready(void);

/*
 * This function is used to trigger an ota procedure for the loader. You need to mark the loader as
 * ready with `ota_loader_ready` in order for it to be applied.
 */
int ota_loader_start(void);

#ifdef __cplusplus
}
#endif
