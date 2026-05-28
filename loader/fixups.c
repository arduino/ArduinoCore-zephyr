/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cmsis_core.h>
#include <zephyr/init.h>

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

Z_GENERIC_SECTION(SDRAM1) static uint8_t __aligned(32) smh_pool[4 * 1024 * 1024];

int smh_init(void) {
	int ret = 0;
	ret = shared_multi_heap_pool_init();
	if (ret != 0) {
		return ret;
	}

	struct shared_multi_heap_region smh_sdram = {
		.addr = (uintptr_t)smh_pool,
		.size = sizeof(smh_pool),
		.attr = SMH_REG_ATTR_EXTERNAL,
	};

	ret = shared_multi_heap_add(&smh_sdram, NULL);
	if (ret != 0) {
		return ret;
	}
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
	}
	return 0;
}

SYS_INIT(maybe_flash_bootloader, POST_KERNEL, CONFIG_FILE_SYSTEM_INIT_PRIORITY);

#endif

#if defined(CONFIG_BOARD_ARDUINO_UNO_Q) || defined(CONFIG_BOARD_ARDUINO_VENTUNO_Q)
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

	#if defined(__HAL_RCC_SYSCFG_CLK_ENABLE)
	__HAL_RCC_SYSCFG_CLK_ENABLE();
	#endif
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

	// TODO: change the defines so in variant.h we can use the ST definition
	// Doesn't work for UNOQ at the moment ¯\_(ツ)_/¯

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

#if defined(CONFIG_BOARD_ARDUINO_VENTUNO_Q) || defined(CONFIG_BOARD_ARDUINO_UNO_Q)
struct backup_store {
	uint32_t wait_for_app_magic;
	uint32_t magic;
	uint8_t fan_control_buffer[256];
	uint8_t leds_control_buffer[256];
};
volatile __stm32_backup_sram_section struct backup_store backup;
#endif

#if defined(CONFIG_BOARD_ARDUINO_VENTUNO_Q)

#include <cannectivity/usb/class/gs_usb.h>

int cannectivity_led_init(void);
int cannectivity_timestamp_init(void);

static const struct gs_usb_ops gs_usb_ops = {
#ifdef CONFIG_CANNECTIVITY_TIMESTAMP
	.timestamp = cannectivity_timestamp_get,
#endif
#ifdef CONFIG_CANNECTIVITY_LED
	.event = cannectivity_led_event,
#endif
};

#define CANNECTIVITY_DT_NODE_ID DT_NODELABEL(cannectivity)
#define CANNECTIVITY_DT_HAS_CHANNEL DT_HAS_COMPAT_STATUS_OKAY(cannectivity_channel)
#define CANNECTIVITY_DT_FOREACH_CHANNEL_SEP(fn, sep)                                               \
	DT_FOREACH_CHILD_STATUS_OKAY_SEP(CANNECTIVITY_DT_NODE_ID, fn, sep)
#define CHANNEL_CAN_CONTROLLER_DT_GET(node_id) DEVICE_DT_GET(DT_PHANDLE(node_id, can_controller))

int enable_cannectivity() {

	const struct device *gs_usb = DEVICE_DT_GET(DT_NODELABEL(gs_usb0));
	const struct device *channels[] = {
		CANNECTIVITY_DT_FOREACH_CHANNEL_SEP(CHANNEL_CAN_CONTROLLER_DT_GET, (,))
	};
	int err;

	if (IS_ENABLED(CONFIG_CANNECTIVITY_LED)) {
		err = cannectivity_led_init();
		if (err != 0) {
			return -1;
		}
	}

	if (IS_ENABLED(CONFIG_CANNECTIVITY_TIMESTAMP)) {
		err = cannectivity_timestamp_init();
		if (err != 0) {
			return -1;
		}
	}

	err = gs_usb_register(gs_usb, channels, ARRAY_SIZE(channels), &gs_usb_ops, NULL);
	if (err != 0U) {
		return -1;
	}
	return 0;
}

