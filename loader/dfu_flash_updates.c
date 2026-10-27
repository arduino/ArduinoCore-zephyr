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

/* User sketch update image: writes directly to the user_sketch partition */
static bool user_sketch_next(void *priv, enum usb_dfu_state state, enum usb_dfu_state next) {
	ARG_UNUSED(priv);

	if (state == DFU_MANIFEST_SYNC && next == DFU_IDLE) {
		LOG_INF("Sketch update download finished");
		/* TODO: verify the sketch signature before marking it usable */
	}

	return true;
}

static struct usbd_dfu_flash_data user_sketch_data = {
	.id = PARTITION_ID(user_sketch),
};

USBD_DFU_DEFINE_IMG(user_sketch_image, "user_sketch_image", &user_sketch_data, dfu_flash_read,
					dfu_flash_write, user_sketch_next);

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
