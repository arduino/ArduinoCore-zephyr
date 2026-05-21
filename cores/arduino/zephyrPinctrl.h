/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <zephyr/kernel.h>

namespace zephyr {
namespace arduino {

int init_dev_apply_channel_pinctrl(const struct device *dev, size_t state_pin_idx);
int init_dev_apply_pinctrl(const struct device *dev);

/*
 * Resolve pin index in a device ARDUINO pinctrl state from a DT spec array.
 * The resulting index is the per-device ordinal at spec_idx.
 */
template <typename DT_SPEC, size_t N>
inline constexpr size_t state_pin_index_from_spec_index(const DT_SPEC (&specs)[N],
														size_t spec_idx) {
	const struct device *dev = specs[spec_idx].dev;
	size_t state_pin_idx = 0;

	for (size_t i = 0; i < spec_idx; i++) {
		if (specs[i].dev == dev) {
			state_pin_idx++;
		}
	}

	return state_pin_idx;
}

} // namespace arduino
} // namespace zephyr
