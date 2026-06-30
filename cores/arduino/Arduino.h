/*
 * Copyright (c) 2022 Dhruva Gole
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "api/ArduinoAPI.h"

// redefined by Zephyr below
#undef min
#undef max

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/dac.h>
#include <zephyr/drivers/i2c.h>
#include <math.h>

#include "dt_user_pins.h"

#if DT_NODE_HAS_PROP(DT_PATH(zephyr_user), builtin_led_gpios) &&                                   \
	(DT_PROP_LEN(DT_PATH(zephyr_user), builtin_led_gpios) > 0)

#if !(DT_FOREACH_PROP_ELEM_SEP_VARGS(                                                              \
		  DT_PATH(zephyr_user), digital_pin_gpios, DIGITAL_PIN_EXISTS, (+),                        \
		  DT_REG_ADDR(DT_PHANDLE_BY_IDX(DT_PATH(zephyr_user), builtin_led_gpios, 0)),              \
		  DT_PHA_BY_IDX(DT_PATH(zephyr_user), builtin_led_gpios, 0, pin)) > 0)
#warning "pin not found in digital_pin_gpios"
#else
#define ZARD_LED_BUILTIN                                                                           \
	DIGITAL_PIN_GPIOS_FIND_PIN(                                                                    \
		DT_REG_ADDR(DT_PHANDLE_BY_IDX(DT_PATH(zephyr_user), builtin_led_gpios, 0)),                \
		DT_PHA_BY_IDX(DT_PATH(zephyr_user), builtin_led_gpios, 0, pin))
#endif

/* If digital-pin-gpios is not defined, tries to use the led0 alias */
#elif DT_NODE_EXISTS(DT_ALIAS(led0))

#if !(DT_FOREACH_PROP_ELEM_SEP_VARGS(DT_PATH(zephyr_user), digital_pin_gpios, DIGITAL_PIN_EXISTS,  \
									 (+),                                                          \
									 DT_REG_ADDR(DT_PHANDLE_BY_IDX(DT_ALIAS(led0), gpios, 0)),     \
									 DT_PHA_BY_IDX(DT_ALIAS(led0), gpios, 0, pin)) > 0)
#warning "pin not found in digital_pin_gpios"
#else
#define ZARD_LED_BUILTIN                                                                           \
	DIGITAL_PIN_GPIOS_FIND_PIN(DT_REG_ADDR(DT_PHANDLE_BY_IDX(DT_ALIAS(led0), gpios, 0)),           \
							   DT_PHA_BY_IDX(DT_ALIAS(led0), gpios, 0, pin))
#endif

#endif // builtin_led_gpios

/*
 * expand as
 * enum digitalPins { D0, D1, ... LED... NUM_OF_DIGITAL_PINS };
 */
enum digitalPins {
#if DT_PROP_LEN(DT_PATH(zephyr_user), digital_pin_gpios) > 0
	DT_FOREACH_PROP_ELEM_SEP(DT_PATH(zephyr_user), digital_pin_gpios, ZARD_DN_ENUMS, (, )),
#endif
	NUM_OF_DIGITAL_PINS
};

#ifdef CONFIG_ADC

enum analogPins {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), adc_pin_gpios, ZARD_AN_ENUMS)
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

#if !defined(LED_BUILTIN) && defined(ZARD_LED_BUILTIN)
#define LED_BUILTIN ZARD_LED_BUILTIN
#endif // LED_BUILTIN

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
