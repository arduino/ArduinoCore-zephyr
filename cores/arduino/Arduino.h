/*
 * Copyright (c) 2022 Dhruva Gole
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "api/ArduinoAPI.h"

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/i2c.h>
#include <math.h>

#if DT_PROP_LEN(DT_PATH(zephyr_user), digital_pin_gpios) > 0
#define DIGITAL_PIN_EXISTS(n, p, i, dev, num)                                                      \
	(((dev == DT_REG_ADDR(DT_PHANDLE_BY_IDX(n, p, i))) &&                                      \
	  (num == DT_PHA_BY_IDX(n, p, i, pin)))                                                    \
		 ? 1                                                                               \
		 : 0)

/* Check all pins are defined only once */
#define DIGITAL_PIN_CHECK_UNIQUE(i, _)                                                             \
	((DT_FOREACH_PROP_ELEM_SEP_VARGS(                                                          \
		 DT_PATH(zephyr_user), digital_pin_gpios, DIGITAL_PIN_EXISTS, (+),                 \
		 DT_REG_ADDR(DT_PHANDLE_BY_IDX(DT_PATH(zephyr_user), digital_pin_gpios, i)),       \
		 DT_PHA_BY_IDX(DT_PATH(zephyr_user), digital_pin_gpios, i, pin))) == 1)

#if !LISTIFY(DT_PROP_LEN(DT_PATH(zephyr_user), digital_pin_gpios), DIGITAL_PIN_CHECK_UNIQUE, (&&))
#error "digital_pin_gpios has duplicate definition"
#endif

#undef DIGITAL_PIN_CHECK_UNIQUE
#endif

#ifndef LED_BUILTIN

/* Return the index of it if matched, oterwise return 0 */
#define LED_BUILTIN_INDEX_BY_REG_AND_PINNUM(n, p, i, dev, num)                                     \
	(DIGITAL_PIN_EXISTS(n, p, i, dev, num) ? i : 0)

/* Only matched pin returns non-zero value, so the sum is matched pin's index */
#define DIGITAL_PIN_GPIOS_FIND_PIN(dev, pin)                                                     \
	DT_FOREACH_PROP_ELEM_SEP_VARGS(DT_PATH(zephyr_user), digital_pin_gpios,                    \
				       LED_BUILTIN_INDEX_BY_REG_AND_PINNUM, (+), dev, pin)

#if DT_NODE_HAS_PROP(DT_PATH(zephyr_user), builtin_led_gpios) &&                                   \
	(DT_PROP_LEN(DT_PATH(zephyr_user), builtin_led_gpios) > 0)

#if !(DT_FOREACH_PROP_ELEM_SEP_VARGS(                                                               \
	     DT_PATH(zephyr_user), digital_pin_gpios, DIGITAL_PIN_EXISTS, (+),                     \
	     DT_REG_ADDR(DT_PHANDLE_BY_IDX(DT_PATH(zephyr_user), builtin_led_gpios, 0)),           \
	     DT_PHA_BY_IDX(DT_PATH(zephyr_user), builtin_led_gpios, 0, pin)) > 0)
#warning "pin not found in digital_pin_gpios"
#else
#define LED_BUILTIN                                                                                \
	DIGITAL_PIN_GPIOS_FIND_PIN(                                                              \
		DT_REG_ADDR(DT_PHANDLE_BY_IDX(DT_PATH(zephyr_user), builtin_led_gpios, 0)),        \
		DT_PHA_BY_IDX(DT_PATH(zephyr_user), builtin_led_gpios, 0, pin))
#endif

/* If digital-pin-gpios is not defined, tries to use the led0 alias */
#elif DT_NODE_EXISTS(DT_ALIAS(led0))

#if !(DT_FOREACH_PROP_ELEM_SEP_VARGS(DT_PATH(zephyr_user), digital_pin_gpios, DIGITAL_PIN_EXISTS,   \
				    (+), DT_REG_ADDR(DT_PHANDLE_BY_IDX(DT_ALIAS(led0), gpios, 0)), \
				    DT_PHA_BY_IDX(DT_ALIAS(led0), gpios, 0, pin)) > 0)
#warning "pin not found in digital_pin_gpios"
#else
#define LED_BUILTIN                                                                                \
	DIGITAL_PIN_GPIOS_FIND_PIN(DT_REG_ADDR(DT_PHANDLE_BY_IDX(DT_ALIAS(led0), gpios, 0)),     \
				     DT_PHA_BY_IDX(DT_ALIAS(led0), gpios, 0, pin))
#endif

#endif // builtin_led_gpios

#endif // LED_BUILTIN

#define DN_ENUMS(n, p, i) D##i = i

/*
 * expand as
 * enum digitalPins { D0, D1, ... LED... NUM_OF_DIGITAL_PINS };
 */
enum digitalPins {
#if DT_PROP_LEN(DT_PATH(zephyr_user), digital_pin_gpios) > 0
	DT_FOREACH_PROP_ELEM_SEP(DT_PATH(zephyr_user), digital_pin_gpios, DN_ENUMS, (, )),
#endif
	NUM_OF_DIGITAL_PINS
};

#ifdef CONFIG_ADC

#define AN_ENUMS(n, p, i) A ## i = DIGITAL_PIN_GPIOS_FIND_PIN( \
		DT_REG_ADDR(DT_PHANDLE_BY_IDX(DT_PATH(zephyr_user), p, i)),        \
		DT_PHA_BY_IDX(DT_PATH(zephyr_user), p, i, pin)),
enum analogPins { DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user),
				       adc_pin_gpios, AN_ENUMS) };

// We provide analogReadResolution APIs
void analogReadResolution(int bits);

#endif

void interrupts(void);
void noInterrupts(void);

int digitalPinToInterrupt(pin_size_t pin);

#define digitalPinToPort(x) (x)
#define digitalPinToBitMask(x) (x)
#define portOutputRegister(x) (x)
#define portInputRegister(x) (x)

#include <variant.h>
#ifdef __cplusplus
#include <SerialUSB.h>
#include <zephyrSerial.h>
#include <strings.h>
#include <api/itoa.h>
#include <time_macros.h>

// Allow namespace-less operations if Arduino.h is included
using namespace arduino;

#endif // __cplusplus