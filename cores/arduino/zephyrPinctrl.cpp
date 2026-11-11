/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <Arduino.h>
#include <zephyrPinctrl.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pinctrl.h>

// Keep the public IDs independent of the driver header, but verify their mapping here.
static_assert(static_cast<uint8_t>(arduino::PinctrlState::Standard) == PINCTRL_STATE_ARDUINO,
			  "PinctrlState::Standard must match PINCTRL_STATE_ARDUINO");
static_assert(static_cast<uint8_t>(arduino::PinctrlState::Alt1) == PINCTRL_STATE_ARDUINO_ALT,
			  "PinctrlState::Alt1 must match PINCTRL_STATE_ARDUINO_ALT");

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
#define ZARD_RESTORE_ARDUINO_MACRO 1
#pragma push_macro("ARDUINO")
#undef ARDUINO
#endif

/*
 * Pinctrl configuration structures for dynamic pin switching.
 *
 * Map deferred-init peripherals and zephyr,console (that is not deferred) with their
 * pinctrl configuration from devicetree.
 */
#define PINCTRL_NODE_SELECTED(node_id)                                                             \
	UTIL_AND(DT_NODE_HAS_PROP(node_id, pinctrl_0),                                                 \
			 UTIL_OR(DT_PROP(node_id, zephyr_deferred_init),                                       \
					 DT_SAME_NODE(node_id, DT_CHOSEN(zephyr_console))))

#define PINCTRL_DEFINE_IF_SELECTED(node_id)                                                        \
	COND_CODE_1(PINCTRL_NODE_SELECTED(node_id), (PINCTRL_DT_DEFINE(node_id);), ())

DT_FOREACH_STATUS_OKAY_NODE(PINCTRL_DEFINE_IF_SELECTED)

struct pinctrl_map_entry {
	const struct device *dev;
	const struct pinctrl_dev_config *pcfg;
};

#define PINCTRL_MAP_ENTRY(node_id) {DEVICE_DT_GET(node_id), PINCTRL_DT_DEV_CONFIG_GET(node_id)},
#define PINCTRL_MAP_ENTRY_IF_PRESENT(node_id)                                                      \
	COND_CODE_1(PINCTRL_NODE_SELECTED(node_id), (PINCTRL_MAP_ENTRY(node_id)), ())

static const struct pinctrl_map_entry pinctrl_map[] = {
	DT_FOREACH_STATUS_OKAY_NODE(PINCTRL_MAP_ENTRY_IF_PRESENT){NULL, NULL},
};

#if defined(ZARD_RESTORE_ARDUINO_MACRO)
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

static int init_device_with_dependencies(const struct device *dev, uint8_t state_id) {
#if defined(CONFIG_DEVICE_DEPS)
	// Recurse on all dependencies, always bringing them up in their own ARDUINO state.
	size_t handle_count = 0;
	const device_handle_t *handles = device_required_handles_get(dev, &handle_count);
	if (handles != nullptr) {
		for (size_t i = 0; i < handle_count; i++) {
			const struct device *dep_dev = device_from_handle(handles[i]);
			if (dep_dev == nullptr) {
				continue;
			}
			int ret = init_device_with_dependencies(dep_dev, PINCTRL_STATE_ARDUINO);
			if (ret < 0) {
				return ret;
			}
		}
	}
#endif

	// Initialize the device
	if (!device_is_ready(dev)) {
		int ret = device_init(dev);
		if (ret != 0 && ret != -EALREADY) {
			return ret;
		}
	}

	/*
	 * If the device is without pinctrl or pinctrl_apply_state returns -ENOENT because no pins where
	 * defined in the requested state this should not be treated as an error, so just continue.
	 */
	const struct pinctrl_dev_config *pcfg = get_known_pcfg(dev);
	if (pcfg != nullptr) {
		/* An unavailable routing falls back to the normal Arduino pins. */
		const struct pinctrl_state *state;
		if (pinctrl_lookup_state(pcfg, state_id, &state) == -ENOENT) {
			state_id = PINCTRL_STATE_ARDUINO;
		}
		int ret = pinctrl_apply_state(pcfg, state_id);
		if (ret < 0 && ret != -ENOENT) {
			return ret;
		}
	}

	return 0;
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
		/* Device not in DT mapping, add to pinctrl_map if needed. Don't return an error because
		 * the device has been initialized successfully. */
		return 0;
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
 * @brief Initialize a peripheral and apply the requested pinctrl state.
 * Dependencies always use ARDUINO. If the requested state is absent, the target
 * also falls back to ARDUINO. Devices already initialized are remuxed as well.
 *
 * @param dev Target peripheral device
 * @param state Pinctrl state ID; defaults to ARDUINO
 */
int init_dev_apply_pinctrl(const struct device *dev, ::arduino::PinctrlState state) {
	if (dev == nullptr) {
		return -EINVAL;
	}

	return init_device_with_dependencies(dev, static_cast<uint8_t>(state));
}

} // namespace arduino
} // namespace zephyr

/*
 * Declare selected device objects as weak to allow the linker to succeed when a driver is disabled
 * via Kconfig and the device won't be instantiated. The runtime check in get_known_pcfg() will
 * handle NULL pointers gracefully.
 *
 * Note that it must be at global scope to be correctly applied to the same global symbol that LLEXT
 * exports.
 */
#define DECLARE_DEVICE_WEAK(node_id)                                                               \
	COND_CODE_1(PINCTRL_NODE_SELECTED(node_id),                                                             \
		(extern const struct device DEVICE_DT_NAME_GET(node_id) __attribute__((weak));), ())

DT_FOREACH_STATUS_OKAY_NODE(DECLARE_DEVICE_WEAK)
