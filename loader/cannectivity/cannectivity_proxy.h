/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <zephyr/drivers/can.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Acquire the sketch's share of a bridge controller. -ENODEV means that the
 * controller is not managed here; the caller must use the CAN driver directly.
 * The first client chooses the physical timing. Subsequent clients must request
 * the same bitrate(s) and mode. Changing configuration first requires releasing it.
 * This function does not configure pinctrl. The Arduino CAN caller is
 * responsible for applying the selected pin routing after successful acquisition.
 */
int proxy_can_init(const struct device *dev, can_mode_t mode, uint32_t bitrate,
				   uint32_t bitrate_data, can_rx_callback_t callback, void *user_data);

/* Unsubscribe and release only this sketch's share. An in-flight RX callback
 * finishes before this returns. The USB client's share remains active.
 */
int proxy_can_end(const struct device *dev, void *user_data);

#ifdef __cplusplus
}
#endif
