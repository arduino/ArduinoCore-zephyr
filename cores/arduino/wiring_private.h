/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 * Copyright (c) 2026 KurtE
 * Copyright (c) 2026 TOKITA Hiroshi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus

namespace zephyr {
namespace arduino {

constexpr pin_size_t invalid_pin_number = pin_size_t(-1);

#if ZARD_DT_HAS_USER_PINS
constexpr struct gpio_dt_spec arduino_pins[] = {
	DT_FOREACH_PROP_ELEM_SEP(
	DT_PATH(zephyr_user), digital_pin_gpios, GPIO_DT_SPEC_GET_BY_IDX, (, ))};
#else
static constexpr const struct device *gpio_ports[] = {DT_FOREACH_NODE(ZARD_GET_GPIO_DEVICES)};
static constexpr uint32_t gpio_ngpios[] = {DT_FOREACH_NODE(ZARD_GET_GPIO_NGPIOS)};
static constexpr gpio_flags_t gpio_flags[] = {DT_FOREACH_NODE(ZARD_GET_GPIO_FLAGS)};

BUILD_ASSERT(
	ARRAY_SIZE(gpio_ports) > 0,
	"No gpio-controller nodes found: cannot derive Arduino pins from connector definition");
#endif // ZARD_DT_HAS_USER_PINS

#ifdef CONFIG_PWM

constexpr struct pwm_dt_spec arduino_pwm[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), pwms, ZARD_PWM_DT_SPEC)};

/* pwm-pins node provides a mapping digital pin numbers to pwm channels */
constexpr pin_size_t arduino_pwm_pins[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), pwm_pin_gpios, ZARD_PWM_PINS)};

#endif

#ifdef CONFIG_ADC

constexpr struct adc_dt_spec arduino_adc[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), io_channels, ZARD_ADC_DT_SPEC)};

/* adc-pin-gpios provides a mapping digital pin numbers to adc channels */
constexpr pin_size_t arduino_analog_pins[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), adc_pin_gpios, ZARD_ADC_PINS)};

#endif

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

#if !ZARD_DT_HAS_USER_PINS
constexpr inline const struct device *local_gpio_port(pin_size_t gpin);

constexpr inline const struct device *local_gpio_port_r(pin_size_t pin,
														const struct device *const *ctrl,
														const uint32_t accum, const uint32_t *end,
														size_t n) {
	return (n == 0) ? nullptr :
		   (pin < accum + end[0]) ?
					  ctrl[0] :
					  local_gpio_port_r(pin, ctrl + 1, accum + end[0], end + 1, n - 1);
}

constexpr inline size_t port_index_r(const struct device *target, const struct device *const *table,
									 pin_size_t idx, size_t n) {
	return (n == 0)             ? size_t(-1) :
		   (target == table[0]) ? idx :
								  port_index_r(target, table + 1, idx + 1, n - 1);
}

constexpr inline pin_size_t port_idx(pin_size_t gpin) {
	return port_index_r(local_gpio_port(gpin), gpio_ports, 0, ARRAY_SIZE(gpio_ports));
}

constexpr inline pin_size_t end_accum_r(const uint32_t accum, const uint32_t *end, size_t n) {
	return (n == 0) ? accum : end_accum_r(accum + end[0], end + 1, n - 1);
}

constexpr inline pin_size_t end_accum(size_t n) {
	return end_accum_r(0, gpio_ngpios, n);
}

constexpr inline pin_size_t global_gpio_pin_(size_t port_idx, pin_size_t lpin) {
	return port_idx == size_t(-1) ? size_t(-1) : end_accum(port_idx) + lpin;
}

constexpr inline pin_size_t global_gpio_pin(const struct device *lport, pin_size_t lpin) {
	return global_gpio_pin_(port_index_r(lport, gpio_ports, 0, ARRAY_SIZE(gpio_ports)), lpin);
}
#endif // !ZARD_DT_HAS_USER_PINS

constexpr inline bool local_gpio_pin_is_valid(pin_size_t pin) {
	return pin != invalid_pin_number;
}

constexpr inline const struct device *local_gpio_port(pin_size_t gpin) {
#if ZARD_DT_HAS_USER_PINS
	return (gpin < ARRAY_SIZE(arduino_pins)) ? arduino_pins[gpin].port : nullptr;
#else
	return local_gpio_port_r(gpin, gpio_ports, 0, gpio_ngpios, ARRAY_SIZE(gpio_ports));
#endif
}

constexpr inline pin_size_t local_gpio_pin(pin_size_t gpin) {
#if ZARD_DT_HAS_USER_PINS
	return (gpin < ARRAY_SIZE(arduino_pins)) ? arduino_pins[gpin].pin : invalid_pin_number;
#else
	return port_idx(gpin) == invalid_pin_number ? invalid_pin_number :
												  gpin - end_accum(port_idx(gpin));
#endif
}

constexpr inline gpio_flags_t local_gpio_flags(pin_size_t gpin) {
#if ZARD_DT_HAS_USER_PINS
	return (gpin < ARRAY_SIZE(arduino_pins)) ? arduino_pins[gpin].dt_flags : 0;
#else
	return (gpin < ARRAY_SIZE(gpio_flags)) ? gpio_flags[gpin] : 0;
#endif
}

inline int global_gpio_pin_configure(pin_size_t pinNumber, int flags) {
#if ZARD_DT_HAS_USER_PINS
	if (pinNumber >= ARRAY_SIZE(arduino_pins)) {
		return -1;
	}
	return gpio_pin_configure_dt(&arduino_pins[pinNumber], flags);
#else
	const struct device *port = local_gpio_port(pinNumber);

	if (port) {
		return gpio_pin_configure(port, local_gpio_pin(pinNumber),
								  flags | local_gpio_flags(pinNumber));
	} else {
		return -1;
	}
#endif
}

#if ZARD_DT_HAS_USER_PINS
#define RETURN_ON_INVALID_PIN(pinNumber, ...)                                                      \
	do {                                                                                           \
		if ((pin_size_t)(pinNumber) >= ARRAY_SIZE(arduino_pins)) {                                 \
			return __VA_ARGS__;                                                                    \
		}                                                                                          \
	} while (0)
#if DT_PROP_LEN_OR(DT_PATH(zephyr_user), digital_pin_gpios, 0) > 0
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
#endif // digital_pin_gpios > 0
#else
#define RETURN_ON_INVALID_PIN(pinNumber, ...)                                                      \
	do {                                                                                           \
		if (!local_gpio_port(pinNumber)) {                                                         \
			return __VA_ARGS__;                                                                    \
		}                                                                                          \
	} while (0)
constexpr int port_num = ARRAY_SIZE(gpio_ports);
constexpr int max_ngpios = max_in_list(0, DT_FOREACH_NODE(ZARD_GET_GPIO_NGPIOS) 0);
#endif // ZARD_DT_HAS_USER_PINS

} // namespace arduino
} // namespace zephyr

#endif // __cplusplus
