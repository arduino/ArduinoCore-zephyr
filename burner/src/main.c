/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 * SPDX-License-Identifier: Apache-2.0
 *
 * Bootloader updater: uploaded in place of the loader, it rewrites the boot
 * partition with the embedded bootloader if it differs from the flash content.
 */

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/storage/flash_map.h>
#ifdef CONFIG_CPU_CORTEX_M_HAS_DWT
#include <cmsis_core.h>
#endif

/*
 * Not const, so it is copied to RAM at boot: some flash controllers (e.g.
 * Renesas RA code flash) cannot be read while they are being programmed.
 */
static uint8_t bootloader[] = {
#include "bootloader.bin.inc"
};

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

#if defined(CONFIG_SOC_FLASH_STM32) && defined(CONFIG_SOC_SERIES_STM32H7X)
/* Clear stale error flags left by the bootloader, as the loader does */
static int internal_flash_clean_regs(void) {
	FLASH->CCR1 = FLASH_FLAG_ALL_BANK1;
#ifdef DUAL_BANK
	FLASH->CCR2 = FLASH_FLAG_ALL_BANK2;
#endif
	return 0;
}

SYS_INIT(internal_flash_clean_regs, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
#endif

static bool bootloader_matches(const struct flash_area *fa) {
	static uint8_t buf[256];

	for (size_t off = 0; off < sizeof(bootloader); off += sizeof(buf)) {
		size_t len = MIN(sizeof(buf), sizeof(bootloader) - off);

		if (flash_area_read(fa, off, buf, len) != 0 || memcmp(buf, &bootloader[off], len) != 0) {
			return false;
		}
	}
	return true;
}

static int update_bootloader(void) {
	const struct flash_area *fa;
	int rc;

	rc = flash_area_open(PARTITION_ID(boot_partition), &fa);
	if (rc) {
		printk("Failed to open boot partition: %d\n", rc);
		return rc;
	}

	if (sizeof(bootloader) > fa->fa_size) {
		printk("Bootloader (%u bytes) larger than boot partition (%u bytes)\n",
			   (unsigned int)sizeof(bootloader), (unsigned int)fa->fa_size);
		rc = -EFBIG;
		goto out;
	}

	if (bootloader_matches(fa)) {
		printk("Bootloader already up to date\n");
		goto out;
	}

	printk("Updating bootloader (%u bytes)...\n", (unsigned int)sizeof(bootloader));
	rc = flash_area_erase(fa, 0, fa->fa_size);
	if (rc) {
		printk("Erase failed: %d\n", rc);
		goto out;
	}
	rc = flash_area_write(fa, 0, bootloader, sizeof(bootloader));
	if (rc) {
		printk("Write failed: %d\n", rc);
		goto out;
	}
	if (!bootloader_matches(fa)) {
		printk("Verify failed\n");
		rc = -EIO;
		goto out;
	}
	printk("Bootloader updated\n");

out:
	flash_area_close(fa);
	return rc;
}

#ifdef CONFIG_CPU_CORTEX_M_HAS_DWT
/*
 * DWT watchpoints survive a system reset. The loader's null pointer detection
 * watches 0x0-0x3ff, where some boards keep their bootloader: when the burner
 * reads it, a matching watchpoint halts the core if a debugger enabled halting
 * debug since power-up. Disable them before reading the bootloader.
 */
static void dwt_disable_watchpoints(void) {
	volatile uint32_t *function = &DWT->FUNCTION0;
	uint32_t num = (DWT->CTRL & DWT_CTRL_NUMCOMP_Msk) >> DWT_CTRL_NUMCOMP_Pos;

	/* COMPn, MASKn, FUNCTIONn, reserved: 4 words per comparator */
	for (uint32_t i = 0; i < num; i++) {
		function[i * 4] = 0;
	}
}
#endif

int main(void) {
#ifdef CONFIG_CPU_CORTEX_M_HAS_DWT
	dwt_disable_watchpoints();
#endif
	int rc = update_bootloader();

	printk("Done; double-tap RESET and upload a sketch to install the loader\n");

	/* slow blink: success, fast blink: failure */
	k_timeout_t period = rc ? K_MSEC(100) : K_MSEC(1000);

	gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	while (true) {
		gpio_pin_toggle_dt(&led);
		k_sleep(period);
	}
}
