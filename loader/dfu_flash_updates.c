/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Loader USB DFU mode: a "complete_image" and a "sketch" alternate, both
 * writing slot1 and requesting a permanent MCUboot update. The request is a
 * boot-mode value, the image starts one sector into slot1 (.dfu padding), the
 * sketch alternate checks the loader range instead of skipping it, and a
 * stale trailer is erased. */
#ifdef CONFIG_LOADER_DFU_MODE

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/devicetree/fixed-partitions.h>
#include <zephyr/retention/bootmode.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/class/usbd_dfu.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/dfu/flash_img.h>
#include <zephyr/logging/log.h>

#include "slot1_trailer.h"
LOG_MODULE_REGISTER(dfu_flash_updates);

/* Sketch layout from the running image's header (zephyr-sketch-tool writes it
 * after the 32-byte MCUboot header), used by the "sketch" alternate; read
 * only when DFU is requested. */
struct sketch_flash_layout {
	uint32_t sketch_offset;
	uint32_t erase_size;
	uint32_t block_num;
};

static struct sketch_flash_layout sketch_layout;
#define MCU_BOOT_HEADER_OFFSET 32
static void retrieve_flash_info(void) {
	const struct flash_area *fa;
	int rc;
	uint32_t value = 0;
	sketch_layout.sketch_offset = 0;
	sketch_layout.erase_size = 0;
	sketch_layout.block_num = 0;

	rc = flash_area_open(PARTITION_ID(slot0_partition), &fa);
	if (rc) {
		LOG_ERR("Failed to open flash area, rc %d\n", rc);
		return;
	}

	rc = flash_area_read(fa, MCU_BOOT_HEADER_OFFSET, &value, sizeof(value));
	if (rc) {
		LOG_ERR("Failed to read sketch_address, rc %d\n", rc);
	} else {
		sketch_layout.sketch_offset = value;
	}

	rc = flash_area_read(fa, MCU_BOOT_HEADER_OFFSET + 4, &value, sizeof(value));
	if (rc) {
		LOG_ERR("Failed to read eraze size, rc %d\n", rc);
	} else {
		sketch_layout.erase_size = value;
	}

	rc = flash_area_read(fa, MCU_BOOT_HEADER_OFFSET + 8, &value, sizeof(value));
	if (rc) {
		LOG_ERR("Failed to read block num, rc %d\n", rc);
	} else {
		sketch_layout.block_num = value;
	}

	LOG_INF("sketch offset: 0x%08X", sketch_layout.sketch_offset);
	LOG_INF("erase size: 0x%08X", sketch_layout.erase_size);
	LOG_INF("sketch block number: 0x%08X", sketch_layout.block_num);
	flash_area_close(fa);
}

/* The .dfu file starts one erased sector early (zsk --dfu_offset 4096). */
#define XL_IMAGE_OFFSET 0x1000
#define XL_IMAGE_MAGIC  0x96f3b83dU /* MCUboot image header */

/* The request is a boot-mode value; Nano XL has no retention2. */
bool check_dfu_mode_requested(void) {
	if (bootmode_check(CONFIG_LOADER_DFU_BOOT_MODE) == 1) {
		if (bootmode_clear() < 0) {
			LOG_ERR("ERROR: Unable delete boot mode\n");
		}
		return true;
	}
	return false;
}

struct usbd_dfu_flash_data {
	struct flash_img_context fi_ctx;
	uint32_t last_block;
	uint32_t skip; /* Leading pad bytes still to drop */
	const uint8_t id;

	union {
		uint32_t uploaded;
		uint32_t downloaded;
	};
};

static int dfu_flash_read(void *const priv, const uint32_t block, const uint16_t size,
						  uint8_t buf[static CONFIG_USBD_DFU_TRANSFER_SIZE]) {
	struct usbd_dfu_flash_data *const data = priv;
	const struct flash_area *fa;
	uint32_t to_upload;
	int len;
	int ret;

	if (size == 0) {
		return 0;
	}

	if (block == 0) {
		data->last_block = 0;
		data->uploaded = 0;
	} else if (data->last_block + 1U != block) {
		return -EINVAL;
	}

	ret = flash_area_open(data->id, &fa);
	if (ret) {
		return ret;
	}

	to_upload = fa->fa_size - data->uploaded;
	len = (to_upload < size) ? (int)to_upload : (int)size;

	ret = flash_area_read(fa, data->uploaded, buf, len);
	flash_area_close(fa);
	if (ret) {
		return ret;
	}

	data->last_block = block;
	data->uploaded += size;

	return len;
}

