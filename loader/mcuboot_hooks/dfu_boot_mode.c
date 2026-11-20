/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * USB DFU on a retention boot mode request, on top of upstream MCUboot hooks.
 *
 * The loader's 1200-bps touch and the board's double tap set the bootloader
 * boot mode. MCUboot then waits in USB DFU without a timeout, feeding the
 * watchdog (upstream boot_usb_dfu_wait()), and installs the download in the
 * same boot. Without a bootable image MCUboot reboots into the same mode.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/retention/bootmode.h>
#include <zephyr/sys/reboot.h>

#include "bootutil/bootutil.h"
#include "bootutil/bootutil_log.h"
#include "bootutil/boot_hooks.h"
#include "bootutil/fault_injection_hardening.h"
#include "bootutil/mcuboot_status.h"
#include "usbd_dfu.h"

BOOT_LOG_MODULE_DECLARE(mcuboot);

/*
 * Read and clear the request once. It is cleared even when boot_go_hook()
 * skips DFU for a pending update: that swap must run, and a request left set
 * would send a later, unrelated reset into DFU.
 */
static bool dfu_requested(void)
{
	static int requested = -1;

	if (requested < 0) {
		requested = bootmode_check(BOOT_MODE_TYPE_BOOTLOADER) == 1;
		/* Other values (the loader's own DFU mode) are for the image. */
		if (requested && bootmode_clear() != 0) {
			BOOT_LOG_ERR("Unable to clear boot mode");
		}
	}

	return requested;
}

#if defined(CONFIG_ARDUINO_MCUBOOT_DFU_FADE_LED)
#include <zephyr/drivers/pwm.h>

#define FADE_DELAY_MS 10
#define FADE_STEPS    50

static const struct pwm_dt_spec fade_led = PWM_DT_SPEC_GET(DT_ALIAS(mcuboot_fade_led0));
static K_THREAD_STACK_DEFINE(fade_led_stack, 1024);
static struct k_thread fade_led_thread;
static atomic_t fade_led_running;

static void fade_led_loop(void *p1, void *p2, void *p3)
{
	const uint32_t step = fade_led.period / FADE_STEPS;
	uint32_t pulse = 0;
	bool rising = true;

	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (atomic_get(&fade_led_running)) {
		if (pwm_set_pulse_dt(&fade_led, pulse) < 0) {
			break;
		}
		if (rising) {
			pulse += step;
			if (pulse >= fade_led.period) {
				pulse = fade_led.period;
				rising = false;
			}
		} else {
			pulse = pulse > step ? pulse - step : 0;
			rising = pulse == 0;
		}
		k_msleep(FADE_DELAY_MS);
	}

	(void)pwm_set_pulse_dt(&fade_led, 0);
}

static void fade_led_start(void)
{
	if (!pwm_is_ready_dt(&fade_led)) {
		BOOT_LOG_ERR("PWM device %s is not ready", fade_led.dev->name);
		return;
	}
	atomic_set(&fade_led_running, 1);
	k_thread_create(&fade_led_thread, fade_led_stack, K_THREAD_STACK_SIZEOF(fade_led_stack),
			fade_led_loop, NULL, NULL, NULL, 1, 0, K_NO_WAIT);
}

static void fade_led_stop(void)
{
	if (atomic_clear(&fade_led_running)) {
		k_thread_join(&fade_led_thread, K_FOREVER);
	}
}
#else
static inline void fade_led_start(void) {}
static inline void fade_led_stop(void) {}
#endif

fih_ret boot_go_hook(struct boot_rsp *rsp)
{
	int rc;

	ARG_UNUSED(rsp);

	if (dfu_requested()) {
		/* Like upstream's GPIO entrance: never touch an interrupted swap. */
		if (boot_check_for_updates()) {
			BOOT_LOG_INF("Update in progress, USB DFU skipped");
			FIH_RET(FIH_BOOT_HOOK_REGULAR);
		}
		BOOT_LOG_INF("Boot mode requests USB DFU");
		fade_led_start();
		rc = boot_usb_dfu_enable();
		if (rc != 0) {
			BOOT_LOG_ERR("Cannot enable USB: %d", rc);
		} else {
			boot_usb_dfu_wait(K_FOREVER);
		}
		fade_led_stop();
	}

	/* boot_go() installs the download, if any. */
	FIH_RET(FIH_BOOT_HOOK_REGULAR);
}

void mcuboot_status_change(mcuboot_status_type_t status)
{
	/* Enter DFU through boot_go_hook(), which installs the download in the
	 * same boot (upstream NO_APPLICATION panics after it).
	 */
	if (status == MCUBOOT_STATUS_NO_BOOTABLE_IMAGE_FOUND &&
	    bootmode_set(BOOT_MODE_TYPE_BOOTLOADER) == 0) {
		sys_reboot(SYS_REBOOT_WARM);
	}
}
