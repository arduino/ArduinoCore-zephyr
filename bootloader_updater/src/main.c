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

#if defined(CONFIG_SOC_FAMILY_MICROCHIP_PIC32CK_SG_GC)
#include <zephyr/drivers/flash.h>
#include <zephyr/drivers/flash/mchp_nvmctrl_g3.h>
#include <soc.h>
#endif

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

static int rewrite_bootloader(const struct flash_area *fa)
{
	int rc;

#if defined(CONFIG_SOC_FAMILY_MICROCHIP_PIC32CK_SG_GC)
	uint32_t protection[] = {FCW_REGS->FCW_LBWP, FCW_REGS->FCW_UBWP};
	struct bfm_exop wp = {.wp_page = FCW_LBWP_LBWP_Msk};

	/* A local lock cannot be cleared until reset. Check before erasing anything. */
	if ((protection[0] & FCW_LBWP_LBWPLOCK_Msk) != 0 ||
	    (protection[1] & FCW_UBWP_UBWPLOCK_Msk) != 0) {
		return -EACCES;
	}

	for (size_t bank = 0; bank < ARRAY_SIZE(protection); bank++) {
		wp.boot_bank = bank == 0 ? BOOT_FLASH_BANK_1 : BOOT_FLASH_BANK_2;
		rc = flash_ex_op(fa->fa_dev, FLASH_EX_OP_BFM_WRITE_PROTECT_DISABLE,
				 (uintptr_t)&wp, NULL);
		if (rc != 0) {
			goto out;
		}
	}
	if ((FCW_REGS->FCW_LBWP & FCW_LBWP_LBWP_Msk) != 0 ||
	    (FCW_REGS->FCW_UBWP & FCW_UBWP_UBWP_Msk) != 0) {
		rc = -EACCES;
		goto out;
	}
#endif

	rc = flash_area_erase(fa, 0, fa->fa_size);
	if (rc != 0) {
		printk("Erase failed: %d\n", rc);
		goto out;
	}

#if defined(CONFIG_SOC_FAMILY_MICROCHIP_PIC32CK_SG_GC)
	/* PIC32CK row programming reads its source through the SRAM bus. */
	static uint8_t buf[DT_PROP(DT_NODELABEL(flash0), write_block_size_row)] __aligned(4);

	for (size_t off = 0; off < sizeof(bootloader); off += sizeof(buf)) {
		size_t len = MIN(sizeof(buf), sizeof(bootloader) - off);

		memcpy(buf, &bootloader[off], len);
		rc = flash_area_write(fa, off, buf, len);
		if (rc != 0) {
			break;
		}
	}
#else
	rc = flash_area_write(fa, 0, bootloader, sizeof(bootloader));
#endif
	if (rc != 0) {
		printk("Write failed: %d\n", rc);
	}

out:
#if defined(CONFIG_SOC_FAMILY_MICROCHIP_PIC32CK_SG_GC)
	/* Restore both regions, including after a failed erase or write. */
	for (size_t bank = 0; bank < ARRAY_SIZE(protection); bank++) {
		wp.boot_bank = bank == 0 ? BOOT_FLASH_BANK_1 : BOOT_FLASH_BANK_2;
		wp.wp_page = protection[bank] & FCW_LBWP_LBWP_Msk;
		int restore_rc = flash_ex_op(fa->fa_dev, FLASH_EX_OP_BFM_WRITE_PROTECT_ENABLE,
					   (uintptr_t)&wp, NULL);

		if (rc == 0) {
			rc = restore_rc;
		}
	}
	if (FCW_REGS->FCW_LBWP != protection[0] || FCW_REGS->FCW_UBWP != protection[1]) {
		printk("Failed to restore BFM write protection\n");
		if (rc == 0) {
			rc = -EIO;
		}
	}
#endif
	return rc;
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
	rc = rewrite_bootloader(fa);
	if (rc) {
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