static int dfu_flash_write(void *const priv, const uint32_t block, const uint16_t size,
						   const uint8_t buf[static CONFIG_USBD_DFU_TRANSFER_SIZE]) {
	struct usbd_dfu_flash_data *const data = priv;
	const bool flush = (size == 0);
	int ret;

	if (block == 0) {
		if (flash_img_init_id(&data->fi_ctx, data->id)) {
			return -EINVAL;
		}
		/* flash_img clears only the trailer's first page. */
		ret = slot1_erase_trailer(data->fi_ctx.flash_area);
		if (ret) {
			return ret;
		}

		data->last_block = 0;
		data->downloaded = 0;

		if (size == 0) {
			/* Nothing to download */
			return 0;
		}
	} else if (data->last_block + 1U != block) {
		return -EINVAL;
	}

	/*
	 * The IDE sends the same .dfu file to every alt; it starts with one
	 * erased sector that flash_img does not want (it already writes one sector
	 * into slot1 in swap-using-offset). An image without the pad is taken as is.
	 */
	if (block == 0) {
		data->skip = sys_get_le32(buf) == XL_IMAGE_MAGIC ? 0 : XL_IMAGE_OFFSET;
	}
	const uint16_t drop = MIN(size, data->skip);

	data->skip -= drop;
	if (size - drop > 0 || flush) {
		ret = flash_img_buffered_write(&data->fi_ctx, &buf[drop], size - drop, flush);
		if (ret) {
			return ret;
		}
	}

	data->last_block = block;
	data->downloaded += size;

	return 0;
}

/* Loader update image: writes to the MCUboot secondary slot (slot1_partition) */
static bool slot1_next(void *priv, enum usb_dfu_state state, enum usb_dfu_state next) {
	ARG_UNUSED(priv);

	if (state == DFU_MANIFEST_SYNC && next == DFU_IDLE) {
		LOG_INF("Loader update download finished, requesting MCUboot upgrade");
		if (IS_ENABLED(CONFIG_BOOTLOADER_MCUBOOT)) {
			boot_request_upgrade(BOOT_UPGRADE_PERMANENT); /* Overwrite, see mcuboot_hooks */
		}
	}

	return true;
}

static struct usbd_dfu_flash_data slot1_data = {
	.id = PARTITION_ID(slot1_partition),
};

USBD_DFU_DEFINE_IMG(all_image, "complete_image", &slot1_data, dfu_flash_read, dfu_flash_write,
					slot1_next);

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* ---------------------- DYNAMIC DFU ALTERNATE ----------------------------- */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

#define UPDATE_PARTITION_ID   PARTITION_ID(slot1_partition)
#define UPDATE_PARTITION_SIZE DT_REG_SIZE(DT_NODELABEL(slot1_partition))

/* Running state of the dynamic sketch DFU write; the flash layout it relies
 * on comes from sketch_layout (retrieve_flash_info()). */
struct sketch_dfu_state {
	uint32_t current_offset;
};

static struct sketch_dfu_state dfu_state = {.current_offset = 0};

/*
 * The gap (the loader) is checked instead of skipped. After an OTA swap
 * slot1 holds the previous image one sector lower, and after an image with a
 * different loader slot1 holds that loader: a blind skip would leave a slot1
 * image that MCUboot rejects. Each gap sector is compared with slot1 and
 * rewritten only when it differs.
 */
#define GAP_SECTOR_MAX 4096
#define GAP_CMP_CHUNK  256

static struct {
	uint8_t buf[GAP_SECTOR_MAX] __aligned(4);
	uint32_t fill;
	uint32_t rewritten;
	uint32_t skipped;
} gap;

