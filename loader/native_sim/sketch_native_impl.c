/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Embedded side of the native sketch loader: --sketch option and exit(). */

#include <stdbool.h>
#include <stddef.h>

#include <zephyr/kernel.h>
#include <cmdline.h>
#include <posix_board_if.h>
#include <posix_native_task.h>

#include "sketch_native_host.h"

static char *sketch_path;
static bool sketch_loaded;

static void sketch_native_options(void) {
	static struct args_struct_t sketch_options[] = {
		{
			.option = "sketch",
			.name = "path",
			.type = 's',
			.dest = (void *)&sketch_path,
			.descript = "Static sketch (.bin-zsk.bin) to load and run",
		},
		ARG_TABLE_ENDMARKER,
	};

	native_add_command_line_opts(sketch_options);
}

static void sketch_native_load(void) {
	if (sketch_path == NULL) {
		return;
	}

	if (sketch_native_map(sketch_path, CONFIG_ARDUINO_NATIVE_SKETCH_BASE,
						  CONFIG_ARDUINO_NATIVE_SKETCH_MAX_SIZE) != 0) {
		posix_exit(1);
	}

	sketch_loaded = true;
}

NATIVE_TASK(sketch_native_options, PRE_BOOT_1, 1);
NATIVE_TASK(sketch_native_load, PRE_BOOT_2, 1);

bool native_sketch_loaded(void) {
	return sketch_loaded;
}

/* Replaces the picolibc stub, which spins forever, to end the process */
void _exit(int status) {
	posix_exit(status);
}
