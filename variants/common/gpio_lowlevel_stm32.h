/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* native_sim posing as an STM32 board has no STM32 headers: keep the generic
 * port macros of the core there
 */
#ifndef CONFIG_ARCH_POSIX

#include <stm32_ll_gpio.h>
#include <gpio/gpio_stm32.h>

#undef digitalPinToPort
#undef digitalPinToBitMask
#undef portOutputRegister
#undef portInputRegister

#define digitalPinToPort(x)    (GPIO_TypeDef *)(((struct gpio_stm32_config *)(digitalPinToPortDevice(x)->config))->base)
#define digitalPinToPinName(x) (digitalPinToPinIndex(x))
#define STM_LL_GPIO_PIN(x)     (1U << x)

#define digitalPinToBitMask(x) STM_LL_GPIO_PIN(x)
#define portOutputRegister(x)  (digitalPinToPort(x)->ODR)
#define portInputRegister(x)   (digitalPinToPort(x)->IDR)

#endif /* CONFIG_ARCH_POSIX */
