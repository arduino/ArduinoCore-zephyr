/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h>
#include <zephyr/drivers/counter.h>
#include <string.h>

#if defined(CONFIG_BOARD_ARDUINO_UNO_Q)

static const uint8_t pins[][2] = {
    { 0, 1 }, // 0
    { 1, 0 },
    { 0, 2 },
    { 2, 0 },
    { 1, 2 },
    { 2, 1 },
    { 0, 3 },
    { 3, 0 },
    { 1, 3 },
    { 3, 1 },
    { 2, 3 }, // 10
    { 3, 2 },
    { 0, 4 },
    { 4, 0 },
    { 1, 4 },
    { 4, 1 },
    { 2, 4 },
    { 4, 2 },
    { 3, 4 },
    { 4, 3 },
    { 0, 5 }, // 20
    { 5, 0 },
    { 1, 5 },
    { 5, 1 },
    { 2, 5 },
    { 5, 2 },
    { 3, 5 },
    { 5, 3 },
    { 4, 5 },
    { 5, 4 },
    { 0, 6 }, // 30
    { 6, 0 },
    { 1, 6 },
    { 6, 1 },
    { 2, 6 },
    { 6, 2 },
    { 3, 6 },
    { 6, 3 },
    { 4, 6 },
    { 6, 4 },
    { 5, 6 }, // 40
    { 6, 5 },
    { 0, 7 },
    { 7, 0 },
    { 1, 7 },
    { 7, 1 },
    { 2, 7 },
    { 7, 2 },
    { 3, 7 },
    { 7, 3 },
    { 4, 7 }, // 50
    { 7, 4 },
    { 5, 7 },
    { 7, 5 },
    { 6, 7 },
    { 7, 6 },
    { 0, 8 },
    { 8, 0 },
    { 1, 8 },
    { 8, 1 },
    { 2, 8 }, // 60
    { 8, 2 },
    { 3, 8 },
    { 8, 3 },
    { 4, 8 },
    { 8, 4 },
    { 5, 8 },
    { 8, 5 },
    { 6, 8 },
    { 8, 6 },
    { 7, 8 }, // 70
    { 8, 7 },
    { 0, 9 },
    { 9, 0 },
    { 1, 9 },
    { 9, 1 },
    { 2, 9 },
    { 9, 2 },
    { 3, 9 },
    { 9, 3 },
    { 4, 9 }, // 80
    { 9, 4 },
    { 5, 9 },
    { 9, 5 },
    { 6, 9 },
    { 9, 6 },
    { 7, 9 },
    { 9, 7 },
    { 8, 9 },
    { 9, 8 },
    { 0, 10 }, // 90
    { 10, 0 },
    { 1, 10 },
    { 10, 1 },
    { 2, 10 },
    { 10, 2 },
    { 3, 10 },
    { 10, 3 },
    { 4, 10 },
    { 10, 4 },
    { 5, 10 }, // 100
    { 10, 5 },
    { 6, 10 },
    { 10, 6 },
  };

static void turnLed(int idx, bool on) {
    GPIOF->MODER &= 0xF0000000U;
    if (on) {
        uint8_t pin0 = pins[idx][0];
        uint8_t pin1 = pins[idx][1];
        
        GPIOF->BSRR |= (1U << pin0) | (1U << (pin1 + 16));
        GPIOF->MODER |= (1U << (pin0 << 1)) | (1U << (pin1 << 1));
    }
}

#endif

#if defined(CONFIG_BOARD_ARDUINO_VENTUNO_Q)

