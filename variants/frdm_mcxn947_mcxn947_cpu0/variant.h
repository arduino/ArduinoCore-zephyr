/*
 * Copyright (c) 2022 Dhruva Gole
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// TODO: correctly handle these legacy defines
#define MOSI    0
#define MISO    0
#define SCK     0
#define SS      0
#define SDA     0
#define SCL     0

/*
 * The core names analog pins A0.. in adc-pin-gpios order, but this board's
 * A0/A1 are analog-only pads with no GPIO, so the first entry is the
 * header's A2. Define the header names explicitly as their digital pins.
 */
#define A2      18 /* P0_14 */
#define A3      19 /* P0_22 */
#define A4      20 /* P0_15 */
#define A5      16 /* P0_23, shared with SW2 */
