/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <Arduino.h>
#include <Client.h>
#include <zephyr/autoconf.h>

#include "api/ota.h"

#ifndef CONFIG_OTA
#error "This board's loader was built without CONFIG_OTA: OTA is not available."
#endif

/*
 * Download http://<host>:<port><path> with a plain HTTP GET into the staging
 * file of an update.  The update is not applied: mark it ready and reboot with
 * ota_sketch_ready() + ota_sketch_start() (or ota_loader_ready() +
 * ota_loader_start()) once the download succeeds.
 *
 * - ota_sketch_download(): sketch-only image, "<sketch>.ino.elf-zsk.bin".
 *   The Zephyr loader installs it on the next boot.
 * - ota_loader_download(): loader + sketch image,
 *   "<sketch>.ino.elf-zsk.bin-bundle.bin".  The Arduino bootloader (the one
 *   shipped with the board, also used by the mbed core) installs it on the
 *   next boot.  STM32H7 boards only.
 *
 * The server must answer 200 with a Content-Length header (any static file
 * server does, e.g. "python3 -m http.server").  A partial download is deleted.
 *
 * Returns the number of bytes downloaded, or a negative errno:
 *   -ECONNREFUSED  could not connect to host:port
 *   -EPROTO        the response is not "200 OK" (wrong path, server error)
 *   -EMSGSIZE      missing or zero Content-Length
 *   -ETIMEDOUT     no data for 10 s, or the connection closed early
 *   other          filesystem error opening or writing the staging file
 */
long ota_sketch_download(Client &client, const char *host, uint16_t port, const char *path);
long ota_loader_download(Client &client, const char *host, uint16_t port, const char *path);
