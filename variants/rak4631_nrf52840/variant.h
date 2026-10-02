/*
 * Copyright (c) 2026 RAKwireless Technology Limited
 * Author: sercan.erat@rakwireless.com
 * SPDX-License-Identifier: Apache-2.0
 */

/* This header only provides the BSP-compatible names! */

#ifndef _VARIANT_RAK4631_
#define _VARIANT_RAK4631_

/*
 * Internal Pins
 *
 * RAK4630 module-internal signals. They are not routed to the WisBlock
 * connector and have no Arduino pin number, so a sketch cannot (and must
 * not) drive them: the SX1262 radio is owned by the Zephyr 'lora0' device
 * on SPI1, and the low-frequency crystal by the clock control driver.
 *
 *   P0.00  XL1       32.768 kHz crystal
 *   P0.01  XL2       32.768 kHz crystal
 *   P1.05  ANT_SW    SX1262 RX enable, RF switch
 *   P1.06  NRESET    SX1262 reset
 *   P1.07  DIO2      SX1262 TX enable (driven by the radio itself)
 *   P1.10  SPI_NSS   SX1262 chip select (active low)
 *   P1.11  SPI_CLK   SX1262 SPI clock
 *   P1.12  SPI_MOSI  SX1262 SPI MOSI
 *   P1.13  SPI_MISO  SX1262 SPI MISO
 *   P1.14  BUSY      SX1262 busy
 *   P1.15  DIO1      SX1262 interrupt
 */

/*
 * Connector Pins
 *
 * WisBlock connector signals without an Arduino pin number: the bus
 * pins are set up by devicetree pin control and belong to a Zephyr
 * peripheral, the rest are not mapped by this variant. The number in
 * brackets is the connector pin.
 *
 *   UART0 ('Serial1'): P0.20 TXD0, P0.19 RXD0
 *   UART1 ('Serial2'): P0.16 TXD1, P0.15 RXD1
 *   I2C0  ('Wire'):    P0.13 I2C1_SDA, P0.14 I2C1_SCL
 *   I2C1  (disabled):  -
 *   SPI2  ('SPI'):     P0.03 SPI_CLK, P0.30 SPI_MOSI,
 *                      P0.29 SPI_MISO, P0.26 SPI_CS
 *   QSPI:              P0.03 SCK, P0.30 DIO0, P0.29 DIO1, P0.28 DIO2,
 *                      P0.02 DIO3, P0.26 CSN
 *   ADC:               P0.05 A0, P0.31 A1, P0.04 A2, P0.28 A3
 *   Others:            P1.01 SW1, P1.02 SW2, P0.18 RESET
 *
 * I2C1 is disabled because it shares the TWI1/SPI1 hardware instance
 * with the SX1262 SPI. QSPI shares its pins with SPI2, with A3/D6 (P0.28)
 * and with LED3/D13 (P0.02), so QSPI and the WisBlock SPI cannot be used
 * at the same time. P0.18 is the nRF52840 reset line (UICR 'gpio-as-nreset'),
 * not a usable GPIO.
 */

/*
 * WisBlock Base GPIO definitions
 */
#define WB_IO1    D0  /* P0.17, SLOT_A SLOT_B IO_SLOT, PWM */
#define WB_IO2    D1  /* P1.02, SLOT_A SLOT_B IO_SLOT, PWM, 3V3_S power enable */
#define WB_IO3    D2  /* P0.21, SLOT_C IO_SLOT, PWM */
#define WB_IO4    D3  /* P0.04, SLOT_C IO_SLOT, PWM, also A2 */
#define WB_IO5    D4  /* P0.09, SLOT_D IO_SLOT, PWM (NFC pin used as GPIO) */
#define WB_IO6    D5  /* P0.10, SLOT_D IO_SLOT, PWM (NFC pin used as GPIO) */
#define WB_IO7    D6  /* P0.28, IO_SLOT, PWM, also A3 */
#define WB_LED1   D7  /* P1.03, green LED, PWM */
#define WB_LED2   D8  /* P1.04, blue LED, PWM */
#define WB_A0     D9  /* P0.05, IO_SLOT */
#define WB_A1     D10 /* P0.31, IO_SLOT */
#define WB_SPI_CS D11 /* P0.26, SLOT_A SLOT_B SLOT_C SLOT_D IO_SLOT */
#define WB_SW1    D12 /* P1.01, IO_SLOT */
#define WB_LED3   D13 /* P0.02, base board LED3, shared with QSPI DIO3 */

/*
 * LEDs (active high)
 *
 * LED1/LED2 are fitted on the RAK4631 module; LED3 is only a connector
 * signal, so its polarity depends on the base board.
 */
#define PIN_LED1 WB_LED1
#define PIN_LED2 WB_LED2
#define PIN_LED3 WB_LED3

#define LED_BUILTIN PIN_LED1
#define LED_CONN    PIN_LED2

#define LED_GREEN PIN_LED1
#define LED_BLUE  PIN_LED2
#define GREEN_LED PIN_LED1
#define BLUE_LED  PIN_LED2

#define LED_STATE_ON 1

/*
 * Analog pins (A0..A3 map to SAADC channels 3, 7, 2, 4)
 */
#define PIN_A0 A0 /* WB_A0 */
#define PIN_A1 A1 /* WB_A1 */
#define PIN_A2 A2 /* WB_IO4 */
#define PIN_A3 A3 /* WB_IO7 */

/*
 * On the RAK19007 base board, A0 also reads the battery voltage
 * divider (1.5 MOhm / 2.5 MOhm): VBAT = A0 * 5 / 3.
 */
#define PIN_VBAT PIN_A0

/*
 * SPI interfaces
 */
#define SPI_INTERFACES_COUNT 1

#define PIN_SPI_CS WB_SPI_CS
#define SS         PIN_SPI_CS

/*
 * Wire interfaces
 */
#define WIRE_INTERFACES_COUNT 1

#endif /* _VARIANT_RAK4631_ */
