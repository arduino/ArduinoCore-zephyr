/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../cores/arduino/zephyr_sketch_header.h"

int sketch_header_v1_verify(const struct sketch_header_v1 *hdr) {
	if (hdr->ver != SKETCH_VERSION_LATEST || hdr->magic != SKETCH_MAGIC) {
		return -1;
	}

	return 0;
}
