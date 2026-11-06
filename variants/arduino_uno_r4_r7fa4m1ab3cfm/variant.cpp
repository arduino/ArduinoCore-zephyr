/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>

#define BSP_PRV_PRCR_KEY                  (0xA500U)
#define BSP_PRV_PRCR_PRC1_UNLOCK          ((BSP_PRV_PRCR_KEY) | 0x2U)
#define BSP_PRV_PRCR_LOCK                 ((BSP_PRV_PRCR_KEY) | 0x0U)

#define BOOT_DOUBLE_TAP_DATA              (*((volatile uint32_t *) &R_SYSTEM->VBTBKR[0]))
#define DOUBLE_TAP_MAGIC                  0x07738135

void _on_1200_bps() {
    R_SYSTEM->PRCR = (uint16_t) BSP_PRV_PRCR_PRC1_UNLOCK;
    BOOT_DOUBLE_TAP_DATA = DOUBLE_TAP_MAGIC;
    R_SYSTEM->PRCR = (uint16_t) BSP_PRV_PRCR_LOCK;
    NVIC_SystemReset();
}
