/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cmsis_core.h>
#include <zephyr/init.h>
#include <zephyr/drivers/led.h>

#ifndef CONFIG_CPP
void __cxa_pure_virtual() {
	while (1)
		;
}
#endif

int disable_mpu_rasr_xn(void) {
	uint32_t index;
	/* Kept the max index as 8(irrespective of soc) because the sram
	 * would most likely be set at index 2.
	 */
	for (index = 0U; index < 8; index++) {
		MPU->RNR = index;
#if defined(CONFIG_ARMV8_M_BASELINE) || defined(CONFIG_ARMV8_M_MAINLINE)
		if (MPU->RBAR & MPU_RBAR_XN_Msk) {
			MPU->RBAR ^= MPU_RBAR_XN_Msk;
		}
#else
		if (MPU->RASR & MPU_RASR_XN_Msk) {
			MPU->RASR ^= MPU_RASR_XN_Msk;
		}
#endif /* CONFIG_ARMV8_M_BASELINE || CONFIG_ARMV8_M_MAINLINE */
	}
	return 0;
}

#if defined(CONFIG_BOARD_ARDUINO_NANO_33_BLE)
int disable_bootloader_mpu() {
	// MPU was previously enabled in the bootloader
	// https://github.com/bcmi-labs/zephyr/blob/31cb7dd00fd5bce4c69896b3b2ddf6259d0c0f2b/boards/arm/arduino_nano_33_ble/arduino_nano_33_ble_defconfig#L10C1-L10C15
	__disable_irq();
	disable_mpu_rasr_xn();
	__DMB();
	MPU->CTRL = 0;
	__enable_irq();
	return 0;
}

