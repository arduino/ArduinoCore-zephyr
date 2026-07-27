/*
 * Copyright (c) 2026 KurtE
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
/**
 * Retrive CPU Temperature from zephyr die_temp code,
 */

/**
 * Retrive the CPU dic_temp from zephyr.
 *
 *
 * @param sensor_index - on the chance a processor is defined with more than one
 * @param mask CAN ID mask (0 = accept all, 0x7FF for 11-bit, 0x1FFFFFFF for 29-bit)
 * @return celsius temperature or NAN if error or processor does not support this.
 */
extern double CPUTemperature(uint8_t sensor_index = 0);
