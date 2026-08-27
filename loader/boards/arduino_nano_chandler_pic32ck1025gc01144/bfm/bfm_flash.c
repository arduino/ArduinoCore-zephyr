/*
 * Copyright (c) 2026 Arduino SA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * On-chip BFM writer. A port of the register sequence in
 * tools/chandler_flasher/chandler_flasher.py, which is the same sequence but driven from the host
 * over SWD with the CPU halted.
 *
 * Why not the Zephyr flash driver (flash_mchp_nvmctrl_g3.c), which already
 * implements this and even exposes FLASH_EX_OP_BFM_WRITE_PROTECT_DISABLE:
 *
 * - It is a singleton keyed on DT_INST(0, soc_nv_flash), and the board dtsi
 *   deletes flash0's `compatible` (arduino_nano_chandler-common.dtsi) precisely
 *   so it binds to PFM at 0x0C000000 instead. Its offset 0 is PFM byte 0, so no
 *   non-negative offset reaches BFM. (validate_flash_parameters() only bounds
 *   the upper end, so a negative off_t would in fact land in BFM -- that is a
 *   hole to be fixed there, not an API to build on.)
 * - It signals completion from an ISR whose vector lives in the running image.
 *   The operations below run with interrupts locked, so that ISR can never fire
 *   and the semaphore it posts would never be given.
 *
 * The three constraints that shape this file, all from DS60001795G:
 *
 * 1. "Any access to the panel containing the page or panel being erased stalls
 *    reads from all panels until the erase finishes" (31.2.16). ALL panels --
 *    so it is not enough to run from a different panel than the one being
 *    erased. The sequencer functions are therefore __ramfunc and must not call
 *    anything that lives in flash. A stall is not a fault and would eventually
 *    resolve, but a bootloader that freezes for the length of an erase every
 *    time is not something to rely on.
 * 2. Row Write sources its data through the bus matrix from SRCADDR
 *    (31.2.6), which must be SRAM and must stay valid for the whole operation,
 *    or the operation takes FIFOERR/BUSERR. Hence the staging buffer.
 * 3. WRKEY protects a single register and clears on every CTRLA write, so it
 *    has to be rewritten before each NVMOP. CFGKEY protects a group and stays
 *    unlocked until software clears it, so one write covers LBWP and UBWP
 *    together (31.2.12.2).
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
#include <zephyr/devicetree.h>
#include <zephyr/irq.h>
#include <zephyr/sys/util.h>

#include "bfm_flash.h"

#define FCW ((fcw_registers_t *)DT_REG_ADDR_BY_NAME(DT_NODELABEL(nvmctrl), fcw))

/* KEYCODE 0x91C32C in the top 24 bits; the low byte selects what it unlocks. */
#define FCW_WRKEY  0x91C32C01U /* CTRLA, one shot */
#define FCW_CFGKEY 0x91C32C04U /* CTRLB, PWPx, LBWP, UBWP, UOWP, CWP; sticky */

#define NVMOP_QUAD_WRITE 0x2U
#define NVMOP_ROW_WRITE  0x3U
#define NVMOP_PAGE_ERASE 0x4U

#define FCW_INTFLAG_ERR_Msk                                                                        \
	(FCW_INTFLAG_KEYERR_Msk | FCW_INTFLAG_CFGERR_Msk | FCW_INTFLAG_FIFOERR_Msk |               \
	 FCW_INTFLAG_BUSERR_Msk | FCW_INTFLAG_WPERR_Msk | FCW_INTFLAG_OPERR_Msk |                  \
	 FCW_INTFLAG_SECERR_Msk | FCW_INTFLAG_BORERR_Msk | FCW_INTFLAG_WRERR_Msk)

/*
 * DS 31.2.15.1: "It is highly recommended to always have CTRLA.PREPG = 1", and
 * "Mixed use of PREPG yields undefined endurance and retention of the panel".
 * flash_mchp_nvmctrl_g3.c sets it on every PFM operation, and Lower Boot shares
 * a panel with Lower PFM, so leaving it off here would be exactly the mixed use
 * the datasheet warns about. (tools/chandler_flasher/chandler_flasher.py does not set it; see the
 * note in documentation/nano_chandler_bfm.md.)
 */
