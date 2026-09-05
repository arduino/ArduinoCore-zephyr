/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/llext/symbol.h>

#define EXPORT_DEVICE_API_SYMS(name)                                                               \
	STRUCT_SECTION_START_EXTERN(name##_driver_api);                                                \
	extern const uint8_t _##name##_driver_api_ext_end[];                                           \
	EXPORT_SYMBOL(STRUCT_SECTION_START(name##_driver_api));                                        \
	EXPORT_SYMBOL(_##name##_driver_api_ext_end);

/* Symbols required by DEVICE_API_GET() checks in sketch code. */
#if defined(CONFIG_ADC)
#include <zephyr/drivers/adc.h>
EXPORT_DEVICE_API_SYMS(adc)
#endif
#if defined(CONFIG_CAN)
#include <zephyr/drivers/can.h>
EXPORT_DEVICE_API_SYMS(can)
#endif
#if defined(CONFIG_CLOCK_CONTROL)
#include <zephyr/drivers/clock_control.h>
EXPORT_DEVICE_API_SYMS(clock_control)
#endif
#if defined(CONFIG_COUNTER)
#include <zephyr/drivers/counter.h>
EXPORT_DEVICE_API_SYMS(counter)
#endif
#if defined(CONFIG_DAC)
#include <zephyr/drivers/dac.h>
EXPORT_DEVICE_API_SYMS(dac)
#endif
#if defined(CONFIG_ENTROPY_GENERATOR)
#include <zephyr/drivers/entropy.h>
EXPORT_DEVICE_API_SYMS(entropy)
#endif
#if defined(CONFIG_FLASH)
#include <zephyr/drivers/flash.h>
EXPORT_DEVICE_API_SYMS(flash)
#endif
#if defined(CONFIG_GPIO)
#include <zephyr/drivers/gpio.h>
EXPORT_DEVICE_API_SYMS(gpio)
#endif
#if defined(CONFIG_I2C)
#include <zephyr/drivers/i2c.h>
EXPORT_DEVICE_API_SYMS(i2c)
#endif
#if defined(CONFIG_PWM)
#include <zephyr/drivers/pwm.h>
EXPORT_DEVICE_API_SYMS(pwm)
#endif
#if defined(CONFIG_REGULATOR)
#include <zephyr/drivers/regulator.h>
EXPORT_DEVICE_API_SYMS(regulator)
#endif
#if defined(CONFIG_RTC)
#include <zephyr/drivers/rtc.h>
EXPORT_DEVICE_API_SYMS(rtc)
#endif
#if defined(CONFIG_SPI)
#include <zephyr/drivers/spi.h>
EXPORT_DEVICE_API_SYMS(spi)
#endif
#if defined(CONFIG_SERIAL)
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/uart/uart_internal.h>
EXPORT_DEVICE_API_SYMS(uart)
#endif
#if defined(CONFIG_VIDEO)
#include <zephyr/drivers/video.h>
EXPORT_DEVICE_API_SYMS(video)
#endif
