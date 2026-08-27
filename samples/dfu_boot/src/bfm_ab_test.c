/*
 * Copyright (c) 2026 Arduino SA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Proof of the Dual-Boot swap: promote the spare BFM slot at boot.
 *
 * Deliberately observable without a console. The verdict is the hardware state
 * itself, readable over SWD after a reset:
 *
 *   FCW.SWAP    0x44004048  -- BFSWAP flips
 *   FSEQ0 p1    0x0A000000  -- sequence numbers
 *   FSEQ0 p2    0x0A008000
 *
 * A console would only tell you what the firmware believed. BFSWAP is what the
 * Boot ROM actually did with it.
 *
 * Setup: this build goes in the ACTIVE slot, a plain build in the spare. See
 * CONFIG_APP_BFM_AB_TEST for why that ordering matters.
 */

#include <zephyr/kernel.h>

#ifdef CONFIG_APP_BFM_AB_TEST

#include <errno.h>

#include "bfm_boot.h"
#include "bfm_flash.h"

#include "bfm_ab_test.h"

#ifdef CONFIG_PRINTK
#include <zephyr/sys/printk.h>
#define AB_PRINT(...) printk(__VA_ARGS__)
#else
#define AB_PRINT(...)
#endif

void bfm_ab_test(void)
{
	struct bfm_boot_state state;
	int ret;

	bfm_boot_get_state(&state);

	AB_PRINT("\nbfm_ab_test: BFSWAP=%u BFSLOCK=%u\n", state.bfswap, state.swap_locked);
	AB_PRINT("bfm_ab_test: active ucfg 0x%08x seq=%u valid=%u\n", state.active_ucfg,
		 state.active_seq, state.active_seq_valid);
	AB_PRINT("bfm_ab_test: spare  ucfg 0x%08x seq=%u valid=%u\n", state.spare_ucfg,
		 state.spare_seq, state.spare_seq_valid);

	if (!bfm_boot_spare_image_valid()) {
		/*
		 * Expected whenever the spare slot is blank or holds debris.
		 * Nothing is written, and the board boots as usual.
		 */
		AB_PRINT("bfm_ab_test: spare slot holds no valid image, nothing to do\n\n");
		return;
	}

	AB_PRINT("bfm_ab_test: promoting spare to seq=%u\n", state.active_seq + 1U);

	ret = bfm_boot_promote_spare();
	if (ret != 0) {
		AB_PRINT("bfm_ab_test: promote FAILED ret=%d intflag=0x%08x\n", ret,
			 bfm_last_intflag());
		return;
	}

	AB_PRINT("bfm_ab_test: promoted; the spare boots after the next reset\n\n");
}

#endif /* CONFIG_APP_BFM_AB_TEST */
