/*
 * Copyright (c) 2026 Seeed Technology Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define MOSI 10
#define MISO 9
#define SCK  8
#define SS   10
#define SDA  4
#define SCL  5

/* Pin naming matches the Seeed nRF52 core so that existing libraries and
 * wiki examples recognize the board. The board identity macro
 * ARDUINO_SEEED_XIAO_NRF52840 is injected by the build system from
 * boards.txt (build.board), so it is not redefined here. LED_BUILTIN is
 * derived from the overlay (builtin-led-gpios), not defined here. */

/* RGB LED indexes, same values as the Seeed nRF52 core (D12 is blue and
 * D13 is green there -- not physical R,G,B order). */
#define LED_RED   11
#define LED_GREEN 13
#define LED_BLUE  12
