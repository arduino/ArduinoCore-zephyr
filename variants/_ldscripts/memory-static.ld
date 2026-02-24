/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

MEMORY
{
    /* 16 below compensates for the zsk header that is added before the .bin */
    FLASH (rx) : ORIGIN = _sketch_start + 16, LENGTH = _sketch_max_size - 16
    RAM (rwx) : ORIGIN = kheap__sketch_heap, LENGTH = kheap__sketch_heap_size
}

PROVIDE(__heap_start = kheap__system_heap);
PROVIDE(__heap_end = kheap__system_heap + kheap__system_heap_size);
