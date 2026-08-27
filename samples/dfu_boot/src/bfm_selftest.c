/*
 * Copyright (c) 2026 Arduino SA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Proof that BFM can be programmed by code running on this chip, with no SWD
 * probe. Enable with CONFIG_APP_BFM_SELFTEST and watch sercom6 at 115200.
 *
 * Two pages are programmed, one per write-protect register, because they are
 * genuinely separate hardware: LBWP[15:0] guards Lower Boot and UBWP[15:0]
 * guards Upper Boot, and a writer that clears only the first works perfectly
 * until an image crosses 64 KB. Upper Boot goes first -- if the very first
 * on-chip NVMOP ever attempted were to decode to the wrong address, that is the
 * half that does not contain the reset vector.
 *
 * The pattern is address-derived, so a page written to the right size but the
 * wrong offset fails the compare instead of passing it.
 *
 * The device's own verdict is not the acceptance gate. Read the pages back from
 * the host afterwards:
 *
 *   pyocd commander -t pic32ck1025sg01144 -O pack.debug_sequences.enable=false \
 *     -O connect_mode=under-reset -c "read32 0x0801F000 0x8" -c "read32 0x0800F000 0x8"
 *
 * A silently dropped write is the failure mode this whole area is built around,
 * and firmware that never noticed cannot report it.
 */

#include <zephyr/kernel.h>

#ifdef CONFIG_APP_BFM_SELFTEST

#include <errno.h>

#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include "bfm_flash.h"
#include "bfm_selftest.h"

#define SELFTEST_MAGIC 0xB1F0AA55U

/*
 * Upper Boot page 15 (0x0801F000). Blank as shipped.
 *
 * Lower Boot page 15 (0x0800F000) is past the end of this image -- dfu_boot is
 * ~56 KB and ends around 0x0800DAB8 -- but it is NOT blank: it holds debris
 * from an older, larger image that tools/chandler_flasher/chandler_flasher.py never erased,
 * because that script only erases the pages the new image covers. Erasing it
 * here is both safe and a cleanup. If dfu_boot ever grows past 60 KB, this
 * target has to move.
 */
#define SELFTEST_UPPER_PAGE (BFM_BASE + BFM_SIZE - BFM_PAGE_SIZE)
#define SELFTEST_LOWER_PAGE (BFM_BASE + BFM_LOWER_SIZE - BFM_PAGE_SIZE)

/* 4 KB, too big for a thread stack. */
static uint32_t pattern[BFM_PAGE_SIZE / sizeof(uint32_t)];

static void build_pattern(uint32_t page)
{
	for (size_t i = 0; i < ARRAY_SIZE(pattern); i++) {
		pattern[i] = SELFTEST_MAGIC ^ (page + (uint32_t)(i * sizeof(uint32_t)));
	}
}

static int run_one(const char *what, uint32_t page)
{
	int ret;

	build_pattern(page);

	printk("bfm_selftest: %s page 0x%08x: programming %u bytes\n", what, page,
	       (unsigned int)sizeof(pattern));

	ret = bfm_program(page, pattern, sizeof(pattern));
	if (ret != 0) {
		printk("bfm_selftest: %s page 0x%08x: FAILED ret=%d intflag=0x%08x\n", what, page,
		       ret, bfm_last_intflag());
		return ret;
	}

	/*
	 * bfm_program() already compared, but it compared against the same
	 * buffer it programmed from. Re-read through a volatile pointer so the
	 * compiler cannot fold this into the earlier compare.
	 */
	for (size_t i = 0; i < ARRAY_SIZE(pattern); i++) {
		volatile const uint32_t *p = (volatile const uint32_t *)(uintptr_t)page;

		if (p[i] != pattern[i]) {
			printk("bfm_selftest: %s page 0x%08x: MISMATCH at +0x%x: "
			       "read 0x%08x want 0x%08x\n",
			       what, page, (unsigned int)(i * sizeof(uint32_t)), p[i], pattern[i]);
			return -EIO;
		}
	}

	printk("bfm_selftest: %s page 0x%08x: OK (intflag=0x%08x)\n", what, page,
	       bfm_last_intflag());

	return 0;
}

void bfm_selftest(void)
{
	int upper;
	int lower;

	printk("\nbfm_selftest: on-chip BFM writer, no probe involved\n");

	upper = run_one("UBWP/upper", SELFTEST_UPPER_PAGE);
	lower = run_one("LBWP/lower", SELFTEST_LOWER_PAGE);

	printk("bfm_selftest: result upper=%s lower=%s\n", upper == 0 ? "OK" : "FAIL",
	       lower == 0 ? "OK" : "FAIL");
	printk("bfm_selftest: confirm from the host before believing this\n\n");
}

#endif /* CONFIG_APP_BFM_SELFTEST */