static const uint8_t pins[][2] = {
    { 2, 3 }, // 0
    { 3, 2 },
    { 2, 4 },
    { 4, 2 },
    { 3, 4 },
    { 4, 3 },
    { 2, 5 },
    { 5, 2 },
    { 3, 5 },
    { 5, 3 },
    { 4, 5 }, // 10
    { 5, 4 },
    { 2, 6 },
    { 6, 2 },
    { 3, 6 },
    { 6, 3 },
    { 4, 6 },
    { 6, 4 },
    { 5, 6 },
    { 6, 5 },
    { 2, 7 }, // 20
    { 7, 2 },
    { 3, 7 },
    { 7, 3 },
    { 4, 7 },
    { 7, 4 },
    { 5, 7 },
    { 7, 5 },
    { 6, 7 },
    { 7, 6 },
    { 2, 9 }, // 30
    { 9, 2 },
    { 3, 9 },
    { 9, 3 },
    { 4, 9 },
    { 9, 4 },
    { 5, 9 },
    { 9, 5 },
    { 6, 9 },
    { 9, 6 },
    { 7, 9 }, // 40
    { 9, 7 },
    { 2, 10 },
    { 10, 2 },
    { 3, 10 },
    { 10, 3 },
    { 4, 10 },
    { 10, 4 },
    { 5, 10 },
    { 10, 5 },
    { 6, 10 }, // 50
    { 10, 6 },
    { 7, 10 },
    { 10, 7 },
    { 9, 10 },
    { 10, 9 },
    { 2, 13 },
    { 13, 2 },
    { 3, 13 },
    { 13, 3 },
    { 4, 13 }, // 60
    { 13, 4 },
    { 5, 13 },
    { 13, 5 },
    { 6, 13 },
    { 13, 6 },
    { 7, 13 },
    { 13, 7 },
    { 9, 13 },
    { 13, 9 },
    { 10, 13 }, // 70
    { 13, 10 },
    { 2, 14 },
    { 14, 2 },
    { 3, 14 },
    { 14, 3 },
    { 4, 14 },
    { 14, 4 },
    { 5, 14 },
    { 14, 5 },
    { 6, 14 }, // 80
    { 14, 6 },
    { 7, 14 },
    { 14, 7 },
    { 9, 14 },
    { 14, 9 },
    { 10, 14 },
    { 14, 10 },
    { 13, 14 },
    { 14, 13 },
    { 2, 15 }, // 90
    { 15, 2 },
    { 3, 15 },
    { 15, 3 },
    { 4, 15 },
    { 15, 4 },
    { 5, 15 },
    { 15, 5 },
    { 6, 15 },
    { 15, 6 },
    { 7, 15 }, // 100
    { 15, 7 },
    { 9, 15 },
    { 15, 9 },
  };

static void turnLed(int idx, bool on) {
    GPIOH->MODER &= 0x03C3000FU;               // don't touch pins 0, 1, 8, 11, 12
    if (on) {
        uint8_t pin0 = pins[idx][0];
        uint8_t pin1 = pins[idx][1];

        GPIOH->BSRR |= (1U << pin0) | (1U << (pin1 + 16));
        GPIOH->MODER |= (1U << (pin0 << 1)) | (1U << (pin1 << 1));
    }
}

#endif

#define NUM_MATRIX_LEDS 104
static uint8_t __attribute__((aligned)) framebuffer[NUM_MATRIX_LEDS / 8];
static uint8_t __attribute__((aligned)) framebuffer_color[NUM_MATRIX_LEDS];

static bool color = false;
static uint8_t _max_grayscale_bits = 3;

static void timer_irq_handler_fn(const struct device *counter_dev, void *user_data) {
    static volatile int i_isr = 0;
    if (color) {
        static volatile int counter = 0;
        switch ((framebuffer_color[i_isr] * 8 / (1 << _max_grayscale_bits))) {
            case 0:
                turnLed(i_isr, false);
                break;
            case 1:
                turnLed(i_isr, counter % 23 == 0);
                break;
            case 2:
                turnLed(i_isr, counter % 15 == 0);
                break;
            case 3:
                turnLed(i_isr, counter % 5 == 0);
                break;
            case 4:
                turnLed(i_isr, counter % 3 == 0);
                break;
            case 5:
            case 6:
            case 7:
                turnLed(i_isr, true);
                break;
        }
        counter++;
    } else {
        turnLed(i_isr, ((framebuffer[i_isr >> 3] & (1 << (i_isr % 8))) != 0));
    }
    i_isr = (i_isr + 1) % NUM_MATRIX_LEDS;
}

void matrixWrite(const uint32_t* buf) {
    memcpy(framebuffer, buf, NUM_MATRIX_LEDS/8);
    color = false;
}

void matrixGrayscaleWrite(const uint8_t* buf) {
    memcpy(framebuffer_color, buf, NUM_MATRIX_LEDS);
    color = true;
}

void matrixSetGrayscaleBits(uint8_t _max) {
    _max_grayscale_bits = _max;
}

#define TIMER DT_NODELABEL(counter_matrix)

void matrixBegin() {
	const struct device *const counter_dev = DEVICE_DT_GET(TIMER);
    counter_start(counter_dev);

    struct counter_top_cfg top_cfg;
	top_cfg.ticks = counter_us_to_ticks(counter_dev, 10);
	top_cfg.callback = timer_irq_handler_fn;
	top_cfg.user_data = &top_cfg;
    top_cfg.flags = 0;

	int err = counter_set_top_value(counter_dev, &top_cfg);
    if (err) {
        printk("Failed to set counter_set_top_value");
    }
}

void matrixEnd() {
    const struct device *const counter_dev = DEVICE_DT_GET(TIMER);
    counter_stop(counter_dev);
}


void matrixPlay(const uint8_t* buf, uint32_t len) {
    int i = 0;
    while (i < (len / 104)) {
		matrixGrayscaleWrite(&buf[i*104]);
		i++;
		k_msleep(16);
    }
}
