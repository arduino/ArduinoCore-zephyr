/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Undo the PFM panel swap the Boot ROM performs alongside the BFM Dual-Boot
 * flip (SWAP.PFSWAP alongside SWAP.BFSWAP — contrary to DS 31.2.17.4).
 * Normalising PFSWAP to 0 keeps PFM slot addresses fixed across promotions.
 *
 * Shared header: both MCUboot and the loader need this, neither can include
 * the other's sources. Must run before the first PFM access, from BFM or SRAM.
 */

#pragma once

#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/sys/sys_io.h>
#include <zephyr/sys/util.h>

#define PFSWAP_FCW_BASE   DT_REG_ADDR_BY_NAME(DT_NODELABEL(nvmctrl), fcw)
#define PFSWAP_FCW_STATUS (PFSWAP_FCW_BASE + 0x18U)
#define PFSWAP_FCW_KEY    (PFSWAP_FCW_BASE + 0x1CU)
#define PFSWAP_FCW_SWAP   (PFSWAP_FCW_BASE + 0x48U)

#define PFSWAP_SWAPKEY      0x91C32C02U
#define PFSWAP_STATUS_BUSY  BIT(0)
#define PFSWAP_SWAP_PFSWAP  BIT(8)
#define PFSWAP_SWAP_PFSLOCK BIT(9)

/* What pfm_normalize_swap() did, so each caller can log it with its own
 * logging macros - the only thing that differed between the two copies. */
enum pfm_swap_result {
	PFM_SWAP_ALREADY_NORMAL = 0,
	PFM_SWAP_NORMALIZED,
	PFM_SWAP_LOCKED,   /* PFSLOCK set: addresses stay shifted until reset */
	PFM_SWAP_FAILED,   /* the bit would not clear */
};

static inline enum pfm_swap_result pfm_normalize_swap(void)
{
	uint32_t swap = sys_read32(PFSWAP_FCW_SWAP);

	if ((swap & PFSWAP_SWAP_PFSWAP) == 0U) {
		return PFM_SWAP_ALREADY_NORMAL;
	}
	if ((swap & PFSWAP_SWAP_PFSLOCK) != 0U) {
		return PFM_SWAP_LOCKED;
	}

	while ((sys_read32(PFSWAP_FCW_STATUS) & PFSWAP_STATUS_BUSY) != 0U) {
	}

	/* Preserve BFSWAP: writing it back unchanged is a no-op, clearing it
	 * would fight the Boot ROM's slot selection - and the running image is
	 * in whichever slot that picked. */
	sys_write32(PFSWAP_SWAPKEY, PFSWAP_FCW_KEY);
	sys_write32(swap & ~PFSWAP_SWAP_PFSWAP, PFSWAP_FCW_SWAP);

	if ((sys_read32(PFSWAP_FCW_SWAP) & PFSWAP_SWAP_PFSWAP) != 0U) {
		return PFM_SWAP_FAILED;
	}
	return PFM_SWAP_NORMALIZED;
}
