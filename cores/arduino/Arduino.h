/*
 * Copyright (c) 2022 Dhruva Gole
 * Copyright (c) 2026 TOKITA Hiroshi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "api/ArduinoAPI.h"

// redefined by Zephyr below
#undef min
#undef max

#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/dac.h>
#include <zephyr/drivers/i2c.h>
#include <math.h>

/*
 * ZARD_DT_HAS_USER_PINS: 1 when Arduino pin mapping is provided through the
 * zephyr,user digital-pin-gpios property; 0 when it is derived from connector
 * nodes (arduino_header gpio-map or similar).
 */
#define ZARD_DT_HAS_USER_PINS DT_NODE_HAS_PROP(DT_PATH(zephyr_user), digital_pin_gpios)

#if ZARD_DT_HAS_USER_PINS
#include "dt_user_pins.h"
#else // ZARD_DT_HAS_USER_PINS
#include "dt_connector.h"
#endif // ZARD_DT_HAS_USER_PINS

/*
 * Builtin LED helpers. 'zephyr,user/builtin_led_gpios' takes precedence (one
 * LED per array element); otherwise the led0..ledN aliases are used. The source
 * choice is hidden behind the index-parameterized macros ZARD_LED_PRESENT(n)
 * and ZARD_LED_AT(n), so each LED_BUILTIN<n> (emitted after <variant.h>) is a
 * single uniform reference. Only the names have to be spelled out, since a
 * #define cannot be produced by a loop.
 */
#define ZARD_LED_FROM_LIST(n)                                                                      \
	ZARD_LED_VALUE(DT_PHANDLE_BY_IDX(DT_PATH(zephyr_user), builtin_led_gpios, n),                  \
				   DT_PHA_BY_IDX(DT_PATH(zephyr_user), builtin_led_gpios, n, pin))
#define ZARD_LED_OK_LIST(n)                                                                        \
	ZARD_LED_IN_LIST(DT_PHANDLE_BY_IDX(DT_PATH(zephyr_user), builtin_led_gpios, n),                \
					 DT_PHA_BY_IDX(DT_PATH(zephyr_user), builtin_led_gpios, n, pin))
#define ZARD_LED_ALIAS_NODE(n) DT_ALIAS(UTIL_CAT(led, n))
#define ZARD_LED_FROM_ALIAS(n)                                                                     \
	ZARD_LED_VALUE(DT_PHANDLE_BY_IDX(ZARD_LED_ALIAS_NODE(n), gpios, 0),                            \
				   DT_PHA_BY_IDX(ZARD_LED_ALIAS_NODE(n), gpios, 0, pin))
#define ZARD_LED_OK_ALIAS(n)                                                                       \
	ZARD_LED_IN_LIST(DT_PHANDLE_BY_IDX(ZARD_LED_ALIAS_NODE(n), gpios, 0),                          \
					 DT_PHA_BY_IDX(ZARD_LED_ALIAS_NODE(n), gpios, 0, pin))

#if DT_NODE_HAS_PROP(DT_PATH(zephyr_user), builtin_led_gpios) &&                                   \
	(DT_PROP_LEN(DT_PATH(zephyr_user), builtin_led_gpios) > 0)
#define ZARD_LED_PRESENT(n)                                                                        \
	((DT_PROP_LEN(DT_PATH(zephyr_user), builtin_led_gpios) > (n)) && ZARD_LED_OK_LIST(n))
#define ZARD_LED_AT(n) ZARD_LED_FROM_LIST(n)
#elif DT_NODE_EXISTS(DT_ALIAS(led0))
#define ZARD_LED_PRESENT(n) (DT_NODE_EXISTS(ZARD_LED_ALIAS_NODE(n)) && ZARD_LED_OK_ALIAS(n))
#define ZARD_LED_AT(n)      ZARD_LED_FROM_ALIAS(n)
#else
#define ZARD_LED_PRESENT(n) 0
#define ZARD_LED_AT(n)      0
#endif

/*
 * expand as
 * enum digitalPins { D0, D1, ... LED... NUM_OF_DIGITAL_PINS };
 */
