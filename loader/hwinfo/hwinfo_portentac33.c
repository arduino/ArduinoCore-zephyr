/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <soc.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/sys/byteorder.h>

ssize_t z_impl_hwinfo_get_device_id(uint8_t *buffer, size_t length)
{
	uint32_t id[4];
	bsp_unique_id_t const *unique_id = R_BSP_UniqueIdGet();
	id[0] = sys_cpu_to_be32(unique_id->unique_id_words[0]);
	id[1] = sys_cpu_to_be32(unique_id->unique_id_words[1]);
	id[2] = sys_cpu_to_be32(unique_id->unique_id_words[2]);
	id[3] = sys_cpu_to_be32(unique_id->unique_id_words[3]);

	if (length > sizeof(id)) {
		length = sizeof(id);
	}

	memcpy(buffer, id, length);
	return length;
}
