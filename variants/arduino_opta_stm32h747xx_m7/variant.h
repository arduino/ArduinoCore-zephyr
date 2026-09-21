/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/* Pin indices below match the digital-pin-gpios order in the .overlay file. */

// Relay outputs
#define RELAY1      D0  /* PI_6 */
#define RELAY2      D1  /* PI_5 */
#define RELAY3      D2  /* PI_7 */
#define RELAY4      D3  /* PI_4 */

// Status LEDs (one per relay)
#define LED_D0      D4  /* PI_0 */
#define LED_D1      D5  /* PI_1 */
#define LED_D2      D6  /* PI_3 */
#define LED_D3      D7  /* PH_15 */

#define LED_RELAY1  LED_D0
#define LED_RELAY2  LED_D1
#define LED_RELAY3  LED_D2
#define LED_RELAY4  LED_D3

// RGB user LED (LED_BUILTIN is auto-defined to LEDG via builtin-led-gpios)
#define LEDR        D9  /* PH_11 */
#define LEDG        D8  /* PH_12 */
#define LEDB        D10 /* PE_5 */
#define LED_RESET   LEDG
#define LED_USER    LEDB

// User button
#define BTN_USER    D11 /* PE_4 */

// Analog input aliases matching the serigraphy (I1..I8 -> A0..A7)
#define I1          A0
#define I2          A1
#define I3          A2
#define I4          A3
#define I5          A4
#define I6          A5
#define I7          A6
#define I8          A7
