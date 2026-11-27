/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Start the emulated RTC already set, as on a board whose RTC kept the time
 * (backup battery), at a fixed date so that the readings of the sketches are
 * known: 2025-10-21 12:00:00.
 */

#include <zephyr/device.h>
#include <zephyr/drivers/rtc.h>
#include <zephyr/init.h>

static int rtc_set_init(void) {
	const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(rtc));
	const struct rtc_time time = {
		.tm_sec = 0,
		.tm_min = 0,
		.tm_hour = 12,
		.tm_mday = 21,
		.tm_mon = 10 - 1,
		.tm_year = 2025 - 1900,
		.tm_wday = 2,
		.tm_yday = 293,
		.tm_isdst = -1,
	};

	if (!device_is_ready(dev)) {
		return -ENODEV;
	}

	return rtc_set_time(dev, &time);
}

SYS_INIT(rtc_set_init, APPLICATION, 99);
