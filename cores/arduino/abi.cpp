/*
 * Copyright (c) 2024 Arduino SA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>

extern "C" {

void *__dso_handle = (void *)&__dso_handle;

void __cxa_pure_virtual(void) {
}

void __cxa_deleted_virtual(void) {
}

int __cxa_atexit(void (*func)(void *), void *arg, void *dso_handle) {
	(void)func;
	(void)arg;
	(void)dso_handle; // unused
	return 0;
}

int atexit(void (*func)(void)) {
	(void)func;
	return 0;
}

int strcmp(const char *s1, const char *s2) {
	while (*s1 && (*s1 == *s2)) {
		s1++;
		s2++;
	}
	return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

} /* extern "C" */
