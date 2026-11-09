/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Built in the native simulator runner context: uses the host C library. */

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "sketch_native_host.h"

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

int sketch_native_map(const char *path, uintptr_t base, size_t max_size) {
	struct stat st;
	int fd = open(path, O_RDONLY);

	if (fd < 0) {
		fprintf(stderr, "sketch: cannot open '%s': %s\n", path, strerror(errno));
		return -1;
	}

	if (fstat(fd, &st) != 0) {
		fprintf(stderr, "sketch: cannot stat '%s': %s\n", path, strerror(errno));
		close(fd);
		return -1;
	}

	if ((size_t)st.st_size > max_size) {
		fprintf(stderr, "sketch: '%s' is too large (%lld bytes, max %zu)\n", path,
				(long long)st.st_size, max_size);
		close(fd);
		return -2;
	}

	void *mem = mmap((void *)base, max_size, PROT_READ | PROT_WRITE | PROT_EXEC,
					 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);

	if (mem == MAP_FAILED || mem != (void *)base) {
		fprintf(stderr, "sketch: cannot map %zu bytes at %#lx: %s\n", max_size, (unsigned long)base,
				mem == MAP_FAILED ? strerror(errno) : "address taken");
		if (mem != MAP_FAILED) {
			munmap(mem, max_size);
		}
		close(fd);
		return -3;
	}

	size_t done = 0;

	while (done < (size_t)st.st_size) {
		ssize_t n = read(fd, (char *)mem + done, (size_t)st.st_size - done);

		if (n <= 0) {
			fprintf(stderr, "sketch: cannot read '%s': %s\n", path,
					n < 0 ? strerror(errno) : "short file");
			munmap(mem, max_size);
			close(fd);
			return -1;
		}
		done += (size_t)n;
	}

	close(fd);
	return 0;
}
