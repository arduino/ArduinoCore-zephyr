/*
 * Copyright (c) 2022 Dhruva Gole
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "pure_analog_pins.h"

// SPI2 and I2C3 (primary SPI/Wire) pins; indices match digital-pin-gpios
#define MOSI        D8  /* PC_3 */
#define MISO        D10 /* PC_2 */
#define SCK         D9  /* PI_1 */
#define SS          D7  /* PI_0 */
#define SDA         D11 /* PH_8 */
#define SCL         D12 /* PH_7 */

// RGB LED (indices match digital-pin-gpios; LED_BUILTIN is auto-defined to LEDG)
#define LEDR        D25 /* PK_5 */
#define LEDG        D26 /* PK_6 */
#define LEDB        D27 /* PK_7 */

#define SE05X_ENABLE_GPIO (26u)
