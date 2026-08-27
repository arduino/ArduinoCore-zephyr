/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 * Copyright (c) 2025 Arduino SA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * App-local copy of Zephyr's usbd_dfu_flash.c (CONFIG_USBD_DFU_FLASH, disabled
 * in prj.conf), adding only the dfu_activity_notify() calls.
 *
 * dfu_boot reboots into the new image once DFU traffic goes quiet. "Quiet"
 * cannot be derived from USBD_MSG_DFU_DOWNLOAD_COMPLETED alone, because an
 * upload is two dfu-util invocations (loader to alt 0, sketch to alt 1) and a
 * timer armed on the first completion fires mid-way through the second. The
 * image callbacks are the only place that sees individual transfers.
 */

#include <zephyr/kernel.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/class/usbd_dfu.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/dfu/flash_img.h>

#include "dfu_images.h"
#ifdef CONFIG_APP_BFM_DFU_ALT
#include <string.h>

#include "bfm_boot.h"
#include "bfm_flash.h"
#endif

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(dfu_images, CONFIG_USBD_DFU_LOG_LEVEL);

struct usbd_dfu_flash_data {
	struct flash_img_context fi_ctx;
	uint32_t last_block;
	const uint8_t id;
	union {
		uint32_t uploaded;
		uint32_t downloaded;
	};
};

static int dfu_flash_read(void *const priv,
			  const uint32_t block, const uint16_t size,
			  uint8_t buf[static CONFIG_USBD_DFU_TRANSFER_SIZE])
{
	struct usbd_dfu_flash_data *const data = priv;
	const struct flash_area *fa;
	uint32_t to_upload;
	int len;
	int ret;

	if (size == 0) {
		/* There is nothing to upload */
		return 0;
	}

	dfu_activity_notify();

	if (block == 0) {
		data->last_block = 0;
		data->uploaded = 0;
	} else {
		if (data->last_block + 1U != block) {
			return -EINVAL;
		}
	}

	ret = flash_area_open(data->id, &fa);
	if (ret) {
		return ret;
	}

	if (block == 0) {
		LOG_DBG("Flash area size %u", fa->fa_size);
	}

	to_upload = fa->fa_size - data->uploaded;
	if (to_upload < size) {
		len = to_upload;
	} else {
		len = size;
	}

	ret = flash_area_read(fa, data->uploaded, buf, len);
	flash_area_close(fa);
	if (ret) {
		return ret;
	}

	data->last_block = block;
	data->uploaded += size;
	LOG_DBG("uploaded %u block %u len %u", data->uploaded, block, len);

	return len;
}

static int dfu_flash_write(void *const priv,
			   const uint32_t block, const uint16_t size,
			   const uint8_t buf[static CONFIG_USBD_DFU_TRANSFER_SIZE])
{
	struct usbd_dfu_flash_data *const data = priv;
	const bool flush = size == 0 ? true : false;
	int ret;

	dfu_activity_notify();

	if (block == 0) {
		if (flash_img_init_id(&data->fi_ctx, data->id)) {
			return -EINVAL;
		}

		data->last_block = 0;
		data->downloaded = 0;

		if (size == 0) {
			/* There is nothing to download */
			return 0;
		}
	} else {
		if (data->last_block + 1U != block) {
			LOG_ERR("Block sequence error: expected %u, got %u (downloaded %u)",
				data->last_block + 1U, block, data->downloaded);
			return -EINVAL;
		}
	}

	ret = flash_img_buffered_write(&data->fi_ctx, buf, size, flush);
	if (ret) {
		return ret;
	}

	data->last_block = block;
	data->downloaded += size;
	LOG_DBG("downloaded %u (%u) block %u size %u", data->downloaded,
		flash_img_bytes_written(&data->fi_ctx), block, size);

	return 0;
}

static bool dfu_flash_next(void *const priv,
			   const enum usb_dfu_state state, const enum usb_dfu_state next)
{
	ARG_UNUSED(priv);

	if (state == DFU_MANIFEST_SYNC && next == DFU_IDLE) {
		LOG_DBG("Download finished");
	}

	return true;
}

#if PARTITION_EXISTS(slot0_partition)
static struct usbd_dfu_flash_data slot0_data = {
	.id = PARTITION_ID(slot0_partition),
};

USBD_DFU_DEFINE_IMG(slot0_image, "slot0_image", &slot0_data,
		    dfu_flash_read, dfu_flash_write, dfu_flash_next);
#endif

#if PARTITION_EXISTS(slot1_partition)
static struct usbd_dfu_flash_data slot1_data = {
	.id = PARTITION_ID(slot1_partition),
};

USBD_DFU_DEFINE_IMG(slot1_image, "slot1_image", &slot1_data,
		    dfu_flash_read, dfu_flash_write, dfu_flash_next);
#endif

#ifdef CONFIG_APP_BFM_DFU_ALT

/*
 * Alternate setting 2: the spare BFM slot -- this bootloader updating itself.
 *
 * It deliberately shares nothing with the two above. Those go through
 * flash_area/flash_img onto PFM partitions; BFM has no flash device and no
 * partitions at all (see app.overlay), so these callbacks drive bfm_flash.c
 * directly. That also sidesteps the singleton flash driver being aimed at PFM.
 *
 * The download lands in the slot that is NOT running, so a failure at any point
 * before the final promotion leaves the board booting exactly what it booted
 * before. Only the last quad write in bfm_boot_promote_spare() commits it.
 */