SYS_INIT(disable_bootloader_mpu, PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
#endif

#if defined(CONFIG_ARM_MPU)
SYS_INIT(disable_mpu_rasr_xn, PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
#endif

#if defined(CONFIG_BOARD_ARDUINO_NANO_CONNECT)
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <pico/bootrom.h>

/*
 * Double-tap reset detection: if the board is reset twice within 500ms,
 * enter USB bootloader (BOOTSEL) mode. This mirrors the original
 * ArduinoCore-mbed NANO_RP2040_CONNECT behavior.
 *
 * A magic token is stored in uninitialized RAM (.noinit), which survives
 * a warm reset but is lost on power cycle. On boot:
 *   - If the token is present: a second reset happened quickly, so we
 *     clear the token and enter USB boot mode (never returns).
 *   - If not: write the token, wait 500ms, then clear it and boot normally.
 */
static const uint32_t magic_token[] = {
	0xf01681de,
	0xbd729b29,
	0xd359be7a,
};

/* Non-static so the variant's _on_1200_bps() can arm the same magic. */
uint32_t magic_location[3] __attribute__((section(".noinit.double_tap")));

#define NANO_RP2040_LED_PIN 6

int double_tap_check(void) {
	if (magic_location[0] == magic_token[0] && magic_location[1] == magic_token[1] &&
		magic_location[2] == magic_token[2]) {
		magic_location[0] = 0;
		reset_usb_boot(1 << NANO_RP2040_LED_PIN, 0);
		/* never returns */
	}

	for (int i = 0; i < 3; i++) {
		magic_location[i] = magic_token[i];
	}

	k_busy_wait(500000);

	magic_location[0] = 0;
	return 0;
}

SYS_INIT(double_tap_check, POST_KERNEL, 0);
#endif

#if defined(CONFIG_INPUT)
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/input/input.h>
typedef void (*zephyr_input_callback_t)(struct input_event *evt, void *user_data);

static zephyr_input_callback_t zephyr_input_cb_func = NULL;
static void *zephyr_input_cb_data = NULL;

void zephyr_input_register_callback(zephyr_input_callback_t cb, void *user_data) {
	zephyr_input_cb_func = cb;
	zephyr_input_cb_data = user_data;
}

static void zephyr_input_callback(struct input_event *evt, void *user_data) {
	if (zephyr_input_cb_func) {
		zephyr_input_cb_func(evt, zephyr_input_cb_data);
	}
}

INPUT_CALLBACK_DEFINE(NULL, zephyr_input_callback, NULL);
#endif

#if defined(CONFIG_SHARED_MULTI_HEAP)
#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/multi_heap/shared_multi_heap.h>

#if defined(CONFIG_VIDEO_BUFFER_POOL_ALLOC_OPS)
#include <zephyr/linker/linker-defs.h>
#include <zephyr/sys/util.h>

int smh_region_video_init(void) {
	uintptr_t ram_free_start = ROUND_UP((uintptr_t)_end, CONFIG_VIDEO_BUFFER_POOL_ALIGN);
	uintptr_t ram_free_end = (uintptr_t)__kernel_ram_end;

	if (ram_free_end <= ram_free_start) {
		return -ENOMEM;
	}

	uintptr_t ram_free_size = ram_free_end - ram_free_start;
	uintptr_t split_size = ROUND_DOWN(ram_free_size / 2U, CONFIG_VIDEO_BUFFER_POOL_ALIGN);

	if (split_size == 0U) {
		return -ENOMEM;
	}

	struct shared_multi_heap_region smh_ram_a = {
		.attr = SMH_REG_ATTR_CACHEABLE,
		.addr = ram_free_start,
		.size = split_size,
	};
	struct shared_multi_heap_region smh_ram_b = {
		.attr = SMH_REG_ATTR_CACHEABLE,
		.addr = ram_free_start + split_size,
		.size = ram_free_end - (ram_free_start + split_size),
	};

	int ret = shared_multi_heap_add(&smh_ram_a, NULL);
	if (ret != 0) {
		return ret;
	}

	if (smh_ram_b.size > 0U) {
		ret = shared_multi_heap_add(&smh_ram_b, NULL);
		if (ret != 0) {
			return ret;
		}
	}

	return 0;
}
#endif

int smh_init(void) {
	int ret = 0;
	ret = shared_multi_heap_pool_init();
	if (ret != 0) {
		return ret;
	}

#if !defined(CONFIG_BOARD_ARDUINO_NICLA_VISION)
	Z_GENERIC_SECTION(SDRAM1) static uint8_t __aligned(32) smh_pool[4 * 1024 * 1024];

	struct shared_multi_heap_region smh_sdram = {
		.addr = (uintptr_t)smh_pool,
		.size = sizeof(smh_pool),
		.attr = SMH_REG_ATTR_EXTERNAL,
	};

	ret = shared_multi_heap_add(&smh_sdram, NULL);
	if (ret != 0) {
		return ret;
	}
#endif
	return 0;
}

SYS_INIT(smh_init, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
#endif

#if defined(CONFIG_BOARD_ARDUINO_PORTENTA_C33) && defined(CONFIG_LLEXT)
#include <zephyr/kernel.h>
#include <zephyr/storage/flash_map.h>

int maybe_flash_bootloader(void) {
	// memcmp the first 256bytes of "embedded bootloader" and address 0x0
	// if they are different, flash the bootloader
	const uint8_t embedded_bootloader[] = {
#include "c33_bl_patch/c33_bl.bin.inc"
	};

	const struct flash_area *fa;
	int rc;

	rc = flash_area_open(FIXED_PARTITION_ID(mcuboot), &fa);
	if (rc) {
		printk("Failed to open flash area, rc %d\n", rc);
		return rc;
	}

	uint8_t flash_bootloader[256];
	flash_area_read(fa, 0, flash_bootloader, 256);

	if (memcmp(embedded_bootloader, flash_bootloader, 256) != 0) {
		// flash the bootloader
		rc = flash_area_erase(fa, 0, fa->fa_size);
		if (rc) {
			printk("Failed to erase flash area, rc %d\n", rc);
			return rc;
		}
		flash_area_write(fa, 0, embedded_bootloader, sizeof(embedded_bootloader));
		if (rc) {
			printk("Failed to write flash area, rc %d\n", rc);
			return rc;
		}
		struct led_dt_spec ledb = LED_DT_SPEC_GET(DT_NODELABEL(led3));
		while (1) {
			led_on_dt(&ledb);
			k_msleep(100);
			led_off_dt(&ledb);
			k_msleep(100);
		}
	}
	return 0;
}

SYS_INIT(maybe_flash_bootloader, POST_KERNEL, CONFIG_FILE_SYSTEM_INIT_PRIORITY);

#endif

#if defined(CONFIG_BOARD_ARDUINO_UNO_Q)
#include "matrix.inc"

#include "../variants/arduino_uno_q_stm32u585xx/variant.h"
#include <stm32_ll_adc.h>
#include <zephyr/devicetree.h>

int analog_reference(uint8_t reference) {
	uint8_t init_status;

	/* VREF+ is connected to VDDA by default */
	const struct gpio_dt_spec spec =
		GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), analog_switch_gpios, 0);

	gpio_pin_configure_dt(&spec, GPIO_OUTPUT);

	__HAL_RCC_SYSCFG_CLK_ENABLE();
	__HAL_RCC_VREF_CLK_ENABLE();

	HAL_SYSCFG_VREFBUF_HighImpedanceConfig(SYSCFG_VREFBUF_HIGH_IMPEDANCE_ENABLE);
	HAL_SYSCFG_DisableVREFBUF();

	if (reference == AR_DEFAULT) {
		/* VREF+ is connected to VDDA */
		gpio_pin_set_dt(&spec, 0);
		return 0;
	}

	gpio_pin_set_dt(&spec, 1);

	if (reference == AR_EXTERNAL) {
		return 0;
	}

	uint32_t voltageScaling = SYSCFG_VREFBUF_VOLTAGE_SCALE3;
	switch (reference) {
	case AR_INTERNAL2V5:
		voltageScaling = SYSCFG_VREFBUF_VOLTAGE_SCALE3;
		break;
	case AR_INTERNAL2V05:
		voltageScaling = SYSCFG_VREFBUF_VOLTAGE_SCALE2;
		break;
	case AR_INTERNAL1V8:
		voltageScaling = SYSCFG_VREFBUF_VOLTAGE_SCALE1;
		break;
	case AR_INTERNAL1V5:
		voltageScaling = SYSCFG_VREFBUF_VOLTAGE_SCALE0;
		break;
	}

	HAL_SYSCFG_VREFBUF_VoltageScalingConfig(voltageScaling);
	init_status = HAL_SYSCFG_EnableVREFBUF();
	HAL_SYSCFG_VREFBUF_HighImpedanceConfig(SYSCFG_VREFBUF_HIGH_IMPEDANCE_DISABLE);

	__ASSERT(init_status == HAL_OK, "ADC Conversion value may be incorrect");

	return init_status;
}

EXPORT_SYMBOL(analog_reference);

int disable_vrefbuf() {
	// This is the safe HW configuration
	return analog_reference(AR_DEFAULT);
}

SYS_INIT(disable_vrefbuf, POST_KERNEL, 0);
#endif
