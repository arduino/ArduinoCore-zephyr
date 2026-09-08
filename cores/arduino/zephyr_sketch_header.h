/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>

struct sketch_header_v1 {
	uint8_t ver;    // @ 0x07
	uint32_t len;   // @ 0x08
	uint16_t magic; // @ 0x0c
	uint8_t flags;  // @ 0x0e
} __attribute__((packed));

#define HEADER_LEN        16
#define SKETCH_HEADER_LEN HEADER_LEN

#define SKETCH_FLAG_DEBUG        0x01
#define SKETCH_FLAG_LINKED       0x02
#define SKETCH_FLAG_IMMEDIATE    0x04
#define SKETCH_FLAG_WAIT_FOR_APP 0x08

#define SKETCH_VERSION_LATEST 0x1
#define SKETCH_MAGIC          0x2341

int sketch_header_v1_verify(const struct sketch_header_v1 *hdr);
