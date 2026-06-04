/*
 * Copyright (c) 2022 Dhruva Gole
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#if !ARDUINO_LIBRARY_DISCOVERY_PHASE
#if __has_include("Arduino_RouterBridge.h")
#if __has_include("routerbridge_provides_serial.h")
#define ARDUINO_ROUTERBRIDGE_PROVIDES_SERIAL
#else
#error "Please update the Arduino_RouterBridge library to the latest version to ensure Serial support on this board."
#endif
#endif
#endif

#define LOADER_PROVIDES_EXTRA_USB_CLASSES

// TODO: correctly handle these legacy defines
#define MOSI    0
#define MISO    0
#define SCK     0
#define SS      0
#define SDA     0
#define SCL     0

#define ST_VREF_MASK        0x80

#define AR_DEFAULT          0
#define AR_INTERNAL1V8      (SYSCFG_VREFBUF_VOLTAGE_SCALE2 | ST_VREF_MASK)
#define AR_INTERNAL2V5      (SYSCFG_VREFBUF_VOLTAGE_SCALE0 | ST_VREF_MASK)
#define AR_INTERNAL2V05     (SYSCFG_VREFBUF_VOLTAGE_SCALE1 | ST_VREF_MASK)
#define AR_EXTERNAL         5
#define AR_INTERNAL         AR_INTERNAL2V5

// RGB LEDs Pin Map
#define LED1_R DIGITAL_PIN_GPIOS_FIND_NODE(DT_NODELABEL(red_led_0))
#define LED1_G DIGITAL_PIN_GPIOS_FIND_NODE(DT_NODELABEL(green_led_0))
#define LED1_B DIGITAL_PIN_GPIOS_FIND_NODE(DT_NODELABEL(blue_led_0))

#define LED2_R DIGITAL_PIN_GPIOS_FIND_NODE(DT_NODELABEL(red_led_1))
#define LED2_G DIGITAL_PIN_GPIOS_FIND_NODE(DT_NODELABEL(green_led_1))
#define LED2_B DIGITAL_PIN_GPIOS_FIND_NODE(DT_NODELABEL(blue_led_1))

#define LED3_R DIGITAL_PIN_GPIOS_FIND_NODE(DT_NODELABEL(red_led_2))
#define LED3_G DIGITAL_PIN_GPIOS_FIND_NODE(DT_NODELABEL(green_led_2))
#define LED3_B DIGITAL_PIN_GPIOS_FIND_NODE(DT_NODELABEL(blue_led_2))

#define LED4_R DIGITAL_PIN_GPIOS_FIND_NODE(DT_NODELABEL(red_led_3))
#define LED4_G DIGITAL_PIN_GPIOS_FIND_NODE(DT_NODELABEL(green_led_3))
#define LED4_B DIGITAL_PIN_GPIOS_FIND_NODE(DT_NODELABEL(blue_led_3))