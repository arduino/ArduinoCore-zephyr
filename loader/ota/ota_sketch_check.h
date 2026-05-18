/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/storage/flash_map.h>

#ifdef __cplusplus
extern "C" {
#endif

// rename this into sketch ota check or something
// not part of OTA api, internal use
// TODO remove parameter, the flash area should be provided by config
int try_ota_update(const struct flash_area *fa);

#ifdef __cplusplus
}
#endif
