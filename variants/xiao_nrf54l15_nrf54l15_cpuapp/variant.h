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

/* XIAO connector roles match the other XIAO boards: Serial1 = D6/D7,
 * Wire = D4/D5, SPI = D8/D9/D10; Wire1 (i2c30) is on the back pads
 * D11 (SCL) / D12 (SDA). D0..D18, LED_BUILTIN (D16 = P2.00 user LED)
 * and the analog pin set are generated from the devicetree
 * (zephyr,user) in xiao_nrf54l15_nrf54l15_cpuapp.overlay.
 * The board identity macro ARDUINO_SEEED_XIAO_NRF54L15 is injected by
 * the build system from boards.txt (build.board), so it is not
 * redefined here.
 */

/* Analog numbering is derived positionally from the devicetree:
 *   A0-A3 = D0-D3 (AIN0-3), A4 = D5 (AIN4; D4 has no analog function),
 *   A5 = D18 / VBAT (AIN7, battery voltage; the sense rail is enabled
 *   at boot by the board devicetree).
 * Note this differs from the Seeed community core (nrf54l15clean),
 * which names D5's analog function A5 and VBAT A7.
 */
