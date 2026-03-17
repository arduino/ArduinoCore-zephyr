/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "zephyr/sys/printk.h"
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sketch);

#include <zephyr/kernel.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/llext/llext.h>
#include <zephyr/llext/buf_loader.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>
#include <zephyr/logging/log_ctrl.h>

#include <stdlib.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/uart/cdc_acm.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>

#include <zephyr/devicetree/fixed-partitions.h>

#define HEADER_LEN 16

struct sketch_header_v1 {
	uint8_t ver;    // @ 0x07
	uint32_t len;   // @ 0x08
	uint16_t magic; // @ 0x0c
	uint8_t flags;  // @ 0x0e
} __attribute__((packed));

#define SKETCH_FLAG_DEBUG     0x01
#define SKETCH_FLAG_LINKED    0x02
#define SKETCH_FLAG_IMMEDIATE 0x04

#define SKETCH_RAM_BUFFER_LEN 131072

#define TARGET_HAS_USB_CDC                                                                         \
	DT_NODE_HAS_PROP(DT_PATH(zephyr_user), cdc_acm) &&                                             \
		(CONFIG_USB_DEVICE_STACK || CONFIG_USB_DEVICE_STACK_NEXT)

#if TARGET_HAS_USB_CDC
const struct device *const usb_dev =
	DEVICE_DT_GET(DT_PHANDLE_BY_IDX(DT_PATH(zephyr_user), cdc_acm, 0));

#if CONFIG_USB_DEVICE_STACK_NEXT
#include <zephyr/usb/usbd.h>
struct usbd_context *usbd_init_device(usbd_msg_cb_t msg_cb);

int usb_enable(usb_dc_status_callback status_cb) {
	int err;
	struct usbd_context *_usbd = usbd_init_device(NULL);
	if (_usbd == NULL) {
		return -ENODEV;
	}
	if (!usbd_can_detect_vbus(_usbd)) {
		err = usbd_enable(_usbd);
		if (err) {
			return err;
		}
	}
	return 0;
}
#endif

#if CONFIG_SHELL
static int enable_shell_usb(void) {
	bool log_backend = CONFIG_SHELL_BACKEND_SERIAL_LOG_LEVEL > 0;
	uint32_t level = (CONFIG_SHELL_BACKEND_SERIAL_LOG_LEVEL > LOG_LEVEL_DBG) ?
						 CONFIG_LOG_MAX_LEVEL :
						 CONFIG_SHELL_BACKEND_SERIAL_LOG_LEVEL;
	static const struct shell_backend_config_flags cfg_flags = SHELL_DEFAULT_BACKEND_CONFIG_FLAGS;

	shell_init(shell_backend_uart_get_ptr(), usb_dev, cfg_flags, log_backend, level);

	return 0;
}
#endif
#endif

#ifdef CONFIG_USERSPACE
K_THREAD_STACK_DEFINE(llext_stack, CONFIG_MAIN_STACK_SIZE);
struct k_thread llext_thread;

void llext_entry(void *arg0, void *arg1, void *arg2) {
	void (*fn)(struct llext_loader *, struct llext *) = arg0;
	fn(arg1, arg2);
}
#endif /* CONFIG_USERSPACE */

/* Export Flash parameters for use by core building scripts */
__attribute__((retain)) const uintptr_t sketch_base_addr =
	DT_REG_ADDR(DT_GPARENT(DT_NODELABEL(user_sketch))) + DT_REG_ADDR(DT_NODELABEL(user_sketch));
__attribute__((retain)) const uintptr_t sketch_max_size = DT_REG_SIZE(DT_NODELABEL(user_sketch));

/* Determine maximum size of the loader application */
#if DT_HAS_FIXED_PARTITION_LABEL(image_0) /* "image_0" partition size */
#define LOADER_MAX_SIZE DT_REG_SIZE(DT_NODE_BY_FIXED_PARTITION_LABEL(image_0))
#elif CONFIG_FLASH_LOAD_SIZE > 0 /* forced value from Kconfig */
#define LOADER_MAX_SIZE CONFIG_FLASH_LOAD_SIZE
#elif CONFIG_FLASH_LOAD_OFFSET /* heuristic: size of Flash minus load offset */
#define LOADER_MAX_SIZE (DT_REG_SIZE(DT_NODELABEL(flash0)) - CONFIG_FLASH_LOAD_OFFSET)
#else /* default: size of whole Flash */
#define LOADER_MAX_SIZE DT_REG_SIZE(DT_NODELABEL(flash0))
#endif
__attribute__((retain)) const uintptr_t loader_max_size = LOADER_MAX_SIZE;

