/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifndef __PINS_ARDUINO__
#define __PINS_ARDUINO__

#include <stdint.h>

// Pin count
// ----
#define PINS_COUNT           (30u)
#define NUM_DIGITAL_PINS     (30u)
#define NUM_ANALOG_INPUTS    (4u)
#define NUM_ANALOG_OUTPUTS   (0u)

// LEDs
// ----
#define PIN_LED     (13u)
#define LED_BUILTIN PIN_LED

// Note: RGB LEDs (LEDR, LEDG, LEDB) are on the NINA module
// and require WiFiNINA library to control

// Analog pins
// -----------
// Note: A0-A3 are defined as enum values in Arduino.h via devicetree
#define PIN_A0 (14u)
#define PIN_A1 (15u)
#define PIN_A2 (16u)
#define PIN_A3 (17u)

#define ADC_RESOLUTION 12

// PDM Interfaces
// ---------------
#define PIN_PDM_CLK  (23)
#define PIN_PDM_DIN  (22)
#define PIN_PDM_PWR  (-1)  /* no dedicated power pin on this board */

// IMU Interrupt
#define INT_IMU      (21)

// Serial
// ------
#define PIN_SERIAL_RX (0ul)
#define PIN_SERIAL_TX (1ul)

#define SERIAL_HOWMANY      3

// Serial1: Hardware UART (D0/D1)
#define SERIAL1_TX          (1u)
#define SERIAL1_RX          (0u)

// Serial2: NINA HCI (D25/D26 with flow control)
#define SERIAL2_TX          (25u)
#define SERIAL2_RX          (26u)
#define SERIAL2_CTS         (27u)
#define SERIAL2_RTS         (28u)

// Serial3: NINA communication (D25/D26)
#define SERIAL3_TX          (25u)
#define SERIAL3_RX          (26u)

// USB CDC
#define SERIAL_CDC          1
#define HAS_UNIQUE_ISERIAL_DESCRIPTOR
#define BOARD_VENDORID      0x2341
#define BOARD_PRODUCTID     0x005E
#define BOARD_NAME          "Arduino Nano RP2040 Connect"

#define USB_MAX_POWER       (500)

// SPI
// ---
#define SPI_HOWMANY         (2)

#define PIN_SPI_MISO  (12u)
#define PIN_SPI_MOSI  (11u)
#define PIN_SPI_SCK   (13u)
#define PIN_SPI_SS    (10u)

static const uint8_t SS   = PIN_SPI_SS;
static const uint8_t MOSI = PIN_SPI_MOSI;
static const uint8_t MISO = PIN_SPI_MISO;
static const uint8_t SCK  = PIN_SPI_SCK;

// SPI1 (NINA WiFi module SPI bus)
#define PIN_SPI1_MISO   (25u)  // GPIO8
#define PIN_SPI1_SS     (26u)  // GPIO9
#define PIN_SPI1_MOSI   (28u)  // GPIO11
#define PIN_SPI1_SCK    (29u)  // GPIO14

// Wire (I2C)
// ----------
#define WIRE_HOWMANY        (1)

#define PIN_WIRE_SDA        (18u)
#define PIN_WIRE_SCL        (19u)

static const uint8_t SDA = PIN_WIRE_SDA;
static const uint8_t SCL = PIN_WIRE_SCL;

// NINA WiFi module pins
// ---------------------
#define NINA_RESETN     (24u)
#define NINA_GPIO0      (20u)

#define SPIWIFI_SS      (26u)
#define SPIWIFI_ACK     (27u)
#define SPIWIFI_RESET   (NINA_RESETN)

// Crypto (ATECC608A on I2C bus)
#define CRYPTO_WIRE     Wire

// NINA WiFi SPI interface
#define SPIWIFI         SPI1

// Serial port definitions
#define SERIAL_PORT_USBVIRTUAL      SerialUSB
#define SERIAL_PORT_MONITOR         SerialUSB
#define SERIAL_PORT_HARDWARE        Serial1
#define SERIAL_PORT_HARDWARE_OPEN   Serial2

// NINA Serial aliases
#define SerialNina      Serial3
#define SerialHCI       Serial2

// Note: nina_pins.h should be included after Arduino.h
// It is automatically included by WiFiNINA.h when needed

#endif /* __PINS_ARDUINO__ */