SYS_INIT(enable_cannectivity, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

#include <zephyr/drivers/hwinfo.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/i2c/target/eeprom.h>

static const struct device *fan_eeprom = DEVICE_DT_GET(DT_NODELABEL(fan_control));
static const struct device *gpio_eeprom = DEVICE_DT_GET(DT_NODELABEL(gpio_control));
static const struct device *fan_pwm = DEVICE_DT_GET(DT_NODELABEL(pwm16));
static const struct device *fan_tach = DEVICE_DT_GET(DT_NODELABEL(pwm14));

static void on_fan_changed(const struct device *dev, void *user_data)
{
	size_t size = eeprom_target_get_size(dev);
	/* Read all eeprom memory and backup it */
	eeprom_target_read_data(dev, 0, backup.fan_control_buffer, size);
	/* Read fan speed and set PWM value */
	const uint8_t data = backup.fan_control_buffer[0x30];
	pwm_set(fan_pwm, 1, PWM_USEC(2550), PWM_USEC(data * 10), PWM_POLARITY_NORMAL);
}

static const struct gpio_dt_spec led0g =
		GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), builtin_led_gpios, 0);
static const struct gpio_dt_spec led0b =
		GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), builtin_led_gpios, 1);
static const struct gpio_dt_spec led0r =
		GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), builtin_led_gpios, 2);
static const struct gpio_dt_spec led1g =
		GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), builtin_led_gpios, 3);
static const struct gpio_dt_spec led1b =
		GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), builtin_led_gpios, 4);
static const struct gpio_dt_spec led1r =
		GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), builtin_led_gpios, 5);
static const struct gpio_dt_spec led2g =
		GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), builtin_led_gpios, 6);
static const struct gpio_dt_spec led2b =
		GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), builtin_led_gpios, 7);
static const struct gpio_dt_spec led2r =
		GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), builtin_led_gpios, 8);
static const struct gpio_dt_spec led3g =
		GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), builtin_led_gpios, 9);
static const struct gpio_dt_spec led3b =
		GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), builtin_led_gpios, 10);
static const struct gpio_dt_spec led3r =
		GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), builtin_led_gpios, 11);

// Driver expects order in RGB
static const struct gpio_dt_spec *leds[] = {&led0r, &led0g, &led0b, &led1r, &led1g, &led1b,
										&led2r, &led2g, &led2b, &led3r, &led3g, &led3b};

void configure_leds(const uint8_t *leds_control_buffer) {
	for (size_t i = 0; i < sizeof(leds)/sizeof(leds[0]); i++) {
		const struct gpio_dt_spec *led = leds[i];
		uint8_t reg = 0x14 + (i/4);
		uint8_t mask = 0x3 << ((i % 4)*2);
		gpio_pin_configure_dt(led, GPIO_OUTPUT);
		gpio_pin_set_dt(led, (backup.leds_control_buffer[reg] & mask) == 0 ? 0 : 1);
	}
}

static void on_gpio_changed(const struct device *dev, void *user_data)
{
	size_t size = eeprom_target_get_size(dev);
	eeprom_target_read_data(dev, 0, backup.leds_control_buffer, size);
	// emulate PCA9635
	configure_leds(backup.leds_control_buffer);
}

static volatile uint32_t _period_cycles = 0;
void tacho_capture_callback(const struct device *dev,
					uint32_t pwm,
					uint32_t period_cycles,
					uint32_t pulse_cycles,
					int status,
					void *user_data) {
	_period_cycles = period_cycles;
}

void tacho_thd(void *arg1, void *arg2, void *arg3) {
	while (1) {
		uint8_t data[0x40];
		eeprom_target_read_data(fan_eeprom, 0, data, sizeof(data));
		data[0x3e] = (_period_cycles >> 8) & 0xFF; //Fan 1 tach msb
		data[0x3f] = _period_cycles & 0xFF; //Fan 1 tach lsb
		eeprom_target_write_data(fan_eeprom, 0, data, sizeof(data));
		k_sleep(K_SECONDS(1));
		_period_cycles = 0;
		k_sleep(K_SECONDS(1));
	}
}
K_THREAD_DEFINE(tacho, 512, tacho_thd, NULL, NULL, NULL, 5, 0, 1000);

