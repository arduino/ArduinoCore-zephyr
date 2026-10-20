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

static const uint8_t bootloader[] = {
#include "bootloader.bin.inc"
};

#if DT_NODE_EXISTS(DT_ALIAS(led0))
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
#endif

#if defined(CONFIG_SOC_FLASH_STM32) && defined(CONFIG_SOC_SERIES_STM32H7X)
/* Clear stale error flags left by the bootloader, as the loader does */
static int internal_flash_clean_regs(void)
{
	FLASH->CCR1 = FLASH_FLAG_ALL_BANK1;
#ifdef DUAL_BANK
	FLASH->CCR2 = FLASH_FLAG_ALL_BANK2;
#endif
	return 0;
}

SYS_INIT(internal_flash_clean_regs, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
#endif

static bool bootloader_matches(const struct flash_area *fa)
{
	static uint8_t buf[256];

	for (size_t off = 0; off < sizeof(bootloader); off += sizeof(buf)) {
		size_t len = MIN(sizeof(buf), sizeof(bootloader) - off);

		if (flash_area_read(fa, off, buf, len) != 0 ||
		    memcmp(buf, &bootloader[off], len) != 0) {
			return false;
		}
	}
	return true;
}

static int update_bootloader(void)
{
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

int main(void)
{
	int rc = update_bootloader();

	printk("Done; double-tap RESET and upload a sketch to install the loader\n");

#if DT_NODE_EXISTS(DT_ALIAS(led0))
	/* slow blink: success, fast blink: failure */
	k_timeout_t period = rc ? K_MSEC(100) : K_MSEC(1000);

	gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	while (true) {
		gpio_pin_toggle_dt(&led);
		k_sleep(period);
	}
#endif
	return rc;
}
