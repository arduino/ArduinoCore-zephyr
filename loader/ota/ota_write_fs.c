/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <zephyr/fs/fs.h>

#include <ota.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ota_write, CONFIG_OTA_LOG_LEVEL);

/* The not-yet-ready names that ota_sketch_ready()/ota_loader_ready() rename. */
#define OTA_SKETCH_FILENAME CONFIG_OTA_SKETCH_UPDATE_PATH CONFIG_OTA_SKETCH_TEMP_PATH_POSTFIX
#define OTA_LOADER_FILENAME CONFIG_OTA_LOADER_UPDATE_PATH CONFIG_OTA_LOADER_TEMP_PATH_POSTFIX

static struct fs_file_t file;
static bool file_open;
static size_t expected;
static size_t written;

int ota_write_begin(enum ota_type type, size_t size) {
	const char *path = type == OTA_LOADER ? OTA_LOADER_FILENAME : OTA_SKETCH_FILENAME;
	int rc;

	if (file_open) {
		fs_close(&file);
		file_open = false;
	}

	fs_unlink(path);
	fs_file_t_init(&file);
	rc = fs_open(&file, path, FS_O_CREATE | FS_O_WRITE);
	if (rc < 0) {
		LOG_ERR("unable to create %s: %d", path, rc);
		return rc;
	}

	file_open = true;
	expected = size;
	written = 0;
	return 0;
}

int ota_write(const void *buf, size_t len) {
	ssize_t n;

	if (!file_open) {
		return -EBADF;
	}

	n = fs_write(&file, buf, len);
	if (n != (ssize_t)len) {
		LOG_ERR("write failed at %u: %d", (unsigned)written, (int)n);
		return n < 0 ? (int)n : -EIO;
	}

	written += len;
	return 0;
}

int ota_confirm(void) {
	/* File updates are installed for good, there is nothing to confirm. */
	return 0;
}

int ota_write_end(void) {
	int rc;

	if (!file_open) {
		return -EBADF;
	}

	rc = fs_close(&file);
	file_open = false;
	if (rc < 0) {
		return rc;
	}

	if (written != expected) {
		LOG_ERR("wrote %u bytes, expected %u", (unsigned)written, (unsigned)expected);
		return -EIO;
	}

	return 0;
}
