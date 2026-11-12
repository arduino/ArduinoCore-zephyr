/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * PIC32CK: turn on the Cortex-M cache controller (CMCC) for instructions
 * only. Built into the loader and, through loader/mcuboot_hooks, into
 * MCUboot: code runs from flash with wait states otherwise. Data is not
 * cached, so flash writes (DFU, OTA, MCUboot copies) cannot leave stale data
 * behind, and no image rewrites code it is running.
 */

#if defined(CONFIG_SOC_FAMILY_MICROCHIP_PIC32CK_SG_GC)

#include <zephyr/init.h>
#include <soc.h>

static int cmcc_icache_init(void) {
	CMCC_REGS->CMCC_CTRL = 0;
	while ((CMCC_REGS->CMCC_SR & CMCC_SR_CSTS_Msk) != 0) {
	}
	CMCC_REGS->CMCC_CFG |= CMCC_CFG_DCDIS_Msk;
	CMCC_REGS->CMCC_CFG &= ~CMCC_CFG_ICDIS_Msk;
	CMCC_REGS->CMCC_MAINT0 = CMCC_MAINT0_INVALL_Msk;
	CMCC_REGS->CMCC_CTRL = CMCC_CTRL_CEN_Msk;

	return 0;
}

SYS_INIT(cmcc_icache_init, PRE_KERNEL_1, 0);

#endif