#define BFM_CTRLA_PREPG FCW_CTRLA_PREPG_Msk

/*
 * Bounded spin instead of a timer: k_busy_wait() lives in flash and calling it
 * from a __ramfunc mid-erase is the one thing this file must not do. Sized well
 * past any page erase or row write at any supported clock.
 */
#define BFM_SPIN_LIMIT 40000000U

/* SRCADDR staging. Mirrors STAGE_ADDR in the pyOCD script, which uses scratch
 * SRAM because the CPU is halted there; here it has to be a real object.
 */
static uint8_t bfm_stage[BFM_ROW_SIZE] __aligned(8);

static uint32_t bfm_intflag;

uint32_t bfm_last_intflag(void)
{
	return bfm_intflag;
}

uint32_t bfm_swap_read(void)
{
	return FCW->FCW_SWAP;
}

/*
 * The sequencer, per DS 31.2.13.2. Everything it touches is a register or a
 * parameter: no calls, no rodata, no logging. Returns the raw INTFLAG, or 0
 * with `*timed_out` set.
 *
 * Note the deliberate absence of any check that `addr` is inside BFM. The
 * caller has that job, and adding it here would mean a comparison against a
 * constant that the compiler is free to place in flash.
 */
static __ramfunc uint32_t fcw_nvmop(uint32_t nvmop, uint32_t addr, uint32_t srcaddr,
				    uint32_t use_src, const uint32_t *data, int *timed_out)
{
	fcw_registers_t *regs = FCW;
	uint32_t spin;
	uint32_t flags;

	*timed_out = 0;

	for (spin = 0; (regs->FCW_STATUS & FCW_STATUS_BUSY_Msk) != 0U; spin++) {
		if (spin >= BFM_SPIN_LIMIT) {
			*timed_out = 1;
			return 0U;
		}
	}

	/* Arbitrate against the Zephyr driver, which takes the same mutex. */
	regs->FCW_MUTEX = FCW_MUTEX_LOCK(1) | FCW_MUTEX_OWNER(1);

	if (use_src != 0U) {
		regs->FCW_SRCADDR = srcaddr;
	}
	if (data != NULL) {
		/* Quad Write sources from DATA[0..7], not SRCADDR. */
		for (uint32_t i = 0; i < 8U; i++) {
			regs->FCW_DATA[i] = data[i];
		}
	}
	regs->FCW_ADDR = addr;

	regs->FCW_INTFLAG = FCW_INTFLAG_Msk; /* write-1-to-clear */

	/* Immediately before CTRLA: writing CTRLA is what clears the key. */
	regs->FCW_KEY = FCW_WRKEY;
	regs->FCW_CTRLA = BFM_CTRLA_PREPG | FCW_CTRLA_NVMOP(nvmop);

	for (spin = 0; (regs->FCW_STATUS & FCW_STATUS_BUSY_Msk) != 0U; spin++) {
		if (spin >= BFM_SPIN_LIMIT) {
			*timed_out = 1;
			return 0U;
		}
	}

	flags = regs->FCW_INTFLAG;

	/*
	 * Clear before returning, so that when interrupts are unlocked the
	 * driver's nvmctrl_isr() does not wake on a DONE that was ours.
	 */
	regs->FCW_INTFLAG = FCW_INTFLAG_Msk;
	regs->FCW_MUTEX = FCW_MUTEX_LOCK(0) | FCW_MUTEX_OWNER(0);

	return flags;
}

static int bfm_nvmop(uint32_t nvmop, uint32_t addr, uint32_t srcaddr, uint32_t use_src,
		     const uint32_t *data)
{
	unsigned int key;
	int timed_out;

	key = irq_lock();
	bfm_intflag = fcw_nvmop(nvmop, addr, srcaddr, use_src, data, &timed_out);
	irq_unlock(key);

	if (timed_out != 0) {
		return -ETIMEDOUT;
	}
	if ((bfm_intflag & FCW_INTFLAG_ERR_Msk) != 0U) {
		return -EIO;
	}
	if ((bfm_intflag & FCW_INTFLAG_DONE_Msk) == 0U) {
		/*
		 * No error and no DONE. Per DS Table 31-3 that is the signature
		 * of a KEY-protected write attempted without the key: the whole
		 * operation was dropped in silence. This is the failure this
		 * module exists to make impossible to miss.
		 */
		return -EACCES;
	}

	return 0;
}

