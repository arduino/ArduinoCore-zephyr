/*
 * Copyright (c) 2022 Dhruva Gole
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "pure_analog_pins.h"

#define LEDR    (23u) /* PE3 */
#define LEDG    (24u) /* PC13 */
#define LEDB    (25u) /* PF4 */

#define SS      (7u)  /* PE11 */
#define MOSI    (8u)  /* PE14 */
#define SCK     (9u)  /* PE12 */
#define MISO    (10u) /* PE13 */

#define SDA     (11u) /* PB9 */
#define SCL     (12u) /* PB8 */

/* SPI interface for LSM6DSOX IMU is on SPI1 */
#define LSM6DS_DEFAULT_SPI SPI1
#define PIN_SPI_SS1    (6u)  /* PF6 */
#define PIN_SPI_MOSI1  (20u) /* PF11 */
#define PIN_SPI_SCK1   (21u) /* PF7 */
#define PIN_SPI_MISO1  (22u) /* PF8 */
#define LSM6DS_INT     (27u) /* PA1 */

#define SE05X_ENABLE_GPIO (28u) /* PG0 */

/* Allocate camera buffers from the libc heap. */
#define ARDUINO_CAMERA_USE_MALLOC