static struct bfm_dfu_data {
	uint32_t last_block;
	uint32_t downloaded;
} bfm_data;

static int bfm_dfu_read(void *const priv, const uint32_t block, const uint16_t size,
			uint8_t buf[static CONFIG_USBD_DFU_TRANSFER_SIZE])
{
	struct bfm_dfu_data *const data = priv;
	uint32_t remaining;
	uint16_t len;

	if (size == 0U) {
		return 0;
	}

	dfu_activity_notify();

	if (block == 0U) {
		data->last_block = 0;
		data->downloaded = 0;
	} else if (data->last_block + 1U != block) {
		return -EINVAL;
	}

	remaining = BFM_SLOT_SIZE - data->downloaded;
	len = (remaining < size) ? (uint16_t)remaining : size;

	memcpy(buf, (const void *)(uintptr_t)(BFM_SPARE_SLOT + data->downloaded), len);

	data->last_block = block;
	data->downloaded += len;

	return len;
}

static int bfm_dfu_write(void *const priv, const uint32_t block, const uint16_t size,
			 const uint8_t buf[static CONFIG_USBD_DFU_TRANSFER_SIZE])
{
	struct bfm_dfu_data *const data = priv;
	const bool flush = (size == 0U);
	int ret;

	dfu_activity_notify();

	if (block == 0U) {
		data->last_block = 0;
		data->downloaded = 0;

		ret = bfm_stream_begin(BFM_SPARE_SLOT, BFM_SLOT_SIZE);
		if (ret) {
			LOG_ERR("BFM stream begin failed: %d", ret);
			return ret;
		}

		if (flush) {
			/* Nothing to download. */
			return 0;
		}
	} else if (data->last_block + 1U != block) {
		LOG_ERR("Block sequence error: expected %u, got %u", data->last_block + 1U, block);
		return -EINVAL;
	}

	if (!flush) {
		ret = bfm_stream_write(buf, size);
		if (ret) {
			LOG_ERR("BFM write failed at %u: %d (intflag 0x%08x)", data->downloaded,
				ret, bfm_last_intflag());
			return ret;
		}

		data->last_block = block;
		data->downloaded += size;

		return 0;
	}

	ret = bfm_stream_finish();
	if (ret) {
		LOG_ERR("BFM stream finish failed: %d", ret);
		return ret;
	}

	/*
	 * Validate before committing. Promoting an image that cannot boot is
	 * the one way this path can brick a board, and it is entirely
	 * preventable -- the spare slot is readable right here.
	 */
	if (!bfm_boot_spare_image_valid()) {
		LOG_ERR("Spare BFM image failed validation, NOT promoting");
		return -ENOEXEC;
	}

	ret = bfm_boot_promote_spare();
	if (ret) {
		LOG_ERR("Promote failed: %d (intflag 0x%08x)", ret, bfm_last_intflag());
		return ret;
	}

	LOG_INF("BFM spare slot promoted (%u bytes); it boots after the next reset",
		bfm_stream_written());

	return 0;
}

/*
 * The identifier must sort AFTER slot0_image and slot1_image, hence "slot2_"
 * and not "bfm_".
 *
 * bAlternateSetting is assigned in the order the linker emits the
 * usbd_dfu_image iterable section, which is sorted by symbol name -- so the id
 * chosen here decides the alt number, and "bfm_image" would sort first and
 * silently renumber the other two. boards.txt pins bootloader.interface=0 to
 * the loader and upload.interface=1 to the sketch, so a shift there would make
 * `burn-bootloader` write the Zephyr loader into BFM and `upload` write a
 * sketch over the loader.
 *
 * The user-visible name is the second argument and is unaffected; dfu-util can
 * select by it (-a bfm_spare) instead of by number.
 */
USBD_DFU_DEFINE_IMG(slot2_bfm, "bfm_spare", &bfm_data, bfm_dfu_read, bfm_dfu_write,
		    dfu_flash_next);

#endif /* CONFIG_APP_BFM_DFU_ALT */

#if PARTITION_EXISTS(staging_partition)
/*
 * Alternate setting 3: MCUboot's secondary slot.
 *
 * This is where a combined loader+sketch bundle is staged. Writing it here is
 * inert on its own -- MCUboot only acts on it once the image is marked pending
 * and the board resets, and it reverts if the new image never confirms itself.
 * That makes it the safe counterpart to alt 0 and alt 1, both of which take
 * effect immediately and irreversibly.
 *
 * It is also the only probe-free way to exercise the swap without a working
 * network stack, which is why it exists before the OTA path does.
 *
 * Plain flash_area callbacks like slot0/slot1 -- unlike alt 2, the secondary
 * slot is an ordinary PFM partition.
 *
 * The id sorts after "slot2_bfm", preserving the alt numbering the two
 * boards.txt interface pins depend on. See the comment above.
 */
static struct usbd_dfu_flash_data slot3_data = {
	.id = PARTITION_ID(staging_partition),
};

USBD_DFU_DEFINE_IMG(slot3_staging, "ota_staging", &slot3_data,
		    dfu_flash_read, dfu_flash_write, dfu_flash_next);
#endif
