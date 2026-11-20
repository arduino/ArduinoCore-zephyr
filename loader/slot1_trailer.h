/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LOADER_SLOT1_TRAILER_H
#define LOADER_SLOT1_TRAILER_H

#include <zephyr/storage/flash_map.h>

/*
 * Offset in fa where MCUboot's trailer starts: the whole update footer
 * (CONFIG_MCUBOOT_UPDATE_FOOTER_SIZE, swap status included) when known.
 * An image must end before it.
 */
off_t slot1_trailer_offset(const struct flash_area *fa);

/*
 * Erase every page of the MCUboot trailer at the end of fa, so that
 * boot_request_upgrade() writes a fresh one. flash_img only clears the page
 * where the trailer starts, while a swap trailer can span more (8 KB here),
 * and a swap leaves its state in slot1.
 */
int slot1_erase_trailer(const struct flash_area *fa);

#endif
