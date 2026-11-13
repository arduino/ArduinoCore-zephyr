/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/autoconf.h>

#if defined(CONFIG_OTA)

#include "ota_download.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <zephyr/fs/fs.h>

#define SKETCH_STAGING_PATH CONFIG_OTA_SKETCH_UPDATE_PATH CONFIG_OTA_SKETCH_TEMP_PATH_POSTFIX
#define LOADER_STAGING_PATH CONFIG_OTA_LOADER_UPDATE_PATH CONFIG_OTA_LOADER_TEMP_PATH_POSTFIX

#define HTTP_TIMEOUT_MS 10000

/* Send the request and parse the response headers; leave the body unread. */
static long http_request(Client &c, const char *host, uint16_t port, const char *path) {
	if (!c.connect(host, port)) {
		return -ECONNREFUSED;
	}

	c.print("GET ");
	c.print(path);
	c.print(" HTTP/1.1\r\nHost: ");
	c.print(host);
	c.print("\r\nConnection: close\r\n\r\n");

	char line[128];
	bool first = true;
	long content_length = -1;
	uint32_t last = millis();

	while (true) {
		if (!c.available()) {
			if (!c.connected() || millis() - last > HTTP_TIMEOUT_MS) {
				return -ETIMEDOUT;
			}
			continue;
		}
		last = millis();

		size_t n = c.readBytesUntil('\n', line, sizeof(line) - 1);
		line[n] = '\0';
		if (n > 0 && line[n - 1] == '\r') {
			line[n - 1] = '\0';
		}

		if (first) {
			if (strncmp(line, "HTTP/1.", 7) != 0 || atoi(line + 9) != 200) {
				return -EPROTO;
			}
			first = false;
			continue;
		}

		if (line[0] == '\0') {
			break; /* end of headers */
		}

		if (strncasecmp(line, "Content-Length:", 15) == 0) {
			content_length = atol(line + 15);
		}
	}

	return content_length > 0 ? content_length : -EMSGSIZE;
}

static long http_body_to_file(Client &c, long content_length, struct fs_file_t *out) {
	uint8_t buf[512];
	long received = 0;
	uint32_t last = millis();

	while (received < content_length) {
		if (c.available() <= 0) {
			if (!c.connected() || millis() - last > HTTP_TIMEOUT_MS) {
				return -ETIMEDOUT;
			}
			continue;
		}

		size_t want = (size_t)(content_length - received);
		if (want > sizeof(buf)) {
			want = sizeof(buf);
		}
		int got = c.read(buf, want);
		if (got <= 0) {
			continue;
		}

		ssize_t wr = fs_write(out, buf, got);
		if (wr != got) {
			return wr < 0 ? wr : -ENOSPC;
		}
		received += got;
		last = millis();
	}

	return received;
}

static long download_to(Client &c, const char *host, uint16_t port, const char *path,
						const char *staging) {
	struct fs_file_t file;
	long ret;

	fs_unlink(staging);
	fs_file_t_init(&file);
	ret = fs_open(&file, staging, FS_O_CREATE | FS_O_WRITE);
	if (ret < 0) {
		return ret;
	}

	ret = http_request(c, host, port, path);
	if (ret > 0) {
		ret = http_body_to_file(c, ret, &file);
	}
	c.stop();

	int rc = fs_close(&file);
	if (ret >= 0 && rc < 0) {
		ret = rc;
	}
	if (ret < 0) {
		fs_unlink(staging);
	}
	return ret;
}

long ota_sketch_download(Client &client, const char *host, uint16_t port, const char *path) {
	return download_to(client, host, port, path, SKETCH_STAGING_PATH);
}

long ota_loader_download(Client &client, const char *host, uint16_t port, const char *path) {
	return download_to(client, host, port, path, LOADER_STAGING_PATH);
}

#endif /* CONFIG_OTA */
