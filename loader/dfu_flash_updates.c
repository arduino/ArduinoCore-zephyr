/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef CONFIG_USE_DFU_FLASH_UPDATES

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/devicetree/fixed-partitions.h>
#include <zephyr/retention/retention.h>
#include <zephyr/retention/bootmode.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/class/usbd_dfu.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/dfu/flash_img.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(dfu_flash_updates);

#ifdef CONFIG_USE_DFU_FADING_BLINK
void fading_blink_led_start(void);
void fading_blink_led_stop(void);
#endif

#ifdef CONFIG_USE_LOADER_AND_BINARY_SQUASHED
/* Defined in main.c (under CONFIG_USE_LOADER_AND_BINARY_SQUASHED): holds the
 * flash layout info populated there by retrieve_flash_info() and consumed
 * here by the dynamic sketch DFU alternate. */
struct sketch_flash_layout {
	const struct device *flash_dev;
	uint32_t sketch_offset;
	uint32_t sketch_addr;
	uint32_t erase_size;
	uint32_t block_num;
	bool custom_padding;
};

extern struct sketch_flash_layout sketch_layout;
#endif

const struct device *retention2 = DEVICE_DT_GET(DT_NODELABEL(retention2));

bool check_dfu_mode_requested(void) {
	uint8_t magic = 0;
	if (retention_read(retention2, 0, &magic, 1)) {
		LOG_ERR("Unable to read retention2\n");
	} else {
		if (magic == CONFIG_LOADER_DFU_MAGIC) {
			if (retention_clear(retention2) < 0) {
				LOG_ERR("ERROR: Unable delete retention2\n");
			}
			return true;
		}
	}
	return false;
}

struct usbd_dfu_flash_data {
	struct flash_img_context fi_ctx;
	uint32_t last_block;
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

		data->last_block = 0;
		data->downloaded = 0;

		if (size == 0) {
			/* Nothing to download */
			return 0;
		}
	} else if (data->last_block + 1U != block) {
		return -EINVAL;
	}

	ret = flash_img_buffered_write(&data->fi_ctx, buf, size, flush);
	if (ret) {
		return ret;
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
			boot_request_upgrade(false);
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

#ifdef CONFIG_USE_LOADER_AND_BINARY_SQUASHED

#define UPDATE_PARTITION_ID   PARTITION_ID(slot1_partition)
#define UPDATE_PARTITION_SIZE DT_REG_SIZE(DT_NODELABEL(slot1_partition))

/* Running state of the dynamic sketch DFU write; the flash layout it relies
 * on (sketch_offset, erase_size, block_num, custom_padding) comes from
 * sketch_layout, populated by main.c's retrieve_flash_info(). */
struct sketch_dfu_state {
	uint32_t current_offset;
};

static struct sketch_dfu_state dfu_state = {.current_offset = 0};

static int dynamic_flash_write(void *const priv, const uint32_t block, const uint16_t size,
							   const uint8_t buf[static CONFIG_USBD_DFU_TRANSFER_SIZE]) {
	const struct flash_area *fa;
	struct sketch_dfu_state *const data = priv;
	int err;

	if (size == 0) {
		return 0;
	}

	/* In case there is no custom padding the sketch is automatically aligned
	 * at the first flash block after the loader, so we start to delete the
	 * sketch from there*/
	uint32_t start_offset = sketch_layout.sketch_offset;
	if (sketch_layout.custom_padding) {
		start_offset =
			(sketch_layout.sketch_offset / sketch_layout.erase_size) * sketch_layout.erase_size;
		LOG_INF("Custom padding is applied, start update at offset %d (0x%08X)", start_offset,
				start_offset);
	}

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

		if ((sketch_layout.sketch_offset + sketch_total_size) > UPDATE_PARTITION_SIZE) {
			LOG_ERR("Sketch offset exceeds partition size");
			err = -ENOSPC;
			goto end;
		}

		data->current_offset = 0;

		LOG_INF("Erase header (1) 0x%08x bytes at offset 0", sketch_layout.erase_size);
		err = flash_area_erase(fa, 0, sketch_layout.erase_size);
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
		if (data->current_offset < sketch_layout.erase_size) {
			process_size = MIN(remaining_size, sketch_layout.erase_size - data->current_offset);
			write_offset = data->current_offset;
		}
		/* Case 2: Gap between header and sketch */
		else if (data->current_offset < start_offset) {
			process_size = MIN(remaining_size, start_offset - data->current_offset);

			/* Advance stream counters without writing */
			data->current_offset += process_size;
			buf_offset += process_size;
			remaining_size -= process_size;
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

#endif /* CONFIG_USE_LOADER_AND_BINARY_SQUASHED */

/* +++ DFU USB CONFIGURATION and FUNCTIONS +++ */

USBD_DEVICE_DEFINE(dfu_usbd, DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)), CONFIG_USB_DEVICE_VID,
				   CONFIG_USB_DEVICE_PID + 0x0100);

USBD_DESC_LANG_DEFINE(sample_lang);
USBD_DESC_CONFIG_DEFINE(fs_cfg_desc, "DFU FS Configuration");
USBD_DESC_CONFIG_DEFINE(hs_cfg_desc, "DFU HS Configuration");

USBD_DESC_MANUFACTURER_DEFINE(sample_mfr, "Arduino");
USBD_DESC_PRODUCT_DEFINE(sample_product, "Arduino Mezza DFU");

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
		return;
	}

	err = usbd_enable(&dfu_usbd);
	if (err) {
		LOG_ERR("Failed to enable USB device support");
		return;
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
}

void wait_for_dfu_update(void) {
#ifdef CONFIG_USE_DFU_FADING_BLINK
	fading_blink_led_start();
#endif
	/* waits for dfu update */
	dfu_update();
#ifdef CONFIG_USE_DFU_FADING_BLINK
	fading_blink_led_stop();
#endif
}

#endif /* CONFIG_USE_DFU_FLASH_UPDATES */