/*
 * `lower` and `upper` select which of the two regions to unprotect. LBWP and
 * UBWP follow the Lower/Upper Boot *regions*, not the physical panels
 * (DS 31.2.14.2), so "upper only" always means the spare slot whichever panel
 * it currently is.
 */
static int bwp_unlock(bool lower, bool upper)
{
	fcw_registers_t *regs = FCW;
	uint32_t lbwp;
	uint32_t ubwp;
	unsigned int key;
	uint32_t spin;

	for (spin = 0; (regs->FCW_STATUS & FCW_STATUS_BUSY_Msk) != 0U; spin++) {
		if (spin >= BFM_SPIN_LIMIT) {
			return -ETIMEDOUT;
		}
	}

	key = irq_lock();
	regs->FCW_INTFLAG = FCW_INTFLAG_Msk;
	/* One CFGKEY covers both registers -- it is sticky, unlike WRKEY. */
	regs->FCW_KEY = FCW_CFGKEY;
	if (lower) {
		regs->FCW_LBWP = 0U;
	}
	if (upper) {
		regs->FCW_UBWP = 0U;
	}
	lbwp = regs->FCW_LBWP;
	ubwp = regs->FCW_UBWP;
	bfm_intflag = regs->FCW_INTFLAG;
	regs->FCW_INTFLAG = FCW_INTFLAG_Msk;
	irq_unlock(key);

	if ((bfm_intflag & FCW_INTFLAG_ERR_Msk) != 0U) {
		return -EIO;
	}

	/*
	 * No DONE check here: this is a plain SFR write, not an NVMOP, so the
	 * sequencer never runs and DONE never sets. The read-back is the test.
	 */
	if (lower && ((lbwp & 0xFFFFU) != 0U)) {
		return -EACCES;
	}
	if (upper && ((ubwp & 0xFFFFU) != 0U)) {
		return -EACCES;
	}

	return 0;
}

int bfm_unlock(void)
{
	return bwp_unlock(true, true);
}

int bfm_unlock_spare(void)
{
	return bwp_unlock(false, true);
}

int bfm_erase_page(uint32_t addr)
{
	if ((addr % BFM_PAGE_SIZE) != 0U || addr < BFM_BASE ||
	    addr > (BFM_BASE + BFM_SIZE - BFM_PAGE_SIZE)) {
		return -EINVAL;
	}

	return bfm_nvmop(NVMOP_PAGE_ERASE, addr, 0U, 0U, NULL);
}

/*
 * CFM User Configuration pages. Same sequencer, different address range and a
 * different write primitive: everything the Boot ROM reads at startup must go
 * in via Quad Write so it carries ECC (DS 31.2.17.2).
 *
 * These pages are NOT scratch. Each holds, in separate 256-bit words, the boot
 * sequence number at +0x00 and a live configuration block at +0x40 and +0x60 --
 * WDT setup and the ECC mode among them. The word granularity is deliberate:
 * "the Sequence Number the User Configuration must exist in separate Flash
 * words ... ECC requires a 256-bit Flash word to be written once only"
 * (DS 11.5.1). So changing the sequence number means erasing the page and
 * putting all three words back, not blanket-rewriting 4 KB.
 */
static int cfg_addr_ok(uint32_t addr, uint32_t align)
{
	if ((addr % align) != 0U) {
		return 0;
	}

	return (addr >= CFM_UCFG1 && addr < CFM_UCFG1 + CFM_PAGE_SIZE) ||
	       (addr >= CFM_UCFG2 && addr < CFM_UCFG2 + CFM_PAGE_SIZE);
}

int bfm_cfg_unlock(void)
{
	fcw_registers_t *regs = FCW;
	unsigned int key;
	uint32_t cwp;

	key = irq_lock();
	regs->FCW_KEY = FCW_CFGKEY;
	regs->FCW_CWP &= ~(FCW_CWP_UC1WP_Msk | FCW_CWP_UC2WP_Msk);
	cwp = regs->FCW_CWP;
	irq_unlock(key);

	/*
	 * Both were already clear on the bench board -- the Boot ROM programs
	 * CWP from the Boot CFG fuses and leaves User CFG writable. This is
	 * here so a board configured differently fails loudly instead of
	 * silently dropping the sequence-number write.
	 */
	if ((cwp & (FCW_CWP_UC1WP_Msk | FCW_CWP_UC2WP_Msk)) != 0U) {
		return -EACCES;
	}

	return 0;
}

