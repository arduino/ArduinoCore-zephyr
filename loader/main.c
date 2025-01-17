/*
 * Copyright (C) 2024 Arduino SA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "zephyr/sys/printk.h"
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app);

#include <zephyr/kernel.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/llext/llext.h>
#include <zephyr/llext/buf_loader.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>

#include <stdlib.h>
#include <zephyr/drivers/uart/cdc_acm.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>

#if DT_NODE_HAS_PROP(DT_PATH(zephyr_user), cdc_acm)
const struct device *const usb_dev = DEVICE_DT_GET(DT_PHANDLE_BY_IDX(DT_PATH(zephyr_user), cdc_acm, 0));
#endif

static int enable_shell_usb(void);

#ifdef CONFIG_USERSPACE
K_THREAD_STACK_DEFINE(llext_stack, CONFIG_MAIN_STACK_SIZE);
struct k_thread llext_thread;

void llext_entry(void *arg0, void *arg1, void *arg2)
{
	void (*fn)(struct llext_loader*, struct llext*) = arg0;
	fn(arg1, arg2);
}
#endif /* CONFIG_USERSPACE */

static int loader(const struct shell *sh)
{
	const struct flash_area *fa;
	int rc;

	/* Test that attempting to open a disabled flash area fails */
	rc = flash_area_open(FIXED_PARTITION_ID(user_sketch), &fa);
	if (rc) {
		printk("Failed to open flash area, rc %d\n", rc);
		return rc;
	}

	char header[16];
	rc = flash_area_read(fa, 0, header, sizeof(header));
	if (rc) {
		printk("Failed to read header, rc %d\n", rc);
		return rc;
	}

	if (!header[0] || header[0] == 0xff) {
		printk("No sketch found\n");
		return -ENOENT;
	}

	char *endptr;
	size_t sketch_buf_len = strtoul(header, &endptr, 10);
	// make sure number got parsed correctly"
	if (header == endptr) {
		printk("Failed to parse sketch size\n");
		return -EINVAL;
	}

#if DT_NODE_HAS_PROP(DT_PATH(zephyr_user), cdc_acm) && CONFIG_SHELL
	uint8_t debug = endptr[1];
	if (debug != 0 && strcmp(k_thread_name_get(k_current_get()), "main") == 0) {
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
		return 0;
	}
#endif

	int header_len = 16;

#if defined(CONFIG_LLEXT_STORAGE_WRITABLE)
	uint8_t* sketch_buf = k_aligned_alloc(4096, sketch_buf_len);

	if (!sketch_buf) {
		printk("Unable to allocate %d bytes\n", sketch_buf_len);
	}

	rc = flash_area_read(fa, header_len, sketch_buf, sketch_buf_len);
	if (rc) {
		printk("Failed to read sketch area, rc %d\n", rc);
		return rc;
	}
#else
	// Assuming the sketch is stored in the same flash device as the loader
	// uint32_t offset =  DT_REG_ADDR(DT_CHOSEN(zephyr_flash)) + DT_REG_ADDR(DT_NODELABEL(user_sketch));
	uint32_t offset =  DT_REG_ADDR(DT_GPARENT(DT_NODELABEL(user_sketch))) + DT_REG_ADDR(DT_NODELABEL(user_sketch));
	uint8_t* sketch_buf = (uint8_t*)(offset+header_len);
#endif

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

	k_thread_create(&llext_thread, llext_stack,
			K_THREAD_STACK_SIZEOF(llext_stack),
			&llext_entry, llext_bootstrap, ext, main_fn,
			1, K_INHERIT_PERMS, K_FOREVER);

	k_mem_domain_add_thread(&domain, &llext_thread);

	k_thread_start(&llext_thread);
	k_thread_join(&llext_thread, K_FOREVER);
#else

	llext_bootstrap(ext, main_fn, NULL);

#endif

	return 0;
}

#if CONFIG_SHELL
SHELL_CMD_REGISTER(sketch, NULL, "Run sketch", loader);

#if DT_NODE_HAS_PROP(DT_PATH(zephyr_user), cdc_acm)
static int enable_shell_usb(void)
{
	bool log_backend = CONFIG_SHELL_BACKEND_SERIAL_LOG_LEVEL > 0;
	uint32_t level =
		(CONFIG_SHELL_BACKEND_SERIAL_LOG_LEVEL > LOG_LEVEL_DBG) ?
		CONFIG_LOG_MAX_LEVEL : CONFIG_SHELL_BACKEND_SERIAL_LOG_LEVEL;
	static const struct shell_backend_config_flags cfg_flags =
					SHELL_DEFAULT_BACKEND_CONFIG_FLAGS;

	shell_init(shell_backend_uart_get_ptr(), usb_dev, cfg_flags, log_backend, level);

	return 0;
}
#endif
#endif

int main(void)
{
	loader(NULL);
	return 0;
}