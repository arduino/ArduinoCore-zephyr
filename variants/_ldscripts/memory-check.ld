/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

MEMORY
{
    FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 0x10000000
    RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 0x10000000
}

PROVIDE(__heap_start = 0x40000000);
PROVIDE(__heap_end = 0x50000000);
PROVIDE(kheap__system_heap = 0x40000000);
PROVIDE(kheap__system_heap_size = 0x10000000);

PROVIDE(kheap__sketch_heap = 0x60000000);
PROVIDE(kheap__sketch_heap_size = 0x10000000);
