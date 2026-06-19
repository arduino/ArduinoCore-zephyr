/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 * Copyright (c) 2026 TOKITA Hiroshi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * dt_connector.h — helper macros for the connector-based DT pin definitions.
 *
 * Included by Arduino.h as a fallback when zephyr,user does not provide a
 * digital-pin-gpios list.  Provides the same named interface (ZARD_LED_VALUE,
 * ZARD_LED_IN_LIST, ZARD_PWM_DT_SPEC, ZARD_ADC_DT_SPEC, ZARD_PWM_PINS,
 * ZARD_ADC_PINS) as dt_user_pins.h so that Arduino.h and wiring_private.h
 * can be option-agnostic.
 */

#pragma once

#if DT_NODE_EXISTS(DT_NODELABEL(arduino_header))
#if DT_NODE_HAS_COMPAT(DT_NODELABEL(arduino_header), arduino_header_r3)
#include "connectors/arduino_header_r3.h"
#else
#warning "Only the arduino-header-r3 connector is supported; ignoring arduino_header"
#endif
#endif

/* -- GPIO controller enumeration ---------------------------------- */

#define ZARD_CHECK_GPIO_CTLR(node_id)                                                              \
	COND_CODE_1(DT_NODE_HAS_PROP(node_id, gpio_controller), (node_id,), ())

#define ZARD_ALL_GPIO_CTLR DT_FOREACH_NODE(ZARD_CHECK_GPIO_CTLR)

#define ZARD_GET_ARG_N_(N, ...) GET_ARG_N(N, __VA_ARGS__)
#define ZARD_GET_ARG_N(N, ...)  ZARD_GET_ARG_N_(N, __VA_ARGS__)

#define ZARD_IDX_IF_MATCH(i, n)                                                                    \
	COND_CODE_1(DT_SAME_NODE(n, ZARD_GET_ARG_N(UTIL_INC(i), ZARD_ALL_GPIO_CTLR)), (i), ())

#define ZARD_MATCH_IDX(n) LISTIFY(NUM_VA_ARGS_LESS_1(ZARD_ALL_GPIO_CTLR), ZARD_IDX_IF_MATCH, (), n)

#define ZARD_GET_NGPIOS(i, ...) DT_PROP(ZARD_GET_ARG_N(UTIL_INC(i), __VA_ARGS__), ngpios)
#define ZARD_SUM_NGPIOS(...)    LISTIFY(NUM_VA_ARGS(__VA_ARGS__), ZARD_GET_NGPIOS, (+), __VA_ARGS__)

#define ZARD_GLOBAL_GPIO_OFFSET_(ph)                                                               \
	ZARD_SUM_NGPIOS(GET_ARGS_FIRST_N(ZARD_MATCH_IDX(ph), ZARD_ALL_GPIO_CTLR))

#define ZARD_GLOBAL_GPIO_OFFSET(ph)                                                                \
	COND_CODE_1(IS_EQ(NUM_VA_ARGS(ZARD_GLOBAL_GPIO_OFFSET_(ph)), 0),                           \
					     (0), (ZARD_GLOBAL_GPIO_OFFSET_(ph)))

/* -- Per-pin GPIO flags from connector gpio-map -------------------- */

#define ZARD_GPIO_FLAGS_IF_MATCH(n, p, i, ctrl, pin)                                               \
	((DT_SAME_NODE(DT_MAP_ENTRY_PARENT_BY_IDX(n, p, i), ctrl) &&                                   \
	  (DT_MAP_ENTRY_PARENT_SPECIFIER_BY_IDX(n, p, i, 0) == pin)) ?                                 \
		 DT_MAP_ENTRY_PARENT_SPECIFIER_BY_IDX(n, p, i, 1) :                                        \
		 0)

#ifdef ZARD_CONNECTOR
#define ZARD_GPIO_FLAGS_FOR_PIN(pin, ctrl)                                                         \
	DT_FOREACH_MAP_ENTRY_SEP_VARGS(DT_NODELABEL(ZARD_CONNECTOR), gpio_map,                         \
								   ZARD_GPIO_FLAGS_IF_MATCH, (|), ctrl, pin)
#else
/* No connector with gpio-map: there is no pull configuration to derive, default to 0. */
#define ZARD_GPIO_FLAGS_FOR_PIN(pin, ctrl) 0
#endif

/*
 * Emit one gpio_flags_t value per pin of a gpio-controller node.
 * Used by wiring_private.h to populate the gpio_flags[] table.
 */
#define ZARD_GET_GPIO_FLAGS(node_id)                                                               \
	COND_CODE_1(DT_NODE_HAS_PROP(node_id, gpio_controller),                                    \
		    (LISTIFY(DT_PROP_OR(node_id, ngpios, 0),                                       \
			     ZARD_GPIO_FLAGS_FOR_PIN, (,), node_id),), ())

/* -- Global GPIO count and device/ngpios tables -------------------- */

#define ZARD_COUNT_GPIO_PINS(node_id)                                                              \
	COND_CODE_1(DT_NODE_HAS_PROP(node_id, gpio_controller),                                    \
		    (+DT_PROP_OR(node_id, ngpios, 0)), ())