static int gap_flush(const struct flash_area *fa, uint32_t off, uint32_t sector)
{
	uint8_t tmp[GAP_CMP_CHUNK];
	bool equal = true;
	bool erased = true;
	int err;

	for (uint32_t i = 0; i < gap.fill && (equal || erased); i += GAP_CMP_CHUNK) {
		uint32_t n = MIN(GAP_CMP_CHUNK, gap.fill - i);

		err = flash_area_read(fa, off + i, tmp, n);
		if (err) {
			return err;
		}
		equal = equal && memcmp(tmp, &gap.buf[i], n) == 0;
		for (uint32_t j = 0; erased && j < n; j++) {
			erased = tmp[j] == 0xff;
		}
	}

	if (equal) {
		gap.skipped++;
	} else {
		if (gap.fill != sector) {
			LOG_ERR("Gap end 0x%08x not sector aligned", off + gap.fill);
			return -EINVAL;
		}
		if (!erased) {
			err = flash_area_erase(fa, off, sector);
			if (err) {
				return err;
			}
		}
		err = flash_area_write(fa, off, gap.buf, sector);
		if (err) {
			return err;
		}
		gap.rewritten++;
	}

	gap.fill = 0;
	return 0;
}

static int dynamic_flash_write(void *const priv, const uint32_t block, const uint16_t size,
							   const uint8_t buf[static CONFIG_USBD_DFU_TRANSFER_SIZE]) {
	const struct flash_area *fa;
	struct sketch_dfu_state *const data = priv;
	int err;

	if (size == 0) {
		LOG_INF("Loader region: %u sectors kept, %u rewritten", gap.skipped,
			gap.rewritten);
		return 0;
	}

	/* The sketch alone can only be rewritten when it starts on its own erase
	 * block (loader padded to the erase size). The header's padding flag is
	 * not used: zephyr-sketch-tool always writes 1 ("not padded"). */
	if (sketch_layout.erase_size == 0 ||
	    sketch_layout.sketch_offset % sketch_layout.erase_size != 0) {
		LOG_ERR("Sketch shares an erase block with the loader, use complete_image");
		return -ENOTSUP;
	}
	uint32_t start_offset = XL_IMAGE_OFFSET + sketch_layout.sketch_offset;

	err = flash_area_open(UPDATE_PARTITION_ID, &fa);
	if (err) {
		LOG_ERR("Failed to open flash area (err %d)", err);
		return err;
	}

	/* Get hardware write block size directly from the flash device */
	size_t wbs = flash_get_write_block_size(flash_area_get_device(fa));

	if (block == 0) {
		if (sketch_layout.sketch_offset == 0 || sketch_layout.erase_size == 0 ||
			sketch_layout.block_num == 0) {
			LOG_ERR("Flash parameter not set");
			err = -EINVAL;
			goto end;
		}

		uint32_t sketch_total_size = sketch_layout.erase_size * sketch_layout.block_num;

		if ((XL_IMAGE_OFFSET + sketch_layout.sketch_offset + sketch_total_size) > UPDATE_PARTITION_SIZE) {
			LOG_ERR("Sketch offset exceeds partition size");
			err = -ENOSPC;
			goto end;
		}

		if (sketch_layout.erase_size > GAP_SECTOR_MAX) {
			err = -ENOTSUP;
			goto end;
		}

		data->current_offset = 0;
		gap.fill = 0;
		gap.rewritten = 0;
		gap.skipped = 0;

		/* Clear a stale trailer (e.g. left by an OTA swap). */
		err = slot1_erase_trailer(fa);
		if (err) {
			LOG_ERR("Trailer erase failed (%d)", err);
			goto end;
		}

		LOG_INF("Erase header (1) 0x%08x bytes at offset 0", sketch_layout.erase_size);
		err = flash_area_erase(fa, 0, XL_IMAGE_OFFSET + sketch_layout.erase_size);
		if (err) {
			LOG_ERR("Flash area erase failed (%d)", err);
			goto end;
		}

		LOG_INF("Erase sketch initial block (2) 0x%08x bytes at offset 0x%08x",
				sketch_layout.erase_size, start_offset);
		err = flash_area_erase(fa, start_offset, sketch_layout.erase_size);
		if (err) {
			LOG_ERR("Flash area erase failed - 2 (%d)", err);
			goto end;
		}
	}

	uint32_t buf_offset = 0;
	uint32_t remaining_size = size;

	while (remaining_size > 0) {
		uint32_t write_offset;
		uint32_t process_size;

		/* Case 1: Header region */
		if (data->current_offset < XL_IMAGE_OFFSET + sketch_layout.erase_size) {
			process_size = MIN(remaining_size,
					   XL_IMAGE_OFFSET + sketch_layout.erase_size - data->current_offset);
			write_offset = data->current_offset;
		}
		/* Case 2: Gap between header and sketch (checked, see gap_flush) */
		else if (data->current_offset < start_offset) {
			process_size = MIN(remaining_size,
					   MIN(start_offset - data->current_offset,
					       sketch_layout.erase_size - gap.fill));
			memcpy(&gap.buf[gap.fill], &buf[buf_offset], process_size);
			gap.fill += process_size;
			data->current_offset += process_size;
			buf_offset += process_size;
			remaining_size -= process_size;

			if (gap.fill == sketch_layout.erase_size ||
			    data->current_offset == start_offset) {
				err = gap_flush(fa, data->current_offset - gap.fill,
						sketch_layout.erase_size);
				if (err) {
					LOG_ERR("Loader region update failed (%d)", err);
					goto end;
				}
			}
			continue;
		}
		/* Case 3: Sketch region */
		else {
			write_offset = data->current_offset;

			if (write_offset >= UPDATE_PARTITION_SIZE) {
				LOG_ERR("Write offset 0x%08x exceeds partition size", write_offset);
				err = -ENOSPC;
				goto end;
			}

			process_size = MIN(remaining_size, UPDATE_PARTITION_SIZE - write_offset);

			/* Erase dynamically as we cross into a new block */
			if (write_offset > start_offset && (write_offset % sketch_layout.erase_size == 0)) {
				LOG_INF("Dynamically erasing block of size 0x%08x at offset 0x%08x",
						sketch_layout.erase_size, write_offset);
				err = flash_area_erase(fa, write_offset, sketch_layout.erase_size);
				if (err) {
					LOG_ERR("Dynamic flash area erase failed at offset 0x%08x (%d)", write_offset,
							err);
					goto end;
				}
			}
		}

		/*
		 * Round up length to align with hardware write block size.
		 * Extra uninitialized trailing bytes from buf will be written to flash.
		 */
		size_t write_len = ROUND_UP(process_size, wbs);

		err = flash_area_write(fa, write_offset, &buf[buf_offset], write_len);
		if (err) {
			LOG_ERR("Flash write failed at offset 0x%08x (len %zu)", write_offset, write_len);
			goto end;
		}

		data->current_offset += process_size;
		buf_offset += process_size;
		remaining_size -= process_size;
	}

end:
	flash_area_close(fa);
	return err;
}

