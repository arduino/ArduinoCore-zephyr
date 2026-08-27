/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * In-field bootloader update, from the loader.
 *
 * MCUboot lives in BFM, which no running MCUboot can rewrite safely and which
 * `arduino-cli` cannot reach at all - so without this, changing the bootloader
 * means a probe and tools/chandler_flasher/chandler_flasher.py. That is a poor position for the
 * *only* bootloader on the board to be in.
 *
 * The update rides the channel that already exists rather than adding one:
 *
 *   1. Host puts a staged bootloader in slot1, using the ordinary recovery
 *      upload (`-smp-slot 2`). MCUboot writes it and ignores it - a raw blob
 *      has no MCUboot swap trailer, so no swap is ever pending.
 *   2. Reset. MCUboot boots as usual and chainloads the loader.
 *   3. This runs, finds the staging header, and writes the image into the BFM
 *      slot that is NOT running, then promotes it by sequence number.
 *   4. Reboot lands on the new bootloader.
 *
 * Power-fail safety is the SoC's, not ours: until the sequence number is
 * bumped - one quad-word, the last thing that happens - the old bootloader
 * still wins. A failure anywhere before that leaves a half-written spare slot
 * that nothing boots from. See the variant's bfm/bfm_boot.h.
 *
 * Why the loader and not MCUboot: MCUboot has ~3 KB of its 64 K left, and it
 * would be rewriting the array it is executing from. The loader runs from PFM,
 * a different flash array, so writing BFM is a cross-array access that cannot
 * stall or corrupt its own fetch (DS 31.3.5.2, "Live Update").
 */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/crc.h>
#include <zephyr/sys/reboot.h>

#include <bfm_boot.h>
#include <bfm_flash.h>
#include <recovery_cookie.h>

LOG_MODULE_REGISTER(bfm_update, CONFIG_LOG_DEFAULT_LEVEL);

/*
 * Staging header, written by the host at the start of slot1. Deliberately not
 * an MCUboot image header: MCUboot must keep seeing slot1 as "nothing pending",
 * and this must be distinguishable from a staged *application* image.
 *
 * Little-endian, 16 bytes, followed immediately by the bootloader image:
 *
 *   0x00  magic  "BFMU"  0x554D4642
 *   0x04  len    image length in bytes
 *   0x08  crc    crc32_ieee over those len bytes
 *   0x0C  pad    0
 */
#define BFM_STAGE_MAGIC 0x554D4642U

struct bfm_stage_hdr {
	uint32_t magic;
	uint32_t len;
	uint32_t crc;
	uint32_t pad;
};

/* Multi-image MCUboot: the BFM package is uploaded to slot2_partition
 * (user_sketch) via SMP --slot 3. Read it back from there at boot. */
#define STAGE_AREA_ID FIXED_PARTITION_ID(user_sketch)
#define COPY_CHUNK    BFM_ROW_SIZE

static uint8_t copy_buf[COPY_CHUNK];

/*
 * Read the staged image back out of user_sketch and check it before a single byte of
 * BFM is touched. A corrupt staging area is the one failure this can catch
 * cheaply, and the alternative - discovering it after the old bootloader has
 * been overwritten - has no recovery that does not involve a probe.
 */
static int stage_verify(const struct flash_area *fa, const struct bfm_stage_hdr *hdr)
{
	uint32_t crc = 0;
	size_t off = 0;

	while (off < hdr->len) {
		size_t n = MIN(sizeof(copy_buf), hdr->len - off);
		int rc = flash_area_read(fa, sizeof(*hdr) + off, copy_buf, n);

		if (rc) {
			LOG_ERR("staging read failed at %zu: %d", off, rc);
			return rc;
		}
		crc = crc32_ieee_update(crc, copy_buf, n);
		off += n;
	}

	if (crc != hdr->crc) {
		LOG_ERR("staged bootloader is corrupt (crc %08x, expected %08x)",
			crc, hdr->crc);
		return -EILSEQ;
	}
	return 0;
}

/* Streamed rather than buffered: the image is up to 64 KB and the loader does
 * not have that much RAM to spare. bfm_stream_* erases each page just before
 * its first row is written. */
static int stage_program(const struct flash_area *fa, const struct bfm_stage_hdr *hdr)
{
	size_t off = 0;
	int rc = bfm_stream_begin(BFM_SPARE_SLOT, BFM_SLOT_SIZE);

	if (rc) {
		LOG_ERR("cannot open the spare BFM slot: %d", rc);
		return rc;
	}

	while (off < hdr->len) {
		size_t n = MIN(sizeof(copy_buf), hdr->len - off);

		rc = flash_area_read(fa, sizeof(*hdr) + off, copy_buf, n);
		if (rc) {
			return rc;
		}
		rc = bfm_stream_write(copy_buf, n);
		if (rc) {
			LOG_ERR("BFM write failed at %zu: %d", off, rc);
			return rc;
		}
		off += n;
	}

	rc = bfm_stream_finish();
	return rc;
}

/* Drop the magic so the update runs once. Erasing the first page is the
 * unambiguous way to do it - the header is the only thing in it that matters,
 * and a partially cleared magic would be worse than either state. */
static void stage_invalidate(const struct flash_area *fa)
{
	int rc = flash_area_erase(fa, 0, BFM_PAGE_SIZE);

	if (rc) {
		LOG_ERR("could not invalidate the staging header: %d - the update "
			"would run again on the next boot", rc);
	}
}

static int bfm_update_check(void)
{
	const struct flash_area *fa;
	struct bfm_stage_hdr hdr;
	int rc;


	rc = flash_area_open(STAGE_AREA_ID, &fa);
	if (rc) {
		return 0;
	}

	rc = flash_area_read(fa, 0, &hdr, sizeof(hdr));
	if (rc || hdr.magic != BFM_STAGE_MAGIC) {
		goto out;
	}

	if (hdr.len == 0U || hdr.len > BFM_SLOT_SIZE) {
		LOG_ERR("staged bootloader claims %u bytes; the slot is %u",
			hdr.len, (unsigned)BFM_SLOT_SIZE);
		stage_invalidate(fa);
		goto out;
	}

	LOG_INF("staged bootloader found: %u bytes", hdr.len);

	if (stage_verify(fa, &hdr) != 0) {
		stage_invalidate(fa);
		goto out;
	}

	if (stage_program(fa, &hdr) != 0) {
		/* The spare slot is now junk, but it is the slot that is NOT
		 * running and its sequence number was never raised, so the
		 * board still boots what it booted before. Leave the staging
		 * header in place: the next boot retries. */
		goto out;
	}

	if (!bfm_boot_spare_image_valid()) {
		LOG_ERR("what was written to the spare slot is not bootable; "
			"not promoting it");
		stage_invalidate(fa);
		goto out;
	}

	rc = bfm_boot_promote_spare();
	if (rc) {
		LOG_ERR("promotion failed: %d", rc);
		stage_invalidate(fa);
		goto out;
	}

	stage_invalidate(fa);
	flash_area_close(fa);

	LOG_INF("new bootloader promoted; rebooting into it");
	k_msleep(50);   /* let the log drain */
	sys_reboot(SYS_REBOOT_COLD);
	CODE_UNREACHABLE;

out:
	flash_area_close(fa);
	return 0;
}

/*
 * APPLICATION priority: after the flash driver (POST_KERNEL) and before main()
 * starts the sketch, so a staged bootloader is installed before anything else
 * can touch flash or take over the board.
 */
SYS_INIT(bfm_update_check, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