int bfm_cfg_erase_page(uint32_t addr)
{
	if (!cfg_addr_ok(addr, CFM_PAGE_SIZE)) {
		return -EINVAL;
	}

	return bfm_nvmop(NVMOP_PAGE_ERASE, addr, 0U, 0U, NULL);
}

int bfm_cfg_quad_write(uint32_t addr, const uint32_t data[8])
{
	/* Copy so the sequencer always reads DATA from SRAM, never from flash. */
	static uint32_t quad[8];

	if (!cfg_addr_ok(addr, CFM_QUAD_SIZE) || data == NULL) {
		return -EINVAL;
	}

	for (uint32_t i = 0; i < 8U; i++) {
		quad[i] = data[i];
	}

	return bfm_nvmop(NVMOP_QUAD_WRITE, addr, 0U, 0U, quad);
}

/*
 * Stage `chunk` bytes into SRAM, padding the rest of the row to the erased
 * value, and program the row. Every row write goes through here so that
 * SRCADDR is always the staging buffer and never the caller's pointer.
 */
static int bfm_stage_and_write(uint32_t addr, const void *src, size_t chunk)
{
	if (chunk < BFM_ROW_SIZE) {
		memset(bfm_stage + chunk, 0xFF, BFM_ROW_SIZE - chunk);
	}
	memcpy(bfm_stage, src, chunk);

	return bfm_nvmop(NVMOP_ROW_WRITE, addr, (uint32_t)(uintptr_t)bfm_stage, 1U, NULL);
}

int bfm_write_row(uint32_t addr, const void *src)
{
	if ((addr % BFM_ROW_SIZE) != 0U || addr < BFM_BASE ||
	    addr > (BFM_BASE + BFM_SIZE - BFM_ROW_SIZE)) {
		return -EINVAL;
	}
	if (src == NULL) {
		return -EINVAL;
	}

	return bfm_stage_and_write(addr, src, BFM_ROW_SIZE);
}

static int bfm_is_blank(uint32_t addr, size_t len)
{
	const uint8_t *p = (const uint8_t *)(uintptr_t)addr;

	for (size_t i = 0; i < len; i++) {
		if (p[i] != 0xFFU) {
			return 0;
		}
	}

	return 1;
}

int bfm_program(uint32_t addr, const void *data, size_t len)
{
	const uint8_t *src = data;
	size_t pages;
	size_t rows;
	int ret;

	if ((addr % BFM_PAGE_SIZE) != 0U || data == NULL || len == 0U) {
		return -EINVAL;
	}
	if (addr < BFM_BASE || (addr - BFM_BASE) + len > BFM_SIZE) {
		return -EINVAL;
	}

	pages = DIV_ROUND_UP(len, BFM_PAGE_SIZE);
	rows = DIV_ROUND_UP(len, BFM_ROW_SIZE);

	ret = bfm_unlock();
	if (ret != 0) {
		return ret;
	}

	for (size_t p = 0; p < pages; p++) {
		uint32_t page_addr = addr + (p * BFM_PAGE_SIZE);

		ret = bfm_erase_page(page_addr);
		if (ret != 0) {
			return ret;
		}

		/*
		 * Read back blank rather than trusting DONE. Reprogramming an
		 * image that is already present verifies OK even when every
		 * write was dropped, so the erase is the only step that can
		 * prove anything actually happened.
		 */
		if (!bfm_is_blank(page_addr, BFM_PAGE_SIZE)) {
			return -EIO;
		}
	}

	for (size_t r = 0; r < rows; r++) {
		uint32_t row_addr = addr + (r * BFM_ROW_SIZE);
		size_t offset = r * BFM_ROW_SIZE;
		size_t chunk = MIN(BFM_ROW_SIZE, len - offset);

		ret = bfm_stage_and_write(row_addr, src + offset, chunk);
		if (ret != 0) {
			return ret;
		}
	}

	if (memcmp((const void *)(uintptr_t)addr, src, len) != 0) {
		return -EIO;
	}

	return 0;
}