static int loader(const struct shell *sh) {
	const struct flash_area *fa;
	int rc;

	/* Test that attempting to open a disabled flash area fails */
	rc = flash_area_open(FIXED_PARTITION_ID(user_sketch), &fa);
	if (rc) {
		printk("Failed to open flash area, rc %d\n", rc);
		return rc;
	}

	uintptr_t base_addr =
		DT_REG_ADDR(DT_GPARENT(DT_NODELABEL(user_sketch))) + DT_REG_ADDR(DT_NODELABEL(user_sketch));

	char header[HEADER_LEN];
	rc = flash_area_read(fa, 0, header, sizeof(header));
	if (rc) {
		printk("Failed to read header, rc %d\n", rc);
		return rc;
	}

	bool sketch_valid = true;
	struct sketch_header_v1 *sketch_hdr = (struct sketch_header_v1 *)(header + 7);
	if (sketch_hdr->ver != 0x1 || sketch_hdr->magic != 0x2341) {
		printk("Invalid sketch header\n");
		sketch_valid = false;
		// This is not a valid sketch, but try to start a shell anyway
	}

#if TARGET_HAS_USB_CDC
	int debug = (!sketch_valid) || (sketch_hdr->flags & SKETCH_FLAG_DEBUG);
#if CONFIG_SHELL
	if (strcmp(k_thread_name_get(k_current_get()), "main") == 0) {
		// disables default shell on UART
		shell_uninit(shell_backend_uart_get_ptr(), NULL);
		// enables USB and starts the shell
		usb_enable(NULL);
		int dtr;
		do {
			// wait for the serial port to open
			uart_line_ctrl_get(usb_dev, UART_LINE_CTRL_DTR, &dtr);
			k_sleep(K_MSEC(100));
		} while (!dtr);
		enable_shell_usb();
	}
#elif CONFIG_LOG
#if !CONFIG_USB_DEVICE_INITIALIZE_AT_BOOT
	if (debug) {
		usb_enable(NULL);
	}
#endif
	for (int i = 0; i < log_backend_count_get(); i++) {
		const struct log_backend *backend;
		backend = log_backend_get(i);
		log_backend_init(backend);
		log_backend_enable(backend, backend->cb->ctx, CONFIG_LOG_DEFAULT_LEVEL);
		if (!debug) {
			break;
		}
	}
#endif
#endif

#if defined(CONFIG_BOARD_ARDUINO_UNO_Q)
	void matrixBegin(void);
	void matrixEnd(void);
	void matrixPlay(uint8_t *buf, uint32_t len);
	void matrixSetGrayscaleBits(uint8_t _max);
	void matrixGrayscaleWrite(uint8_t *buf);
#include "bootanimation.h"

	uint8_t *_bootanimation = (uint8_t *)bootanimation;
	size_t _bootanimation_len = bootanimation_len;
	uint8_t *_bootanimation_end = (uint8_t *)bootanimation_end;
	size_t _bootanimation_end_len = bootanimation_end_len;

	__attribute__((packed)) struct bootanimation_user_data {
		size_t magic; // must be 0xBA for bootanimation
		size_t len_loop;
		size_t len_end;
		size_t empty;
		char buf_loop;
	};

	uintptr_t bootanimation_addr = DT_REG_ADDR(DT_GPARENT(DT_NODELABEL(bootanimation))) +
								   DT_REG_ADDR(DT_NODELABEL(bootanimation));

	struct bootanimation_user_data *user_bootanimation =
		(struct bootanimation_user_data *)bootanimation_addr;
	if (user_bootanimation->magic == 0xBA) {
		_bootanimation = &(user_bootanimation->buf_loop);
		_bootanimation_len = user_bootanimation->len_loop;
		_bootanimation_end_len = user_bootanimation->len_end;
		_bootanimation_end = _bootanimation + user_bootanimation->len_loop;
	}

	if ((!sketch_valid) || !(sketch_hdr->flags & SKETCH_FLAG_IMMEDIATE)) {
		// Start the bootanimation while waiting for the MPU to boot
		const struct gpio_dt_spec spec =
			GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), control_gpios, 0);

		gpio_pin_configure_dt(&spec, GPIO_INPUT | GPIO_PULL_DOWN);
		k_sleep(K_MSEC(200));
		uint8_t *ram_firmware = NULL;
		uint32_t *ram_start = (uint32_t *)0x20000000;
		if (!sketch_valid) {
			ram_firmware = (uint8_t *)malloc(SKETCH_RAM_BUFFER_LEN);
			if (!ram_firmware) {
				printk("Failed to allocate RAM for firmware\n");
				return -ENOMEM;
			}
			memset(ram_firmware, 0, SKETCH_RAM_BUFFER_LEN);
			*ram_start = (uint32_t)&ram_firmware[0];
		}
		if (gpio_pin_get_dt(&spec) == 0) {
			matrixBegin();
			matrixSetGrayscaleBits(8);
			while (gpio_pin_get_dt(&spec) == 0) {
				matrixPlay(_bootanimation, _bootanimation_len);
			}
			matrixPlay(_bootanimation_end, _bootanimation_end_len);
			uint8_t _framebuffer[104] = {0};
			matrixGrayscaleWrite(_framebuffer);
			k_sleep(K_MSEC(10));
			matrixEnd();
		}
		while (!sketch_valid) {
			__asm__("bkpt");
			// poll the first bytes, if filled try to use them for booting
			sketch_hdr = (struct sketch_header_v1 *)(ram_firmware + 7);
			if (sketch_hdr->ver == 0x1 && sketch_hdr->magic == 0x2341) {
				// Found valid data, use it for booting
				base_addr = (uintptr_t)ram_firmware;
				*ram_start = 0;
				sketch_valid = true;
			}
		}
	}
