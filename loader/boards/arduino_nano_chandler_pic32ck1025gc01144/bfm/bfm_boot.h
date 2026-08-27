/*
 * Copyright (c) 2026 Arduino SA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Dual-Boot slot management for BFM.
 *
 * BFM is 128 KB at 0x08000000, split across the two physical flash panels, and
 * the hardware supports two ways of using it (DS 31.2.17):
 *
 *  - Dual Boot (what this file implements, DS 31.2.17.2): each panel's 64 KB is
 *    a separate boot image, and the Boot ROM maps whichever has the higher
 *    sequence number - stored in that panel's CFM User CFG page - to
 *    0x08000000. That is what makes a bootloader update safe against power
 *    loss, and it caps an image at 64 KB.
 *  - Single Boot (DS 31.2.17.3): one image spans both panels, "providing a
 *    larger boot code space". There is no mode bit - "the Boot ROM does not
 *    differentiate between Dual and Single Boot models" and still compares the
 *    sequence numbers - so it is a convention: keep the SeqNums such that the
 *    panel order never moves, and lock it with SWAP.BFSLOCK.
 *
 * The 64 KB cap is therefore a consequence of choosing Dual Boot, not a
 * property of the part. Choosing between them is choosing between a safe
 * in-field bootloader update and twice the bootloader.
 *
 * That gives a bootloader update that is safe against power loss, which nothing
 * else on this part does: write the spare slot, verify it, then raise its
 * sequence number. Until that last step lands the old image still boots, and
 * the datasheet's own recipe (31.2.17.2.1) is exactly this.
 *
 * Two facts drive the whole file:
 *
 * - The FCW honours SWAP.BFSWAP for writes as well as reads, so the spare slot
 *   is ALWAYS at 0x08010000 no matter which panel it physically is.
 * - CFM does NOT swap (31.2.17.1). The User CFG pages stay pinned to physical
 *   panels, so finding the spare's sequence number means reading BFSWAP.
 *
 * A useful consequence of the two slots being in different panels: updating the
 * spare is always a cross-panel access, so Read-While-Write applies and the
 * running image never stalls (31.3.5.2).
 */

#ifndef BFM_BOOT_H_
#define BFM_BOOT_H_

#include <stdbool.h>
#include <stdint.h>

#include "bfm_flash.h"

struct bfm_boot_state {
	bool bfswap;           /* SWAP.BFSWAP: 0 = panel 1 is the active slot */
	bool swap_locked;      /* SWAP.BFSLOCK: promotion is impossible until reset */
	uint32_t active_ucfg;  /* User CFG page of the running slot */
	uint32_t spare_ucfg;   /* User CFG page of the slot at 0x08010000 */
	uint16_t active_seq;   /* sequence number of the running slot */
	uint16_t spare_seq;
	bool active_seq_valid; /* true ^ complement == 0xFFFF */
	bool spare_seq_valid;
};

/* Where a new boot image is written. Always the inactive slot. */
#define BFM_SPARE_SLOT  BFM_UPPER_BASE
#define BFM_SLOT_SIZE   BFM_LOWER_SIZE

/* Read SWAP and both sequence numbers. Pure reads; safe to call any time. */
void bfm_boot_get_state(struct bfm_boot_state *state);

/*
 * Check that the spare slot holds something bootable: MSP in SRAM, a Thumb
 * reset vector inside the slot, and a vector table that is not erased. The same
 * shape of check main.c already applies before chainloading the loader --
 * promoting an image that fails this would hand the Boot ROM a brick.
 */
bool bfm_boot_spare_image_valid(void);

/*
 * Promote the spare slot: give it a sequence number one above the running slot,
 * so the Boot ROM maps it to 0x08000000 at the next reset.
 *
 * Refuses unless bfm_boot_spare_image_valid(). Does NOT reset -- the caller
 * decides when.
 *
 * The page holding the sequence number also holds live configuration, in
 * separate ECC words, so this reads those words out, erases the page, and puts
 * all of them back. Losing power inside that window leaves the spare's sequence
 * number erased and therefore invalid, which makes the Boot ROM fall back to
 * the panel that still has a valid one -- the running image. The failure is
 * safe, but it does cost the spare panel's configuration words until the next
 * attempt rewrites them.
 */
int bfm_boot_promote_spare(void);

#endif /* BFM_BOOT_H_ */
