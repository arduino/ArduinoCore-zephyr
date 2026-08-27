/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Undo the PFM panel swap the Boot ROM performs alongside the BFM one.
 *
 * Promoting the spare BFM Dual-Boot slot flips SWAP.BFSWAP *and* SWAP.PFSWAP,
 * so every PFM address moves by half the array: what was at 0x0C000000 is now
 * the other panel. Observed on hardware 2026-08-16, and contrary to DS
 * 31.2.17.4, which describes PFM panel order as "entirely the user's
 * responsibility".
 *
 * Normalising PFSWAP to 0 makes PFM addressing independent of which boot slot
 * won, so the slots keep fixed addresses across a Dual-Boot promotion. Whoever
 * boots first has to do it, which is why this is shared: it lives here rather
 * than in one bootloader because BOTH bootloaders in this tree need it and
 * neither can include the other's sources. MCUboot calls it from a SYS_INIT.
 *
 * The symptom when it is missing is precise and misleading: an image that was
 * just written and verified byte-for-byte over SWD is not found, because the
 * reader is looking at the other panel. For MCUboot that surfaces as
 *
 *     W: Failed reading image headers; Image=0
 *     E: Unable to find bootable image
 *
 * ⚠️ Timing: DS 31.2.17.4 requires that nothing is accessing either PFM panel
 * while PFSWAP changes. Callers must therefore run this before their first PFM
 * access, and must themselves be executing from BFM or SRAM. Both callers link
 * wholly into BFM, which is what makes it safe for them.
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
