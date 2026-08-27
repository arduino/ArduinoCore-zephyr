/*
 * Copyright (c) 2026 Arduino SA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>

/*
 * Whether this file is part of an image is decided by the build that includes
 * it, not by a preprocessor guard: samples/dfu_boot adds it under
 * CONFIG_APP_BFM_FLASH, the loader adds it when the board wants in-field
 * bootloader updates. It used to carry an #ifdef CONFIG_APP_BFM_FLASH, which
 * only made sense while it lived inside that one sample.
 */

#include <errno.h>
#include <string.h>

#include <soc.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/sys_io.h>
#include <zephyr/sys/util.h>

#include "bfm_flash.h"
#include "bfm_boot.h"

#define ERASED_WORD 0xFFFFFFFFU

/* Offset of FSEQ0 within a User CFG page (DS Table 11-3). */
#define UCFG_FSEQ0_OFFSET 0x0000U

/*
 * Highest sequence number we will hand out. The number is 16 bits and the Boot
 * ROM picks the larger, so there is no way to promote past the top without
 * first lowering the running slot -- which would change what boots. At one
 * update per second that is still two decades away; refuse rather than wrap
 * into "the old image wins forever".
 */
#define SEQ_MAX 0xFFFEU

/* Buffer for the whole User CFG page during promotion. .bss, not flash. */
static uint8_t ucfg_page[CFM_PAGE_SIZE] __aligned(8);

static bool seq_decode(uint32_t raw, uint16_t *value)
{
	uint16_t true_half = (uint16_t)(raw & 0xFFFFU);
	uint16_t comp_half = (uint16_t)(raw >> 16);

	/* DS 31.2.17.2: invalid unless true ^ complement == 0xFFFF. */
	if ((uint16_t)(true_half ^ comp_half) != 0xFFFFU) {
		return false;
	}

	*value = true_half;

	return true;
}

static uint32_t seq_encode(uint16_t value)
{
	return ((uint32_t)(uint16_t)~value << 16) | (uint32_t)value;
}

void bfm_boot_get_state(struct bfm_boot_state *state)
{
	uint32_t swap = bfm_swap_read();

	memset(state, 0, sizeof(*state));

	state->bfswap = (swap & FCW_SWAP_BFSWAP_Msk) != 0U;
	state->swap_locked = (swap & FCW_SWAP_BFSLOCK_Msk) != 0U;

	/*
	 * BFSWAP=0 means panel 1 is mapped to Lower Boot, i.e. panel 1 is what
	 * is running. CFM does not swap with it, so the running slot's sequence
	 * number is in that panel's own User CFG page.
	 */
	state->active_ucfg = state->bfswap ? CFM_UCFG2 : CFM_UCFG1;
	state->spare_ucfg = state->bfswap ? CFM_UCFG1 : CFM_UCFG2;

	state->active_seq_valid =
		seq_decode(sys_read32(state->active_ucfg + UCFG_FSEQ0_OFFSET), &state->active_seq);
	state->spare_seq_valid =
		seq_decode(sys_read32(state->spare_ucfg + UCFG_FSEQ0_OFFSET), &state->spare_seq);
}

bool bfm_boot_spare_image_valid(void)
{
	uint32_t msp = sys_read32(BFM_SPARE_SLOT);
	uint32_t reset_vector = sys_read32(BFM_SPARE_SLOT + sizeof(uint32_t));
	uint32_t reset_pc = reset_vector & ~BIT(0);
	uint32_t sram_base = DT_REG_ADDR(DT_CHOSEN(zephyr_sram));
	uint32_t sram_end = sram_base + DT_REG_SIZE(DT_CHOSEN(zephyr_sram));
	uint32_t first_instr;

	if ((msp == 0U) || (msp == ERASED_WORD) || ((msp & 0x7U) != 0U)) {
		return false;
	}
	if ((msp <= sram_base) || (msp > sram_end)) {
		return false;
	}

	if ((reset_vector & BIT(0)) == 0U) {
		return false;
	}

	/*
	 * The image in the spare slot is linked for 0x08000000, because that is
	 * where it will run once promoted -- its vectors point at Lower Boot
	 * addresses, NOT at the spare slot it currently occupies. So the range
	 * check is against the Lower Boot window.
	 */
	if ((reset_pc < BFM_BASE) || (reset_pc >= BFM_BASE + BFM_SLOT_SIZE)) {
		return false;
	}

	/*
	 * ...which also means reading reset_pc directly would inspect the
	 * RUNNING image and cheerfully validate whatever is in the spare slot.
	 * Translate into the spare slot before reading.
	 */
	first_instr = sys_read32(reset_pc - BFM_BASE + BFM_SPARE_SLOT);
	if ((first_instr == 0U) || (first_instr == ERASED_WORD)) {
		return false;
	}

	return true;
}

