/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 * Copyright (c) 2026 KurtE
 * Copyright (c) 2026 TOKITA Hiroshi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#define RETURN_ON_INVALID_PIN(pinNumber, ...)                                                      \
	do {                                                                                           \
		if ((pin_size_t)(pinNumber) >= ARRAY_SIZE(arduino_pins)) {                                 \
			return __VA_ARGS__;                                                                    \
		}                                                                                          \
	} while (0)

#ifdef __cplusplus

namespace zephyr {
namespace arduino {

constexpr struct gpio_dt_spec arduino_pins[] = {
	DT_FOREACH_PROP_ELEM_SEP(
	DT_PATH(zephyr_user), digital_pin_gpios, GPIO_DT_SPEC_GET_BY_IDX, (, ))};

/*
 * Calculate GPIO ports/pins number statically from devicetree configuration
 */

template <class N, class Head> constexpr N sum_of_list(const N sum, const Head &head) {
	return sum + head;
}

template <class N, class Head, class... Tail>
constexpr N sum_of_list(const N sum, const Head &head, const Tail &...tail) {
	return sum_of_list(sum + head, tail...);
}

template <class N, class Head> constexpr N max_in_list(const N max, const Head &head) {
	return (max >= head) ? max : head;
}

template <class N, class Head, class... Tail>
constexpr N max_in_list(const N max, const Head &head, const Tail &...tail) {
	return max_in_list((max >= head) ? max : head, tail...);
}

template <class Query, class Head>
constexpr size_t is_first_appearance(const size_t &idx, const size_t &at, const size_t &found,
									 const Query &query, const Head &head) {
	return ((found == ((size_t)-1)) && (query == head) && (idx == at)) ? 1 : 0;
}

template <class Query, class Head, class... Tail>
constexpr size_t is_first_appearance(const size_t &idx, const size_t &at, const size_t &found,
									 const Query &query, const Head &head, const Tail &...tail) {
	return ((found == ((size_t)-1)) && (query == head) && (idx == at)) ?
			   1 :
			   is_first_appearance(idx + 1, at, (query == head ? idx : found), query, tail...);
}

#define ZARD_GET_DEVICE_VARGS(n, p, i, _) DEVICE_DT_GET(DT_GPIO_CTLR_BY_IDX(n, p, i))
#define ZARD_FIRST_APPEARANCE(n, p, i)                                                             \
	is_first_appearance(0, i, ((size_t)-1), DEVICE_DT_GET(DT_GPIO_CTLR_BY_IDX(n, p, i)),           \
						DT_FOREACH_PROP_ELEM_SEP_VARGS(n, p, ZARD_GET_DEVICE_VARGS, (, ), 0))

#if DT_PROP_LEN(DT_PATH(zephyr_user), digital_pin_gpios) > 0

constexpr int port_num = sum_of_list(
	0, DT_FOREACH_PROP_ELEM_SEP(DT_PATH(zephyr_user), digital_pin_gpios,
            ZARD_FIRST_APPEARANCE, (, )));

#define ZARD_GPIO_NGPIOS(n, p, i) DT_PROP(DT_GPIO_CTLR_BY_IDX(n, p, i), ngpios)
constexpr int max_ngpios = max_in_list(
	0, DT_FOREACH_PROP_ELEM_SEP(DT_PATH(zephyr_user), digital_pin_gpios, ZARD_GPIO_NGPIOS, (, )));

#else

constexpr int port_num = 1;
constexpr int max_ngpios = 0;

#endif

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

#endif // __cplusplus
