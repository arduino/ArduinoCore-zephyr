/*
 * Copyright (c) 2022 Dhruva Gole
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "pure_analog_pins.h"

// SPI1 and I2C2 (primary SPI/Wire) pins; indices match digital-pin-gpios
#define MOSI        D90 /* PD_7  */
#define MISO        D89 /* PG_9  */
#define SCK         D91 /* PB_3  */
#define SS          D10 /* Arduino header D10 CS */
#define SDA         D20 /* PB_11 */
#define SCL         D21 /* PH_4  */

// RGB LED (indices match digital-pin-gpios; LED_BUILTIN is auto-defined to LEDG)
#define LEDR        D86 /* PI_12 */
#define LEDG        D87 /* PJ_13 */
#define LEDB        D88 /* PE_3 */

#include "../common/gpio_lowlevel_stm32.h"
