/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Provide implementations for inline functions.
 */

#ifndef __INLINES_H__
#define __INLINES_H__

#include <zephyr/kernel.h>

extern inline __attribute__((always_inline, gnu_inline)) void delay(unsigned long ms) {
	k_sleep(K_MSEC(ms));
}

extern inline __attribute__((always_inline, gnu_inline)) void delayMicroseconds(unsigned int us) {
	if (us == 0) {
		return;
	}
	k_busy_wait(us - 1);
}

#endif /* __INLINES_H__ */
