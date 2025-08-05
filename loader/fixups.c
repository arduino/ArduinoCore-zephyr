#include <cmsis_core.h>
#include <zephyr/init.h>
int disable_mpu_rasr_xn(void)
{
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

#if defined(CONFIG_SOC_STM32H747XX_M7)
void enable_bkp_access(void)
{
	/* Enable access to the backup domain */
	// HAL_PWR_EnableBkUpAccess();
	SET_BIT(PWR->CR1, PWR_CR1_DBP);
}
SYS_INIT(enable_bkp_access, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
#endif

#if defined(CONFIG_BOARD_ARDUINO_GIGA_R1) && defined(CONFIG_VIDEO)
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/logging/log.h>

int camera_ext_clock_enable(void)
{
	int ret;
	uint32_t rate;
	const struct device *cam_ext_clk_dev = DEVICE_DT_GET(DT_NODELABEL(pwmclock));

	if (!device_is_ready(cam_ext_clk_dev)) {
		return -ENODEV;
	}

	ret = clock_control_on(cam_ext_clk_dev, (clock_control_subsys_t)0);
	if (ret < 0) {
		return ret;
	}

	ret = clock_control_get_rate(cam_ext_clk_dev, (clock_control_subsys_t)0, &rate);
	if (ret < 0) {
		return ret;
	}

	return 0;
}

SYS_INIT(camera_ext_clock_enable, POST_KERNEL, CONFIG_CLOCK_CONTROL_PWM_INIT_PRIORITY);

#endif

#if defined(CONFIG_SHARED_MULTI_HEAP)
#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/multi_heap/shared_multi_heap.h>

__stm32_sdram1_section static uint8_t __aligned(32) smh_pool[4*1024*1024];

int smh_init(void) {
    int ret = 0;
    ret = shared_multi_heap_pool_init();
    if (ret != 0) {
        return ret;
    }

    struct shared_multi_heap_region smh_sdram = {
        .addr = (uintptr_t) smh_pool,
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

int maybe_flash_bootloader(void)
{
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