USBD_DFU_DEFINE_IMG(sketch_image, "sketch", &dfu_state, NULL, dynamic_flash_write, slot1_next);

/* +++ DFU USB CONFIGURATION and FUNCTIONS +++ */

USBD_DEVICE_DEFINE(dfu_usbd, DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)), CONFIG_USB_DEVICE_VID,
				   CONFIG_LOADER_DFU_PID ? CONFIG_LOADER_DFU_PID
							 : CONFIG_USB_DEVICE_PID + 0x0100);

USBD_DESC_LANG_DEFINE(sample_lang);
USBD_DESC_CONFIG_DEFINE(fs_cfg_desc, "DFU FS Configuration");
USBD_DESC_CONFIG_DEFINE(hs_cfg_desc, "DFU HS Configuration");

USBD_DESC_MANUFACTURER_DEFINE(sample_mfr, "Arduino");
USBD_DESC_PRODUCT_DEFINE(sample_product, CONFIG_USB_DEVICE_PRODUCT " DFU");
/* Same serial as the CDC port and MCUboot, for dfu-util -S. */
USBD_DESC_SERIAL_NUMBER_DEFINE(dfu_sn);

static const uint8_t dfu_attributes = USB_SCD_SELF_POWERED | USB_SCD_REMOTE_WAKEUP;

USBD_CONFIGURATION_DEFINE(sample_fs_config, dfu_attributes, 100, &fs_cfg_desc);
USBD_CONFIGURATION_DEFINE(sample_hs_config, dfu_attributes, 100, &hs_cfg_desc);