enum digitalPins {
#if ZARD_DT_HAS_USER_PINS
	DT_FOREACH_PROP_ELEM_SEP(DT_PATH(zephyr_user), digital_pin_gpios, ZARD_DN_ENUMS, (, )),
	NUM_OF_DIGITAL_PINS
#elif defined(ZARD_CONNECTOR)
	DT_FOREACH_MAP_ENTRY(DT_NODELABEL(ZARD_CONNECTOR), gpio_map, ZARD_CONN_DN_ENUMS)
	NUM_OF_DIGITAL_PINS = ZARD_GLOBAL_GPIO_COUNT
#else
	NUM_OF_DIGITAL_PINS
#endif
};

#ifdef CONFIG_ADC

enum analogPins {
#if DT_NODE_HAS_PROP(DT_PATH(zephyr_user), adc_pin_gpios)
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), adc_pin_gpios, ZARD_AN_ENUMS)
#elif defined(ZARD_CONNECTOR)
	DT_FOREACH_MAP_ENTRY(DT_NODELABEL(ZARD_CONNECTOR), gpio_map, ZARD_CONN_AN_ENUMS)
#endif
};

// We provide analogReadResolution APIs
void analogReadResolution(int bits);

#endif

#ifdef CONFIG_DAC

#undef DAC0
#undef DAC1
#undef DAC2
#undef DAC3

enum dacPins {
#if DT_PROP_LEN_OR(DT_PATH(zephyr_user), dac_channels, 0) > 0
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), dac_channels, ZARD_DAC_ENUMS)
#endif
	NUM_OF_DACS
};

#endif

void interrupts(void);
void noInterrupts(void);

int digitalPinToInterrupt(pin_size_t pin);

// These defines can be overridden by variant.h if implemented
#define digitalPinToPort(x)    (x)
#define digitalPinToBitMask(x) (x)
#define portOutputRegister(x)  (x)
#define portInputRegister(x)   (x)

const struct device *digitalPinToPortDevice(pin_size_t pinNumber);
int digitalPinToPinIndex(pin_size_t pinNumber);

#if defined(CONFIG_PWM) || defined(CONFIG_DAC)
void analogWriteResolution(int bits);
#endif

#if defined(__arm__)
#define F_CPU (SystemCoreClock)
#endif

#include <variant.h>

#if !defined(LED_BUILTIN) && ZARD_LED_PRESENT(0)
#define LED_BUILTIN ZARD_LED_AT(0)
#endif
#if !defined(LED_BUILTIN1) && ZARD_LED_PRESENT(1)
#define LED_BUILTIN1 ZARD_LED_AT(1)
#endif
#if !defined(LED_BUILTIN2) && ZARD_LED_PRESENT(2)
#define LED_BUILTIN2 ZARD_LED_AT(2)
#endif
#if !defined(LED_BUILTIN3) && ZARD_LED_PRESENT(3)
#define LED_BUILTIN3 ZARD_LED_AT(3)
#endif
#if !defined(LED_BUILTIN4) && ZARD_LED_PRESENT(4)
#define LED_BUILTIN4 ZARD_LED_AT(4)
#endif
#if !defined(LED_BUILTIN5) && ZARD_LED_PRESENT(5)
#define LED_BUILTIN5 ZARD_LED_AT(5)
#endif
#if !defined(LED_BUILTIN6) && ZARD_LED_PRESENT(6)
#define LED_BUILTIN6 ZARD_LED_AT(6)
#endif
#if !defined(LED_BUILTIN7) && ZARD_LED_PRESENT(7)
#define LED_BUILTIN7 ZARD_LED_AT(7)
#endif
#if !defined(LED_BUILTIN8) && ZARD_LED_PRESENT(8)
#define LED_BUILTIN8 ZARD_LED_AT(8)
#endif
#if !defined(LED_BUILTIN9) && ZARD_LED_PRESENT(9)
#define LED_BUILTIN9 ZARD_LED_AT(9)
#endif
#if !defined(LED_BUILTIN10) && ZARD_LED_PRESENT(10)
#define LED_BUILTIN10 ZARD_LED_AT(10)
#endif
#if !defined(LED_BUILTIN11) && ZARD_LED_PRESENT(11)
#define LED_BUILTIN11 ZARD_LED_AT(11)
#endif

#include <inlines.h>

#ifdef __cplusplus
#include <SerialUSB.h>
#include <zephyrSerial.h>
#include <strings.h>
#include <api/itoa.h>
#include <time_macros.h>
#include <overloads.h>

// Allow namespace-less operations if Arduino.h is included
using namespace arduino;

#if __has_include("postvariant.h")
#include "postvariant.h"
#endif

#endif // __cplusplus
