/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * What an update replaces. On boards booted by MCUboot the sketch is part of
 * the loader image, so both stage the same full image (<sketch>.ino.signed.elf-zsk.bin).
 */
enum ota_type {
	OTA_SKETCH,
	OTA_LOADER,
};

/*
 * Start staging an update of `size` bytes. Discards any update staged before.
 * Follow with ota_write() for the whole payload and ota_write_end(), then
 * ota_sketch_ready() or ota_loader_ready() matching `type`.
 */
int ota_write_begin(enum ota_type type, size_t size);

/*
 * Append `len` bytes of the update. Returns 0 or a negative errno.
 */
int ota_write(const void *buf, size_t len);

/*
 * Finish staging. Fails if fewer or more bytes than announced to
 * ota_write_begin() were written.
 */
int ota_write_end(void);

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

/*
 * Keep the running image. On boards whose MCUboot swaps updates in as a test,
 * an image that is not confirmed is swapped back on the next reset; the loader
 * confirms it by itself after CONFIG_LOADER_CONFIRM_DELAY_MS. Call this to
 * confirm earlier, e.g. once the sketch has reached its server again. Does
 * nothing where updates are not tested.
 */
int ota_confirm(void);

#ifdef __cplusplus
}
#endif
