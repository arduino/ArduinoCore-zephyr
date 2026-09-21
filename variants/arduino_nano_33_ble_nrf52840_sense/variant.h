/*
 * Copyright (c) 2022 Dhruva Gole
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// SPI and I2C (Wire) pins; indices match digital-pin-gpios
#define MOSI    D11 /* P1.01 */
#define MISO    D12 /* P1.08 */
#define SCK     D13 /* P0.13 */
#define SS      D10 /* P1.02 */
#define SDA     D18 /* P0.31 */
#define SCL     D19 /* P0.02 */

// RGB LED and power (indices match digital-pin-gpios)
#define LEDR    D22 /* P0.24 */
#define LEDG    D23 /* P0.16 */
#define LEDB    D24 /* P0.06 */
#define LED_PWR D25 /* P1.09 */

/* Arduino_APDS9960 library */
#define PIN_INT_APDS 26 /* D26 - P0.19 */
#define APDS9960_INT_PIN PIN_INT_APDS
#define APDS9960_WIRE_INSTANCE Wire1

#define ARDUINO_ARDUINO_NANO33BLE /* This is required for the proper functioning of some libraries (i.e. Arduino_LPS22HB, Arduino_HS300x. etc. )*/