K_SEM_DEFINE(dfu_update_sem, 0, 1);

static void dfu_msg_cb(struct usbd_context *const usbd_ctx, const struct usbd_msg *const msg) {
	LOG_INF("USBD message: %s", usbd_msg_type_string(msg->type));

	if (usbd_can_detect_vbus(usbd_ctx)) {
		if (msg->type == USBD_MSG_VBUS_READY) {
			usbd_enable(usbd_ctx);
		}
		if (msg->type == USBD_MSG_VBUS_REMOVED) {
			usbd_disable(usbd_ctx);
		}
	}

	if (msg->type == USBD_MSG_DFU_DOWNLOAD_COMPLETED) {
		LOG_INF("DFU firmware download completed");

		/* Unlock the thread waiting for the update */
		k_sem_give(&dfu_update_sem);
	}
}

/* blocking function - it waits until a dfu update is performed */
static void dfu_update(void) {
	int err;

	LOG_INF("Initializing USB DFU mode...");

	err = usbd_add_descriptor(&dfu_usbd, &sample_lang);
	if (err) {
		LOG_ERR("Failed to initialize language descriptor (%d)", err);
		return;
	}

	err = usbd_add_descriptor(&dfu_usbd, &sample_mfr);
	if (err) {
		LOG_ERR("Failed to initialize manufacturer descriptor (%d)", err);
		return;
	}

	err = usbd_add_descriptor(&dfu_usbd, &sample_product);
	if (err) {
		LOG_ERR("Failed to initialize product descriptor (%d)", err);
		return;
	}

	err = usbd_add_descriptor(&dfu_usbd, &dfu_sn);
	if (err) {
		LOG_ERR("Failed to initialize SN descriptor (%d)", err);
		return;
	}

	if (usbd_caps_speed(&dfu_usbd) == USBD_SPEED_HS) {
		usbd_add_configuration(&dfu_usbd, USBD_SPEED_HS, &sample_hs_config);
		usbd_register_class(&dfu_usbd, "dfu_dfu", USBD_SPEED_HS, 1);
		usbd_device_set_code_triple(&dfu_usbd, USBD_SPEED_HS, 0, 0, 0);
	}

	err = usbd_add_configuration(&dfu_usbd, USBD_SPEED_FS, &sample_fs_config);
	if (err) {
		LOG_ERR("Failed to add Full-Speed configuration");
		return;
	}

	err = usbd_register_class(&dfu_usbd, "dfu_dfu", USBD_SPEED_FS, 1);
	if (err) {
		LOG_ERR("Failed to add register classes");
		return;
	}

	usbd_device_set_code_triple(&dfu_usbd, USBD_SPEED_FS, 0, 0, 0);

	err = usbd_init(&dfu_usbd);
	if (err) {
		LOG_ERR("Failed to initialize USB device support");
		return;
	}

	err = usbd_msg_register_cb(&dfu_usbd, dfu_msg_cb);
	if (err) {
		LOG_ERR("Failed to register message callback");
		goto shutdown;
	}

	err = usbd_enable(&dfu_usbd);
	if (err) {
		LOG_ERR("Failed to enable USB device support");
		goto shutdown;
	}

	LOG_INF("USB initialized. Waiting indefinitely for DFU update...");

	k_sem_take(&dfu_update_sem, K_FOREVER);

	/*
	 * Add a short delay to allow the host (dfu-util) to request
	 * and read the final DFU status before we kill the connection.
	 */
	k_msleep(500);

	LOG_INF("DFU operation fulfilled. Deinitializing USB...");

	usbd_disable(&dfu_usbd);
	usbd_shutdown(&dfu_usbd);

	LOG_INF("Rebooting to apply MCUboot upgrade...");
	sys_reboot(SYS_REBOOT_COLD);

shutdown:
	/* Release the controller: loader() brings up its own USB device next. */
	usbd_shutdown(&dfu_usbd);
}

void wait_for_dfu_update(void) {
	retrieve_flash_info(); /* Only when DFU is requested */
	/* waits for dfu update */
	dfu_update();
}

#endif /* CONFIG_LOADER_DFU_MODE */