const struct gpio_dt_spec power_button =
	GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), power_button_gpios, 0);

const struct gpio_dt_spec power_button_follower =
	GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), power_button_gpios, 1);

const struct gpio_dt_spec force_reboot =
	GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), power_button_gpios, 2);


void power_button_work_handler(struct k_work *work) {
	gpio_pin_set_dt(&led3r, 1);
	// If still pressed, force reboot, otherwise just set the follower pin to 1
	if (gpio_pin_get_dt(&power_button) == 0) {
		gpio_pin_configure_dt(&force_reboot, GPIO_INPUT | GPIO_PULL_UP);
		while (1);
	}
}

K_WORK_DELAYABLE_DEFINE(power_button_work, power_button_work_handler);

void button_irq(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
	int status = gpio_pin_get_dt(&power_button);
	gpio_pin_set_dt(&power_button_follower, !status);

	if (status) {
		k_work_cancel_delayable(&power_button_work);
	} else {
		k_work_schedule(&power_button_work, K_SECONDS(10));
	}
}

static struct gpio_callback button_cb_data;

int system_utilities(void) {

	/* Linux Ready GPIO input */
	const struct gpio_dt_spec spec =
			GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), control_gpios, 0);
	gpio_pin_configure_dt(&spec, GPIO_INPUT | GPIO_PULL_DOWN);

	const struct gpio_dt_spec EDL_mode =
			GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), control_gpios, 1);
	gpio_pin_configure_dt(&EDL_mode, GPIO_INPUT);

	const struct gpio_dt_spec fault_1v8 =
			GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), control_gpios, 2);
	gpio_pin_configure_dt(&fault_1v8, GPIO_INPUT | GPIO_PULL_UP);
	if (gpio_pin_get_dt(&fault_1v8) == 0) {
		k_sleep(K_MSEC(300));
		if (gpio_pin_get_dt(&fault_1v8) == 0) {
			gpio_pin_configure_dt(&led0r, GPIO_OUTPUT);
			gpio_pin_configure_dt(&led1r, GPIO_OUTPUT);
			while (1) {
				gpio_pin_toggle_dt(&led0r);
				gpio_pin_toggle_dt(&led1r);
				k_sleep(K_MSEC(200));
			}
		}
	}

	gpio_pin_configure_dt(&power_button, GPIO_INPUT | GPIO_PULL_UP);
	gpio_init_callback(&button_cb_data, button_irq, BIT(power_button.pin));
	gpio_add_callback(power_button.port, &button_cb_data);
	gpio_pin_interrupt_configure_dt(&power_button, GPIO_INT_EDGE_BOTH);
	gpio_pin_configure_dt(&power_button_follower, GPIO_OUTPUT | GPIO_PULL_DOWN);
	gpio_pin_configure_dt(&force_reboot, GPIO_OUTPUT | GPIO_PULL_DOWN);

	/* Backup memory */
	const struct device *const backup_memory = DEVICE_DT_GET_ONE(st_stm32_backup_sram);
	if (!device_is_ready(backup_memory)) {
		printk("ERROR: BackUp SRAM device is not ready\n");
		return 0;
	}

	/* Initializize controllers values to default at power ON */
	uint32_t reset_cause_id = 0;
	hwinfo_get_reset_cause(&reset_cause_id);
	if (reset_cause_id == RESET_POR || backup.magic != 0x67F44F76) {
		printk("Reset EEPROM memory to default values\n");
		memset(backup.leds_control_buffer, 0x0, sizeof(backup.leds_control_buffer));
		memset(backup.fan_control_buffer, 0xFF, sizeof(backup.fan_control_buffer));
		backup.magic = 0x67F44F76;
		backup.fan_control_buffer[0x27] = 0x00; //Drive fail
		backup.fan_control_buffer[0x30] = 0x66; //Fan 1 drive
		backup.fan_control_buffer[0x38] = 0x66; //Fan 1 min drive
		backup.fan_control_buffer[0x3E] = 0xFF; //Fan 1 tach msb
		backup.fan_control_buffer[0x3F] = 0xF8; //Fan 1 tach lsb
		backup.fan_control_buffer[0x40] = 0x00; //Fan 2 drive
		backup.fan_control_buffer[0x48] = 0x66; //Fan 2 min drive
		backup.fan_control_buffer[0x4E] = 0xFF; //Fan 2 tach msb
		backup.fan_control_buffer[0x4F] = 0xF8; //Fan 2 tach lsb
		backup.fan_control_buffer[0x50] = 0x00; //Fan 3 drive
		backup.fan_control_buffer[0x58] = 0x66; //Fan 3 min drive
		backup.fan_control_buffer[0x5E] = 0xFF; //Fan 3 tach msb
		backup.fan_control_buffer[0x5F] = 0xF8; //Fan 3 tach lsb
		backup.fan_control_buffer[0x60] = 0x00; //Fan 4 drive
		backup.fan_control_buffer[0x68] = 0x66; //Fan 4 min drive
		backup.fan_control_buffer[0x6E] = 0xFF; //Fan 4 tach msb
		backup.fan_control_buffer[0x6F] = 0xF8; //Fan 4 tach lsb
		backup.fan_control_buffer[0x70] = 0x00; //Fan 5 drive
		backup.fan_control_buffer[0x78] = 0x66; //Fan 5 min drive
		backup.fan_control_buffer[0x7E] = 0xFF; //Fan 5 tach msb
		backup.fan_control_buffer[0x7F] = 0xF8; //Fan 5 tach lsb
	}
	backup.fan_control_buffer[0xFD] = 0x34; //Product
	backup.fan_control_buffer[0xFE] = 0x5D; //Vendor

	/* Fan PWM out configuration */
	if (!device_is_ready(fan_pwm)) {
		printk("Error: PWM device is not ready\n");
		return 0;
	}
	const uint8_t data = 254; //backup.fan_control_buffer[0x30];
	pwm_set(fan_pwm, 1, PWM_USEC(2550), PWM_USEC(data * 10), PWM_POLARITY_NORMAL);

	/* TODO Fan TACH input configuration */
	if (!device_is_ready(fan_tach)) {
		printk("device is not ready\n");
		return 0;
	}

	/* Fan controller EEPROM driver configuration */
    if (!device_is_ready(fan_eeprom)) {
		printk("fan eeprom device not ready\n");
		return 0;
	}
	eeprom_target_set_changed_callback(fan_eeprom, on_fan_changed, NULL);

	if (i2c_target_driver_register(fan_eeprom) < 0) {
		printk("Failed to register fan i2c eeprom target driver\n");
		return 0;
	}
	printk("fan eeprom i2c target driver registered\n");

	unsigned int size = eeprom_target_get_size(fan_eeprom);
	eeprom_target_write_data(fan_eeprom, 0, backup.fan_control_buffer, size);
	printk("fan eeprom i2c target driver default values set\n");

	/* GPIO expander EEPROM driver configuration */
	if (!device_is_ready(gpio_eeprom)) {
		printk("gpio eeprom device not ready\n");
		return 0;
	}
	eeprom_target_set_changed_callback(gpio_eeprom, on_gpio_changed, NULL);

	if (i2c_target_driver_register(gpio_eeprom) < 0) {
		printk("Failed to register gpio i2c eeprom target driver\n");
		return 0;
	}

	size = eeprom_target_get_size(gpio_eeprom);
	eeprom_target_write_data(gpio_eeprom, 0, backup.leds_control_buffer, size);
	printk("gpio eeprom i2c target driver registered\n");

	/* Restore LEDs values saved in backup RAM */
	configure_leds(backup.leds_control_buffer);

	pwm_configure_capture(fan_tach, 1,
				    PWM_POLARITY_NORMAL |
				    PWM_CAPTURE_MODE_CONTINUOUS |
				    PWM_CAPTURE_TYPE_PULSE | PWM_CAPTURE_TYPE_PERIOD,
				    tacho_capture_callback, NULL);

	pwm_enable_capture(fan_tach, 1);
}
SYS_INIT(system_utilities, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

#endif
