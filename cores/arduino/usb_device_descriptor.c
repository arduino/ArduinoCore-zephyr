/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/bos.h>

#include <zephyr/logging/log.h>
#if __has_include(<variant.h>)
#include <variant.h>
#endif

#ifdef CONFIG_USB_DEVICE_STACK_NEXT

/* By default, do not register the USB DFU class DFU mode instance. */
static const char *const blocklist[] = {
	"dfu_dfu",
#if defined(LOADER_PROVIDES_EXTRA_USB_CLASSES)
	"cdc_acm_0",
#endif
	NULL,
};

/* doc device instantiation start */
USBD_DEVICE_DEFINE(usbd, DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)), CONFIG_USB_DEVICE_VID,
				   CONFIG_USB_DEVICE_PID);
/* doc device instantiation end */

/* doc string instantiation start */
USBD_DESC_LANG_DEFINE(sample_lang);
USBD_DESC_MANUFACTURER_DEFINE(sample_mfr, CONFIG_USB_DEVICE_MANUFACTURER);
USBD_DESC_PRODUCT_DEFINE(sample_product, CONFIG_USB_DEVICE_PRODUCT);
USBD_DESC_SERIAL_NUMBER_DEFINE(sample_sn);
/* doc string instantiation end */

USBD_DESC_CONFIG_DEFINE(fs_cfg_desc, "FS Configuration");
USBD_DESC_CONFIG_DEFINE(hs_cfg_desc, "HS Configuration");

/* doc configuration instantiation start */
static const uint8_t attributes = 0;

/* Full speed configuration */
USBD_CONFIGURATION_DEFINE(sample_fs_config, attributes, 250, &fs_cfg_desc);

/* High speed configuration */
USBD_CONFIGURATION_DEFINE(sample_hs_config, attributes, 250, &hs_cfg_desc);
/* doc configuration instantiation end */

/*
 * This does not yet provide valuable information, but rather serves as an
 * example, and will be improved in the future.
 */
static const struct usb_bos_capability_lpm bos_cap_lpm = {
	.bLength = sizeof(struct usb_bos_capability_lpm),
	.bDescriptorType = USB_DESC_DEVICE_CAPABILITY,
	.bDevCapabilityType = USB_BOS_CAPABILITY_EXTENSION,
	.bmAttributes = 0UL,
};

USBD_DESC_BOS_DEFINE(sample_usbext, sizeof(bos_cap_lpm), &bos_cap_lpm);

static void sample_fix_code_triple(struct usbd_context *uds_ctx, const enum usbd_speed speed) {
	/* Always use class code information from Interface Descriptors */
	if (IS_ENABLED(CONFIG_USBD_CDC_ACM_CLASS) || IS_ENABLED(CONFIG_USBD_CDC_ECM_CLASS) ||
		IS_ENABLED(CONFIG_USBD_CDC_NCM_CLASS) || IS_ENABLED(CONFIG_USBD_AUDIO2_CLASS)) {
		/*
		 * Class with multiple interfaces have an Interface
		 * Association Descriptor available, use an appropriate triple
		 * to indicate it.
		 */
		usbd_device_set_code_triple(uds_ctx, speed, USB_BCC_MISCELLANEOUS, 0x02, 0x01);
	} else {
		usbd_device_set_code_triple(uds_ctx, speed, 0, 0, 0);
	}
}

static int register_cdc_acm_0(struct usbd_context *const uds_ctx,
			      const enum usbd_speed speed)
{
	struct usbd_config_node *cfg_nd;
	int err;

	err = usbd_register_class(&usbd, "cdc_acm_0", speed, 1);
	if (err) {
		return err;
	}

	return usbd_device_set_code_triple(uds_ctx, speed,
					   USB_BCC_MISCELLANEOUS, 0x02, 0x01);
}

struct usbd_context *usbd_setup_device(usbd_msg_cb_t msg_cb) {
	int err;

	/* doc add string descriptor start */
	err = usbd_add_descriptor(&usbd, &sample_lang);
	if (err) {
		return NULL;
	}

	err = usbd_add_descriptor(&usbd, &sample_mfr);
	if (err) {
		return NULL;
	}

	err = usbd_add_descriptor(&usbd, &sample_product);
	if (err) {
		return NULL;
	}

	err = usbd_add_descriptor(&usbd, &sample_sn);
	if (err) {
		return NULL;
	}
	/* doc add string descriptor end */

	if (usbd_caps_speed(&usbd) == USBD_SPEED_HS) {
		err = usbd_add_configuration(&usbd, USBD_SPEED_HS, &sample_hs_config);
		if (err) {
			return NULL;
		}

		err = usbd_register_all_classes(&usbd, USBD_SPEED_HS, 1, blocklist);
		if (err) {
			return NULL;
		}

		#if defined(LOADER_PROVIDES_EXTRA_USB_CLASSES)
		register_cdc_acm_0(&usbd, USBD_SPEED_HS);
		#endif

		sample_fix_code_triple(&usbd, USBD_SPEED_HS);
	}

	/* doc configuration register start */
	err = usbd_add_configuration(&usbd, USBD_SPEED_FS, &sample_fs_config);
	if (err) {
		return NULL;
	}
	/* doc configuration register end */

	/* doc functions register start */
	err = usbd_register_all_classes(&usbd, USBD_SPEED_FS, 1, blocklist);
	if (err) {
		return NULL;
	}
	/* doc functions register end */

	#if defined(LOADER_PROVIDES_EXTRA_USB_CLASSES)
	register_cdc_acm_0(&usbd, USBD_SPEED_HS);
	#endif

	sample_fix_code_triple(&usbd, USBD_SPEED_FS);

	if (msg_cb != NULL) {
		/* doc device init-and-msg start */
		err = usbd_msg_register_cb(&usbd, msg_cb);
		if (err) {
			return NULL;
		}
		/* doc device init-and-msg end */
	}

	if (0) {
		(void)usbd_device_set_bcd_usb(&usbd, USBD_SPEED_FS, 0x0201);
		(void)usbd_device_set_bcd_usb(&usbd, USBD_SPEED_HS, 0x0201);

		err = usbd_add_descriptor(&usbd, &sample_usbext);
		if (err) {
			return NULL;
		}
	}

	return &usbd;
}

struct usbd_context *usbd_init_device(usbd_msg_cb_t msg_cb) {
	int err;

	if (usbd_setup_device(msg_cb) == NULL) {
		return NULL;
	}

	/* doc device init start */
	err = usbd_init(&usbd);
	if (err) {
		return NULL;
	}
	/* doc device init end */

	return &usbd;
}

#endif
