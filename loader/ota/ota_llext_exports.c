/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ota.h>
#include <zephyr/llext/symbol.h>

EXPORT_SYMBOL(ota_write_begin);
EXPORT_SYMBOL(ota_write);
EXPORT_SYMBOL(ota_write_end);

EXPORT_SYMBOL(ota_sketch_ready);
EXPORT_SYMBOL(ota_sketch_start);

EXPORT_SYMBOL(ota_loader_ready);
EXPORT_SYMBOL(ota_loader_start);

EXPORT_SYMBOL(ota_confirm);
