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

inline __attribute__((always_inline)) void delay(unsigned long ms) {
	k_sleep(K_MSEC(ms));
}

inline __attribute__((always_inline)) void delayMicroseconds(unsigned int us) {
#if CONFIG_ARDUINO_DELAY_US_COMPENSATION < 2
	if (us == 0) {
		return;
	}
#else
	if (us < CONFIG_ARDUINO_DELAY_US_COMPENSATION) {
		return;
	}
#endif
	k_busy_wait(us - CONFIG_ARDUINO_DELAY_US_COMPENSATION);
}

#endif /* __INLINES_H__ */
