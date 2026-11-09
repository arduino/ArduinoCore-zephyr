/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SKETCH_NATIVE_HOST_H
#define SKETCH_NATIVE_HOST_H

#include <stddef.h>
#include <stdint.h>

/*
 * Host side: map an executable region of max_size bytes at the fixed
 * address base and copy the file at path into it.
 *
 * Returns 0 on success, -1 if the file cannot be read, -2 if it is larger
 * than max_size, -3 if the region cannot be mapped at base. Errors are
 * printed on stderr.
 */
int sketch_native_map(const char *path, uintptr_t base, size_t max_size);

#endif /* SKETCH_NATIVE_HOST_H */
