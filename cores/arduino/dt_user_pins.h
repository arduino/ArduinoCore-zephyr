/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 * Copyright (c) 2026 TOKITA Hiroshi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * dt_user_pins.h — helper macros for the zephyr,user DT description path.
 *
 * Collects all helper macros that were previously scattered across
 * Arduino.h and wiring_private.h, and renames them to the ZARD_ namespace.
 * Included unconditionally by Arduino.h.
 */

#pragma once

/* -- Pin lookup helpers --------------------------------------------- */

#if DT_PROP_LEN(DT_PATH(zephyr_user), digital_pin_gpios) > 0

/* Note: DT_REG_ADDR needs an expanded argument or it will not work properly */
#define DIGITAL_PIN_MATCHES(dev_pha, pin, dev, num)                                                \
	(((dev == DT_REG_ADDR(dev_pha)) && (num == pin)) ? 1 : 0)
#define DIGITAL_PIN_EXISTS(n, p, i, dev, num)                                                      \
	DIGITAL_PIN_MATCHES(DT_PHANDLE_BY_IDX(n, p, i), DT_PHA_BY_IDX(n, p, i, pin), dev, num)

/* Check all pins are defined only once */
#define DIGITAL_PIN_CHECK_UNIQUE(i, _)                                                             \
	((DT_FOREACH_PROP_ELEM_SEP_VARGS(                                                              \
		 DT_PATH(zephyr_user), digital_pin_gpios, DIGITAL_PIN_EXISTS, (+),                         \
		 DT_REG_ADDR(DT_PHANDLE_BY_IDX(DT_PATH(zephyr_user), digital_pin_gpios, i)),               \
		 DT_PHA_BY_IDX(DT_PATH(zephyr_user), digital_pin_gpios, i, pin))) == 1)

#if !LISTIFY(DT_PROP_LEN(DT_PATH(zephyr_user), digital_pin_gpios), DIGITAL_PIN_CHECK_UNIQUE, (&&))
#error "digital_pin_gpios has duplicate definition"
#endif

#undef DIGITAL_PIN_CHECK_UNIQUE

/* Return the index of the entry if matched, otherwise return 0 */
#define LED_BUILTIN_INDEX_BY_REG_AND_PINNUM(n, p, i, dev, num)                                     \
	(DIGITAL_PIN_EXISTS(n, p, i, dev, num) ? i : 0)

/* Only the matched pin returns non-zero, so the sum is its index */
#define DIGITAL_PIN_GPIOS_FIND_PIN(dev, pin)                                                       \
	DT_FOREACH_PROP_ELEM_SEP_VARGS(DT_PATH(zephyr_user), digital_pin_gpios,                        \
								   LED_BUILTIN_INDEX_BY_REG_AND_PINNUM, (+), dev, pin)

/* Helper: resolve a DTS alias node (with a gpios cell) to its Arduino pin number */
#define DIGITAL_PIN_GPIOS_FIND_NODE(node)                                                          \
	DIGITAL_PIN_GPIOS_FIND_PIN(DT_REG_ADDR(DT_PHANDLE_BY_IDX(node, gpios, 0)),                     \
							   DT_PHA_BY_IDX(node, gpios, 0, pin))

#endif /* DT_PROP_LEN > 0 */

/* -- Enum helpers -------------------------------------------------- */

#define ZARD_DN_ENUMS(n, p, i) D##i = i
#define ZARD_AN_ENUMS(n, p, i)                                                                     \
	A##i = DIGITAL_PIN_GPIOS_FIND_PIN(DT_REG_ADDR(DT_PHANDLE_BY_IDX(DT_PATH(zephyr_user), p, i)),  \
									  DT_PHA_BY_IDX(DT_PATH(zephyr_user), p, i, pin)),
#define ZARD_DAC_ENUMS(n, p, i) DAC##i = i,

/* -- ADC helpers --------------------------------------------------- */

/* Map a zephyr,user/adc-pin-gpios entry to its global Arduino pin number. */
#define ZARD_ADC_PINS(n, p, i)                                                                     \
	DIGITAL_PIN_GPIOS_FIND_PIN(DT_REG_ADDR(DT_PHANDLE_BY_IDX(DT_PATH(zephyr_user), p, i)),         \
							   DT_PHA_BY_IDX(DT_PATH(zephyr_user), p, i, pin)),

/* Emit an adc_dt_spec initialiser for one element of zephyr,user/io-channels. */
#define ZARD_ADC_DT_SPEC(n, p, i) ADC_DT_SPEC_GET_BY_IDX(n, i),

/* -- PWM helpers --------------------------------------------------- */

/* Map a zephyr,user/pwm-pin-gpios entry to its global Arduino pin number. */
#define ZARD_PWM_PINS(n, p, i)                                                                     \
	DIGITAL_PIN_GPIOS_FIND_PIN(DT_REG_ADDR(DT_PHANDLE_BY_IDX(DT_PATH(zephyr_user), p, i)),         \
							   DT_PHA_BY_IDX(DT_PATH(zephyr_user), p, i, pin)),

/* Emit a pwm_dt_spec initialiser for one element of zephyr,user/pwms. */
#define ZARD_PWM_DT_SPEC(n, p, i) PWM_DT_SPEC_GET_BY_IDX(n, i),
