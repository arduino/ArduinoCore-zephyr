/*
 * Copyright (c) 2026 Arduino SA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * On-chip writer for the Boot Flash Memory (BFM) of the PIC32CK1025GC01144.
 *
 * BFM at 0x08000000 is the SoC's hardware reset address and is where this
 * image itself lives. Everything here exists so that BFM can be reprogrammed
 * by code running on the chip, with no SWD probe -- see bfm_flash.c for why
 * this bypasses the Zephyr flash driver entirely.
 *
 * Addresses are absolute (0x08......), never offsets: there is no flash device
 * behind BFM to be an offset into.
 */

#ifndef BFM_FLASH_H_
#define BFM_FLASH_H_

#include <stddef.h>
#include <stdint.h>

/*
 * Geometry, DS60001795G 31.2.4.2: BFM is 128 KB spread over two panels with
 * "16 pages in each panel's BFM", 4 KB per page. The two halves are protected
 * by separate registers -- LBWP[15:0] for Lower Boot, UBWP[15:0] for Upper
 * Boot, one bit per page. Row Write, the only sane bulk primitive here, moves
 * 1 KB at a time from SRAM.
 */
#define BFM_BASE       0x08000000U
#define BFM_SIZE       (128U * 1024U)
#define BFM_LOWER_SIZE (64U * 1024U) /* LBWP covers this; UBWP covers the rest */
#define BFM_PAGE_SIZE  4096U
#define BFM_ROW_SIZE   1024U

#define BFM_UPPER_BASE (BFM_BASE + BFM_LOWER_SIZE)

/*
 * CFM User Configuration pages, one per physical panel. Unlike BFM these do
 * NOT move when the boot panels swap (DS 31.2.17.1), so the caller must read
 * SWAP.BFSWAP to know which page belongs to which boot slot -- see bfm_boot.c.
 */
#define CFM_UCFG1     0x0A000000U /* panel 1 */
#define CFM_UCFG2     0x0A008000U /* panel 2 */
#define CFM_PAGE_SIZE 4096U
#define CFM_QUAD_SIZE 32U /* one 256-bit ECC word */

/*
 * Clear every LBWP and UBWP bit, unprotecting all 32 BFM pages.
 *
 * The bits reset to 1 at every reset by design (DS 31.2.14.2, "for safety not
 * security"), so this has to be redone after each reset -- it is not a fuse and
 * it is not sticky. Returns -EACCES if the protection is still armed
 * afterwards, which on this part means a LOCK bit is set and only a reset can
 * clear it.
 */
int bfm_unlock(void);

/*
 * Unprotect only the Upper Boot region -- the spare slot. LBWP and UBWP follow
 * the Lower/Upper Boot regions rather than the physical panels, so this always
 * means "the slot that is not running", and the running image stays
 * write-protected for the duration.
 */
int bfm_unlock_spare(void);

/* Erase one 4 KB page. `addr` must be BFM_PAGE_SIZE-aligned. */
int bfm_erase_page(uint32_t addr);

/*
 * Program one 1 KB row. `addr` must be BFM_ROW_SIZE-aligned, `src` must supply
 * BFM_ROW_SIZE bytes and may live anywhere -- it is copied into an SRAM staging
 * buffer before the operation starts, because the controller fetches row data
 * over the bus matrix and cannot source it from flash.
 */
int bfm_write_row(uint32_t addr, const void *src);

/*
 * Unlock, erase the covered pages, blank-check them, program, and verify.
 * `addr` must be page-aligned; `len` is rounded up to a whole row with 0xFF.
 *
 * This is the whole update in one call. It does NOT reset the chip, and it does
 * not care whether the range overlaps the running image -- see bfm_flash.c on
 * why erasing the flash you are executing from is survivable here, and
 * bfm_selftest.c for the case that deliberately does it.
 */
int bfm_program(uint32_t addr, const void *data, size_t len);

/*
 * INTFLAG as it read after the last operation, for diagnostics. Note that a
 * KEY-protected write attempted without the key leaves INTFLAG at 0 -- no error
 * *and* no DONE -- so 0 here is not by itself good news.
 */
uint32_t bfm_last_intflag(void);

/*
 * Clear the User CFG page write-protect bits in CWP. Returns -EACCES if they
 * do not stay clear, which means a WPLOCK is set and only a reset clears it.
 */
/* Raw FCW.SWAP, for the BFSWAP/BFSLOCK bits. See bfm_boot.c. */
uint32_t bfm_swap_read(void);

int bfm_cfg_unlock(void);

/* Erase one 4 KB CFM User CFG page. `addr` must be CFM_UCFG1 or CFM_UCFG2. */
int bfm_cfg_erase_page(uint32_t addr);

/*
 * Program one 256-bit word of a User CFG page. `addr` must be 32-byte aligned
 * and inside a User CFG page; `data` supplies 8 words, padding unused ones with
 * 0xFFFFFFFF.
 *
 * Quad Write and not Row Write because a row would blanket 1 KB, programming
 * every ECC word it covers -- including ones that should stay erased.
 */
int bfm_cfg_quad_write(uint32_t addr, const uint32_t data[8]);

/*
 * Streaming writer, for callers that receive an image in pieces rather than all
 * at once -- the DFU download path. Accumulates into 1 KB rows and erases each
 * page just before its first row is written.
 *
 * begin() takes the region's BASE ADDRESS and its SIZE -- `limit` bounds how
 * much may be written, it is not an end address. It unprotects and resets
 * state; write() may be called with any sizes;
 * finish() flushes the partial row and erases the rest of the region so a short
 * image cannot leave a longer predecessor's tail behind.
 */
int bfm_stream_begin(uint32_t base, uint32_t limit);
int bfm_stream_write(const void *data, size_t len);
int bfm_stream_finish(void);
uint32_t bfm_stream_written(void);

#endif /* BFM_FLASH_H_ */
