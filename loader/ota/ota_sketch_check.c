/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ota, CONFIG_OTA_LOG_LEVEL);

#include "ota_sketch_check.h"
#include "../../cores/arduino/zephyr_sketch_header.h"

#include <zephyr/fs/fs.h>
#include <string.h>
#include <zephyr/drivers/flash.h>

#include <assert.h>

static int verify_sketch_header(const struct flash_area *fa, const struct sketch_header_v1 *hdr,
								struct fs_dirent *entry) {
	int rc = 0;

	LOG_DBG("Header ver: %02x, len: %d, magic: %04x, flags: %02X", hdr->ver, hdr->len, hdr->magic,
			hdr->flags);

	if (sketch_header_v1_verify(hdr) != 0) {
		LOG_ERR("invalid sketch header (ver=0x%x magic=0x%x)", hdr->ver, hdr->magic);
		return -1;
	}

	LOG_INF("sketch length = %u bytes", (unsigned)hdr->len);

	/* Bounds-check before the destructive erase. */
	if (hdr->len > fa->fa_size) {
		LOG_ERR("sketch too large for partition (%u > %u)", (unsigned)hdr->len,
				(unsigned)fa->fa_size);
		return -1;
	}

	if ((off_t)hdr->len > entry->size) {
		LOG_ERR("header len exceeds file size (%u > %ld)", (unsigned)hdr->len, (long)entry->size);
		return -1;
	}

	return rc;
}

static int perform_ota(const struct flash_area *fa, struct fs_file_t *file,
					   const struct sketch_header_v1 *hdr) {
	int rc = 0;

	size_t wbs = flash_area_align(fa) > 0 ? flash_area_align(fa) : 1;

	uint8_t chunk[CONFIG_OTA_TRANSCRIPTION_CHUNK_SIZE];
	__ASSERT(wbs < CONFIG_OTA_TRANSCRIPTION_CHUNK_SIZE,
			 "OTA internal buffer is smaller(%d) than write block size(%d)",
			 CONFIG_OTA_TRANSCRIPTION_CHUNK_SIZE, wbs);

	__ASSERT(CONFIG_OTA_TRANSCRIPTION_CHUNK_SIZE % wbs == 0,
			 "OTA internal buffer(%d) is not multiple of write block "
			 "size(%d)" CONFIG_OTA_TRANSCRIPTION_CHUNK_SIZE,
			 wbs);

	off_t offset = 0;
	size_t remaining = hdr->len;
	ssize_t n;
	size_t wlen;

	LOG_INF("erasing flash partition (%u bytes)...", (unsigned)fa->fa_size);
	rc = flash_area_erase(fa, 0, fa->fa_size);

	if (rc) {
		LOG_ERR("flash erase failed, rc %d — retry on next boot", rc);
		goto exit;
	}

	if ((rc = fs_seek(file, 0, FS_SEEK_SET))) {
		LOG_ERR("failed to seek to start of file, rc %d — retry on next boot", rc);
		goto exit;
	}

	LOG_INF("rewriting sketch binary");

	while (remaining > 0 &&
		   (n = fs_read(file, chunk, remaining < sizeof(chunk) ? remaining : sizeof(chunk))) > 0) {
		wlen = (size_t)n;
		/* Pad the final partial chunk to the write-block-size with 0xFF
		 * (erased value); flash rejects unaligned writes. */
		if (wlen % wbs != 0) {
			memset(chunk + wlen, 0xff, ROUND_UP(wlen, wbs) - wlen);
			wlen = ROUND_UP(wlen, wbs);
		}

		if ((rc = flash_area_write(fa, offset, chunk, wlen))) {
			LOG_ERR("flash write failed at offset %ld, rc %d — retry on next boot", (long)offset,
					rc);

			goto exit;
		}
		offset += n;
		remaining -= n;
	}

	if (remaining > 0) {
		LOG_ERR("short read, %u bytes missing — retry on next boot", (unsigned)remaining);
		goto exit;
	}

	LOG_INF("wrote %ld bytes to flash", (long)offset);
exit:
	return rc;
}

/*
 * Install /ota:/UPDATE.BIN into the sketch partition if present. The sketch
 * validates the OTA payload (magic, CRC32, decompression) before writing it,
 * so the loader trusts the file and only re-checks the inner sketch_header_v1.
 *
 * On failure before the erase, UPDATE.BIN is removed and the existing sketch
 * boots. On failure after it, the partition is already partial so UPDATE.BIN
 * is kept and the next boot retries — the only way back without DFU.
 */
int try_ota_update(const struct flash_area *fa) {
	int rc = 0;

	struct fs_file_t file = {0};
	struct fs_dirent entry;

	struct sketch_header_v1 *hdr = NULL;
	char header[SKETCH_HEADER_LEN];
	bool delete = false;

	LOG_DBG("checking for ota in \"%s\"", CONFIG_OTA_SKETCH_UPDATE_PATH);

	if (fa == NULL) {
		rc = -1;
		goto exit;
	}

	if (fs_stat(CONFIG_OTA_SKETCH_UPDATE_PATH, &entry) != 0) {
		LOG_DBG("no update pending");
		goto exit;
	}

	LOG_INF("update pending, validating...");

	fs_file_t_init(&file);
	rc = fs_open(&file, CONFIG_OTA_SKETCH_UPDATE_PATH, FS_O_READ);
	if (rc < 0) {
		LOG_ERR("failed to open %s, rc %d", CONFIG_OTA_SKETCH_UPDATE_PATH, rc);
		delete = true;
		goto exit;
	}

	if (entry.size < SKETCH_HEADER_LEN) {
		LOG_ERR("file too small (%ld bytes)", (long)entry.size);
		delete = true;
		rc = -1;
		goto exit;
	}

	rc = fs_read(&file, header, SKETCH_HEADER_LEN);
	if (rc != SKETCH_HEADER_LEN) {
		LOG_ERR("failed to read header");
		delete = true;
		rc = -1;
		goto exit;
	}

	hdr = (struct sketch_header_v1 *)(header + 7);
	if ((rc = verify_sketch_header(fa, hdr, &entry))) {
		delete = true;
		goto exit;
	}

	/* Partition becomes inconsistent past here; keep UPDATE.BIN on failure. */
	LOG_INF("performing ota");
	if ((rc = perform_ota(fa, &file, hdr))) {
		goto exit;
	}
	delete = true;
	LOG_INF("update complete");
exit:

	if (file.mp != NULL && (rc = fs_close(&file))) {
		LOG_ERR("error on fs_close %d", rc);
	}

	if (delete && (rc = fs_unlink(CONFIG_OTA_SKETCH_UPDATE_PATH))) {
		LOG_ERR("error on fs_unlink %d", rc);
	}

	return rc;
}