#endif

	size_t sketch_buf_len = sketch_hdr->len;

	if (sketch_hdr->flags & SKETCH_FLAG_LINKED) {
#ifdef CONFIG_BOARD_ARDUINO_PORTENTA_C33
#if CONFIG_MPU
		barrier_dmem_fence_full();
#endif
#if CONFIG_DCACHE
		barrier_dsync_fence_full();
#endif
#if CONFIG_ICACHE
		barrier_isync_fence_full();
#endif
#endif

		extern struct k_heap llext_heap;
		typedef void (*entry_point_t)(struct k_heap *heap, size_t heap_size);
		entry_point_t entry_point = (entry_point_t)(base_addr + HEADER_LEN + 1);
		entry_point(&llext_heap, llext_heap.heap.init_bytes);
		// should never reach here
		for (;;) {
			k_sleep(K_FOREVER);
		}
	}

#if defined(CONFIG_LLEXT_STORAGE_WRITABLE)
	uint8_t *sketch_buf = k_aligned_alloc(4096, sketch_buf_len);

	if (!sketch_buf) {
		printk("Unable to allocate %d bytes\n", sketch_buf_len);
		return -ENOMEM;
	}

	rc = flash_area_read(fa, 0, sketch_buf, sketch_buf_len);
	if (rc) {
		printk("Failed to read sketch area, rc %d\n", rc);
		return rc;
	}
#else
	// Assuming the sketch is stored in the same flash device as the loader
	uint8_t *sketch_buf = (uint8_t *)base_addr;
#endif

#ifdef CONFIG_LLEXT
	struct llext_buf_loader buf_loader = LLEXT_BUF_LOADER(sketch_buf, sketch_buf_len);
	struct llext_loader *ldr = &buf_loader.loader;

	LOG_HEXDUMP_DBG(sketch_buf, 4, "4 byte MAGIC");

	struct llext_load_param ldr_parm = LLEXT_LOAD_PARAM_DEFAULT;
	struct llext *ext;
	int res;

	res = llext_load(ldr, "sketch", &ext, &ldr_parm);
	if (res) {
		printk("Failed to load sketch, rc %d\n", res);
		return res;
	}

	void (*main_fn)() = llext_find_sym(&ext->exp_tab, "main");
	if (!main_fn) {
		printk("Failed to find main function\n");
		return -ENOENT;
	}
#endif

#ifdef CONFIG_USERSPACE
	/*
	 * Due to the number of MPU regions on some parts with MPU (USERSPACE)
	 * enabled we need to always call into the extension from a new dedicated
	 * thread to avoid running out of MPU regions on some parts.
	 *
	 * This is part dependent behavior and certainly on MMU capable parts
	 * this should not be needed! This test however is here to be generic
	 * across as many parts as possible.
	 */
	struct k_mem_domain domain;

	k_mem_domain_init(&domain, 0, NULL);

#ifdef Z_LIBC_PARTITION_EXISTS
	k_mem_domain_add_partition(&domain, &z_libc_partition);
#endif

	res = llext_add_domain(ext, &domain);
	if (res == -ENOSPC) {
		printk("Too many memory partitions for this particular hardware\n");
		return -1;
	}

	k_thread_create(&llext_thread, llext_stack, K_THREAD_STACK_SIZEOF(llext_stack), &llext_entry,
					llext_bootstrap, ext, main_fn, 1, K_INHERIT_PERMS, K_FOREVER);

	k_mem_domain_add_thread(&domain, &llext_thread);

	k_thread_start(&llext_thread);
	k_thread_join(&llext_thread, K_FOREVER);
#else

#ifdef CONFIG_LLEXT
	llext_bootstrap(ext, main_fn, NULL);
#endif

#endif

	return 0;
}

#if CONFIG_SHELL
SHELL_CMD_REGISTER(sketch, NULL, "Run sketch", loader);
#endif

int main(void) {
	loader(NULL);
	return 0;
}
