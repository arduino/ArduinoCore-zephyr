/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 * Copyright (c) 2022 Dhruva Gole
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <Arduino.h>
#include <zephyr/drivers/pinctrl.h>

namespace zephyr {
namespace arduino {

#if defined(ARDUINO)
/*
 * The global ARDUINO macro is numeric (e.g. 10607) in Arduino builds.
 * Temporarily hide it so pinctrl token concatenation can use the literal
 * custom state name "ARDUINO" from devicetree pinctrl-names.
 * Otherwise, the generated pinctrl state identifiers would be like PINCTRL_STATE_10607 instead of
 * PINCTRL_STATE_ARDUINO.
 */
#pragma push_macro("ARDUINO")
#undef ARDUINO
#endif

/*
 * Pinctrl configuration structures for dynamic pin switching.
 *
 * Map deferred-init peripherals and zephyr,console (that is not deferred) with their
 * pinctrl configuration from devicetree.
 */
#define NODE_SELECTED(node_id)                                                                     \
	UTIL_OR(DT_PROP(node_id, zephyr_deferred_init),                                                \
			DT_SAME_NODE(node_id, DT_CHOSEN(zephyr_console)))

#define PINCTRL_DEFINE_IF_SELECTED(node_id)                                                        \
	COND_CODE_1(NODE_SELECTED(node_id), (PINCTRL_DT_DEFINE(node_id);), ())

DT_FOREACH_STATUS_OKAY_NODE(PINCTRL_DEFINE_IF_SELECTED)

struct pinctrl_map_entry {
	const struct device *dev;
	const struct pinctrl_dev_config *pcfg;
};

#define PINCTRL_MAP_ENTRY(node_id) {DEVICE_DT_GET(node_id), PINCTRL_DT_DEV_CONFIG_GET(node_id)},
#define PINCTRL_MAP_ENTRY_IF_PRESENT(node_id)                                                      \
	COND_CODE_1(NODE_SELECTED(node_id), (PINCTRL_MAP_ENTRY(node_id)), ())

static const struct pinctrl_map_entry pinctrl_map[] = {
	DT_FOREACH_STATUS_OKAY_NODE(PINCTRL_MAP_ENTRY_IF_PRESENT){NULL, NULL},
};

#if defined(ARDUINO)
#pragma pop_macro("ARDUINO")
#endif

/* Get pinctrl_dev_config for a device from the generated map. */
static const struct pinctrl_dev_config *get_known_pcfg(const struct device *dev) {
	for (size_t i = 0; i < ARRAY_SIZE(pinctrl_map); i++) {
		if (pinctrl_map[i].dev == dev) {
			return pinctrl_map[i].pcfg;
		}
	}

	return nullptr;
}

/**
 * @brief Initialize the peripheral and acquire a single pin to ARDUINO state.
 *
 * Switches peripheral pins back to ARDUINO pinctrl state (alternate function),
 * typically after a temporary transition to GPIO mode.
 *
 * @param dev Pointer to the peripheral device
 * @param state_pin_idx Index of the pin within the device's ARDUINO pinctrl state
 * @return 0 on success, negative on error
 */
int init_dev_apply_channel_pinctrl(const struct device *dev, size_t state_pin_idx) {

	if (dev == nullptr) {
		return -EINVAL;
	}

	if (!device_is_ready(dev)) {
		// init device for first usage, if not ready
		int err = device_init(dev);
		if (err < 0) {
			return err;
		}
	}

	const struct pinctrl_state *state;
	const struct pinctrl_dev_config *pcfg = get_known_pcfg(dev);

	if (pcfg == nullptr) {
		/* Device not in DT mapping - add to pinctrl_map if needed */
		return -ENOTSUP;
	}

	int err = pinctrl_lookup_state(pcfg, PINCTRL_STATE_ARDUINO, &state);
	if (err < 0) {
		return err; /* Fails if the state is not defined in pinctrl-names */
	}

	/* bounds check */
	if (state_pin_idx >= state->pin_cnt) {
		return -ERANGE;
	}

	/*
	 * On platforms without CONFIG_PINCTRL_STORE_REG (e.g. STM32) the pcfg->reg is not present but
	 * the argument is ignored by their pinctrl driver, so passing PINCTRL_REG_NONE is safe.
	 */
#ifdef CONFIG_PINCTRL_STORE_REG
	uintptr_t reg = pcfg->reg;
#else
	uintptr_t reg = PINCTRL_REG_NONE;
#endif

	return pinctrl_configure_pins(&state->pins[state_pin_idx], 1, reg);
}

/**
 * @brief Optimize peripheral transitions applying pinctrl state PINCTRL_STATE_DEFAULT.
 *
 * @param dev Target peripheral device to acquire pin for
 */
int init_dev_apply_pinctrl(const struct device *dev) {

	if (dev == nullptr) {
		return -EINVAL;
	}

	if (!device_is_ready(dev)) {
		int ret = device_init(dev);
		if (ret != 0 && ret != -EALREADY) {
			return ret;
		}
	}

	const struct pinctrl_dev_config *pcfg = get_known_pcfg(dev);
	if (pcfg == nullptr) {
		/* Device not in DT mapping - add to pinctrl_map if needed */
		return -ENOTSUP;
	}

	return pinctrl_apply_state(pcfg, PINCTRL_STATE_DEFAULT);
}

} // namespace arduino
} // namespace zephyr

using namespace zephyr::arduino;

void yield(void) {
	k_yield();
}

unsigned long micros(void) {
#ifdef CONFIG_TIMER_HAS_64BIT_CYCLE_COUNTER
	return k_cyc_to_us_floor32(k_cycle_get_64());
#else
	return k_cyc_to_us_floor32(k_cycle_get_32());
#endif
}

unsigned long millis(void) {
	return k_uptime_get_32();
}

const struct device *digitalPinToPortDevice(pin_size_t pinNumber) {
	RETURN_ON_INVALID_PIN(pinNumber, nullptr);

	return arduino_pins[pinNumber].port;
}

int digitalPinToPinIndex(pin_size_t pinNumber) {
	RETURN_ON_INVALID_PIN(pinNumber, -1);

	return arduino_pins[pinNumber].pin;
}
