/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Pin control for native targets: there are no pins to multiplex, so pin
 * states are accepted and applying them does nothing.
 */

#ifndef LOADER_NATIVE_SIM_PINCTRL_SOC_H
#define LOADER_NATIVE_SIM_PINCTRL_SOC_H

#include <zephyr/devicetree.h>
#include <zephyr/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t pinctrl_soc_pin_t;

#define Z_PINCTRL_STATE_PIN_INIT(node_id, prop, idx) 0U,

#define Z_PINCTRL_STATE_PINS_INIT(node_id, prop)                                                   \
	{DT_FOREACH_PROP_ELEM(node_id, prop, Z_PINCTRL_STATE_PIN_INIT)}

#ifdef __cplusplus
}
#endif

#endif /* LOADER_NATIVE_SIM_PINCTRL_SOC_H */