#define ZARD_GLOBAL_GPIO_COUNT (0 DT_FOREACH_NODE(ZARD_COUNT_GPIO_PINS))

/*
 * Collect the device pointer for each gpio-controller node in DTS order.
 * Nodes that are disabled produce a nullptr entry.
 */
#define ZARD_GET_GPIO_DEVICES(node_id)                                                             \
	COND_CODE_1(DT_NODE_HAS_PROP(node_id, gpio_controller),                                    \
		    (COND_CODE_1(DT_NODE_HAS_STATUS_OKAY(node_id),                                 \
				 (DEVICE_DT_GET(node_id),),                                        \
				 (nullptr,))),                                                     \
		    ())

/* Emit the ngpios value for each gpio-controller node in DTS order. */
#define ZARD_GET_GPIO_NGPIOS(node_id)                                                              \
	COND_CODE_1(DT_NODE_HAS_PROP(node_id, gpio_controller),                                    \
		    (DT_PROP_OR(node_id, ngpios, 0),), ())

/* -- LED helpers ---------------------------------------------------- */

/*
 * Connector/global-pin mode: the LED pin number is its global GPIO index.
 */
#define ZARD_LED_VALUE(node_ph, pin_num)   (ZARD_GLOBAL_GPIO_OFFSET(node_ph) + (pin_num))
#define ZARD_LED_IN_LIST(node_ph, pin_num) 1

/* -- Enum helpers -------------------------------------------------- */

#define ZARD_AN_ENUMS(n, p, i)                                                                     \
	A##i = ZARD_GLOBAL_GPIO_OFFSET(DT_PHANDLE_BY_IDX(n, p, i)) + DT_PHA_BY_IDX(n, p, i, pin),

/* -- Connector-based pin enumeration ------------------------------- */

#define ZARD_CONNECTOR_PIN_IS_DIGITAL(node, num)                                                   \
	UTIL_CAT(UTIL_CAT(ZARD_,                                                                       \
					  UTIL_CAT(DT_STRING_UPPER_TOKEN_BY_IDX(node, compatible, 0), _IS_DIGITAL_)),  \
			 num)

#define ZARD_CONNECTOR_PIN_IS_ANALOG(node, num)                                                    \
	UTIL_CAT(                                                                                      \
		UTIL_CAT(ZARD_, UTIL_CAT(DT_STRING_UPPER_TOKEN_BY_IDX(node, compatible, 0), _IS_ANALOG_)), \
		num)

#define ZARD_CONNECTOR_PIN_NAME_D(node, num)                                                       \
	UTIL_CAT(UTIL_CAT(ZARD_,                                                                       \
					  UTIL_CAT(DT_STRING_UPPER_TOKEN_BY_IDX(node, compatible, 0), _PIN_NAME_D_)),  \
			 num)

#define ZARD_CONNECTOR_PIN_NAME_A(node, num)                                                       \
	UTIL_CAT(UTIL_CAT(ZARD_,                                                                       \
					  UTIL_CAT(DT_STRING_UPPER_TOKEN_BY_IDX(node, compatible, 0), _PIN_NAME_A_)),  \
			 num)

#define ZARD_CONN_DN_ENUMS(n, p, i)                                                                \
	COND_CODE_1(ZARD_CONNECTOR_PIN_IS_DIGITAL(DT_NODELABEL(ZARD_CONNECTOR),                    \
		     DT_MAP_ENTRY_CHILD_SPECIFIER_BY_IDX(n, p, i, 0)),                             \
		    (ZARD_CONNECTOR_PIN_NAME_D(DT_NODELABEL(ZARD_CONNECTOR),                       \
		     DT_MAP_ENTRY_CHILD_SPECIFIER_BY_IDX(n, p, i, 0)) =                            \
	             ZARD_GLOBAL_GPIO_OFFSET(DT_MAP_ENTRY_PARENT_BY_IDX(n, p, i)) +                \
		     DT_MAP_ENTRY_PARENT_SPECIFIER_BY_IDX(n, p, i, 0),), ())

#define ZARD_CONN_AN_ENUMS(n, p, i)                                                                \
	COND_CODE_1(ZARD_CONNECTOR_PIN_IS_ANALOG(DT_NODELABEL(ZARD_CONNECTOR),                     \
		     DT_MAP_ENTRY_CHILD_SPECIFIER_BY_IDX(n, p, i, 0)),                             \
		    (ZARD_CONNECTOR_PIN_NAME_A(DT_NODELABEL(ZARD_CONNECTOR),                       \
		     DT_MAP_ENTRY_CHILD_SPECIFIER_BY_IDX(n, p, i, 0)) =                            \
	             ZARD_GLOBAL_GPIO_OFFSET(DT_MAP_ENTRY_PARENT_BY_IDX(n, p, i)) +                \
		     DT_MAP_ENTRY_PARENT_SPECIFIER_BY_IDX(n, p, i, 0),), ())
