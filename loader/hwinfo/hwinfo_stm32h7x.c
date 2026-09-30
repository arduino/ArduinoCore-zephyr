/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <soc.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/sys/byteorder.h>

#include <stm32_ll_utils.h>
#include <stm32_ll_rcc.h>
#include <stm32_ll_pwr.h>
#include <stm32_bitops.h>


#define STM32_UID_WORD_2 LL_GetUID_Word2()
#define STM32_UID_WORD_1 LL_GetUID_Word1()
#define STM32_UID_WORD_0 LL_GetUID_Word0()

ssize_t z_impl_hwinfo_get_device_id(uint8_t *buffer, size_t length)
{
	uint32_t id[3];

	id[0] = sys_cpu_to_be32(STM32_UID_WORD_0);
	id[1] = sys_cpu_to_be32(STM32_UID_WORD_1);
	id[2] = sys_cpu_to_be32(STM32_UID_WORD_2);

	if (length > sizeof(id)) {
		length = sizeof(id);
	}

	memcpy(buffer, id, length);

	return length;
}