/* --- streaming writer -------------------------------------------------- *
 *
 * The DFU download path cannot use bfm_program(): it never has the whole image
 * at once, only 512-byte control transfers. This accumulates them into 1 KB
 * rows -- the smallest thing Row Write can place -- and erases each 4 KB page
 * just before its first row is written.
 *
 * Progressive erase rather than erasing the slot up front, because up front
 * means 16 erases between the host's first DNLOAD and its acknowledgement.
 * Spreading them costs nothing and keeps every control transfer prompt. The
 * pages an undersized image never reaches are erased by bfm_stream_finish(), so
 * a short image cannot leave a longer predecessor's tail behind it.
 */

static struct {
	uint32_t base;
	uint32_t limit;
	uint32_t offset;   /* bytes committed to flash */
	uint32_t buffered; /* bytes held in stream_row */
	bool active;
} stream;

static uint8_t stream_row[BFM_ROW_SIZE] __aligned(8);

static int stream_flush_row(size_t len)
{
	uint32_t row_addr = stream.base + stream.offset;
	int ret;

	/* First row of a page: erase it before anything lands in it. */
	if ((stream.offset % BFM_PAGE_SIZE) == 0U) {
		ret = bfm_erase_page(row_addr);
		if (ret != 0) {
			return ret;
		}
	}

	ret = bfm_stage_and_write(row_addr, stream_row, len);
	if (ret != 0) {
		return ret;
	}

	stream.offset += BFM_ROW_SIZE;
	stream.buffered = 0;

	return 0;
}

int bfm_stream_begin(uint32_t base, uint32_t limit)
{
	int ret;

	if ((base % BFM_PAGE_SIZE) != 0U || (limit % BFM_PAGE_SIZE) != 0U || limit == 0U) {
		return -EINVAL;
	}
	if (base < BFM_BASE || (base - BFM_BASE) + limit > BFM_SIZE) {
		return -EINVAL;
	}

	/*
	 * Only the spare region is unprotected. A defect in this writer then
	 * cannot reach the image currently executing -- LBWP stays armed for
	 * the whole transfer.
	 */
	ret = (base == BFM_UPPER_BASE) ? bfm_unlock_spare() : bfm_unlock();
	if (ret != 0) {
		return ret;
	}

	stream.base = base;
	stream.limit = limit;
	stream.offset = 0;
	stream.buffered = 0;
	stream.active = true;

	return 0;
}

int bfm_stream_write(const void *data, size_t len)
{
	const uint8_t *src = data;

	if (!stream.active) {
		return -EPERM;
	}
	if (data == NULL) {
		return -EINVAL;
	}
	if (stream.offset + stream.buffered + len > stream.limit) {
		return -EFBIG;
	}

	while (len > 0U) {
		size_t chunk = MIN(BFM_ROW_SIZE - stream.buffered, len);
		int ret;

		memcpy(&stream_row[stream.buffered], src, chunk);
		stream.buffered += chunk;
		src += chunk;
		len -= chunk;

		if (stream.buffered < BFM_ROW_SIZE) {
			continue;
		}

		ret = stream_flush_row(BFM_ROW_SIZE);
		if (ret != 0) {
			stream.active = false;
			return ret;
		}
	}

	return 0;
}

int bfm_stream_finish(void)
{
	uint32_t next_page;
	int ret;

	if (!stream.active) {
		return -EPERM;
	}

	if (stream.buffered > 0U) {
		ret = stream_flush_row(stream.buffered);
		if (ret != 0) {
			stream.active = false;
			return ret;
		}
	}

	/*
	 * Erase whatever the image did not cover. Without this a shorter image
	 * would inherit the tail of a longer one, which still passes a
	 * vector-table check because that only looks at the front.
	 */
	next_page = ROUND_UP(stream.offset, BFM_PAGE_SIZE);
	while (next_page < stream.limit) {
		ret = bfm_erase_page(stream.base + next_page);
		if (ret != 0) {
			stream.active = false;
			return ret;
		}
		next_page += BFM_PAGE_SIZE;
	}

	stream.active = false;

	return 0;
}

uint32_t bfm_stream_written(void)
{
	return stream.offset;
}

