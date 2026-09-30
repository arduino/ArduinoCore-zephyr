/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <soc.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/sys/byteorder.h>

#include "pico/bootrom.h"
#include <hardware/flash.h>

ssize_t z_impl_hwinfo_get_device_id(uint8_t *buffer, size_t length)
{
	uint32_t id[2];
	uint32_t key;

	key = irq_lock();
	flash_get_unique_id((uint8_t*)&id[0]);
	irq_unlock(key);

	id[0] = sys_cpu_to_be32(id[0]);
	id[1] = sys_cpu_to_be32(id[1]);

	if (length > sizeof(id)) {
		length = sizeof(id);
	}

	memcpy(buffer, id, length);
	return length;
}