static bool quad_is_erased(const uint8_t *word)
{
	for (size_t i = 0; i < CFM_QUAD_SIZE; i++) {
		if (word[i] != 0xFFU) {
			return false;
		}
	}

	return true;
}

int bfm_boot_promote_spare(void)
{
	struct bfm_boot_state state;
	uint16_t new_seq;
	int ret;

	bfm_boot_get_state(&state);

	if (state.swap_locked) {
		/* BFSLOCK is set; only a reset can clear it. */
		return -EACCES;
	}
	if (!state.active_seq_valid) {
		/*
		 * Without a valid running sequence number there is nothing to
		 * count up from, and the Boot ROM is already in its fallback
		 * behaviour. Refuse rather than guess.
		 */
		return -ENOTSUP;
	}
	if (!bfm_boot_spare_image_valid()) {
		return -ENOEXEC;
	}

	/*
	 * One above the HIGHER of the two, not simply above the running slot.
	 *
	 * Promoting twice without an intervening reset would otherwise compute
	 * the number the spare already holds, and equal sequence numbers are not
	 * a no-op: DS 31.2.17.2 resolves the tie by selecting Panel 1. Whenever
	 * panel 1 is the slot being superseded, that silently reverts the update
	 * -- the DFU transfer succeeds, promotion reports success, and the old
	 * image boots.
	 */
	new_seq = state.active_seq;
	if (state.spare_seq_valid && state.spare_seq > new_seq) {
		new_seq = state.spare_seq;
	}

	if (new_seq >= SEQ_MAX) {
		return -EOVERFLOW;
	}

	new_seq++;

	/*
	 * Snapshot the whole page: it holds live configuration -- WDT setup and
	 * the ECC mode among it -- in ECC words separate from the sequence
	 * number, and the erase below takes all of them with it.
	 */
	memcpy(ucfg_page, (const void *)(uintptr_t)state.spare_ucfg, CFM_PAGE_SIZE);
	sys_put_le32(seq_encode(new_seq), &ucfg_page[UCFG_FSEQ0_OFFSET]);

	ret = bfm_cfg_unlock();
	if (ret != 0) {
		return ret;
	}

	ret = bfm_cfg_erase_page(state.spare_ucfg);
	if (ret != 0) {
		return ret;
	}

	/*
	 * Put back every word that had content, skipping the ones that were
	 * erased -- programming an all-ones word is not the same as leaving it
	 * erased, because the ECC parity would be computed and stored for it.
	 *
	 * FSEQ0's word is deliberately last. It is the commit: until it lands,
	 * the spare has no valid sequence number and the Boot ROM keeps booting
	 * the running slot. Writing it first would mean a power cut could
	 * promote a slot whose configuration words never made it back.
	 */
	for (uint32_t off = CFM_QUAD_SIZE; off < CFM_PAGE_SIZE; off += CFM_QUAD_SIZE) {
		if (quad_is_erased(&ucfg_page[off])) {
			continue;
		}

		ret = bfm_cfg_quad_write(state.spare_ucfg + off,
					 (const uint32_t *)&ucfg_page[off]);
		if (ret != 0) {
			return ret;
		}
	}

	ret = bfm_cfg_quad_write(state.spare_ucfg + UCFG_FSEQ0_OFFSET,
				 (const uint32_t *)&ucfg_page[UCFG_FSEQ0_OFFSET]);
	if (ret != 0) {
		return ret;
	}

	if (memcmp((const void *)(uintptr_t)state.spare_ucfg, ucfg_page, CFM_PAGE_SIZE) != 0) {
		return -EIO;
	}

	return 0;
}

