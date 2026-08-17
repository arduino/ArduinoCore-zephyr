/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ARDUINO_ZEPHYR_PDMCONFIG_H
#define ARDUINO_ZEPHYR_PDMCONFIG_H

/* The number of samples the user receive
 * For performance reason the user is strongly suggested to use this
 * dimension for its application buffer that take the audio samples */
#ifndef PDM_NUMBER_OF_SAMPLES
#if defined(ARDUINO_NANO_RP2040_CONNECT)
/* The RP2040 PIO driver decimates a fixed 512 byte raw PDM capture buffer,
 * so the block size is dictated by the driver and not freely selectable:
 * 512 raw bytes / (decimation / 8) samples. At 16 kHz the driver picks a
 * decimation of 64, which yields 64 samples per block. */
#define PDM_NUMBER_OF_SAMPLES 64
#else
#define PDM_NUMBER_OF_SAMPLES 512
#endif
#endif

/* size in bit of an audio sample */
/* NANO 33 BLE will work only if this value is 16
 * GIGA can work up with 24 */
#ifndef PDM_SAMPLE_BIT_WIDTH
#define PDM_SAMPLE_BIT_WIDTH 16
#endif

/* receiving thread configuration */
#ifndef PDM_THREAD_STACK_SIZE
#define PDM_THREAD_STACK_SIZE 1024
#endif
#ifndef PDM_THREAD_PRIORITY
#define PDM_THREAD_PRIORITY 7
#endif

/* memory slab configuration */
#if defined(ARDUINO_NANO33BLE)
#define SLAB_BLOCK_NUM 4
#define SLAB_ALIGN     4
#if PDM_SAMPLE_BIT_WIDTH != 16
#error "Compilation runtime check: PDM_SAMPLE_BIT_WIDTH must be set to 16 for ARDUINO_NANO33BLE"
#endif
#define SLAB_BLOCK_SIZE (PDM_NUMBER_OF_SAMPLES * 2)
#elif defined(ARDUINO_GIGA)
#define SLAB_BLOCK_NUM 4
#define SLAB_ALIGN     32
#if PDM_SAMPLE_BIT_WIDTH == 16
#define SLAB_BLOCK_SIZE (PDM_NUMBER_OF_SAMPLES * 2)
#elif PDM_SAMPLE_BIT_WIDTH == 24
#define SLAB_BLOCK_SIZE (PDM_NUMBER_OF_SAMPLES * 4)
#else
#error "Compilation runtime check: PDM_SAMPLE_BIT_WIDTH must be 16 or 24 for ARDUINO_GIGA"
#endif
#elif defined(ARDUINO_NANO_RP2040_CONNECT)
#define SLAB_BLOCK_NUM 8
#define SLAB_ALIGN     4
#if PDM_SAMPLE_BIT_WIDTH != 16
#error                                                                                             \
	"Compilation runtime check: PDM_SAMPLE_BIT_WIDTH must be set to 16 for ARDUINO_NANO_RP2040_CONNECT"
#endif
#if PDM_NUMBER_OF_SAMPLES != 64
#error "Compilation runtime check: PDM_NUMBER_OF_SAMPLES must be 64 for ARDUINO_NANO_RP2040_CONNECT"
#endif
#define SLAB_BLOCK_SIZE (PDM_NUMBER_OF_SAMPLES * 2)
#endif

#endif // ARDUINO_ZEPHYR_PDMCONFIG_H
