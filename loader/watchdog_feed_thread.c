
#ifdef CONFIG_APP_WATCHDOG_ENFORCED
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/logging/log.h>
#if defined(CONFIG_LOADER_TRIAL_WINDOW_MS) && (CONFIG_LOADER_TRIAL_WINDOW_MS > 0)
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/sys/reboot.h>
#endif
LOG_MODULE_REGISTER(watchdog_feeding);

/* Only compile this thread if Sysbuild injected the security flags */

const struct device *const wdt_dev = DEVICE_DT_GET(DT_ALIAS(watchdog0));
static int wdt_channel_id;

void watchdog_feed_thread(void *p1, void *p2, void *p3) {
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	if (!device_is_ready(wdt_dev)) {
		LOG_ERR("Watchdog device not ready.\n");
		return;
	}

	struct wdt_timeout_cfg wdt_config = {
		.window = {.min = 0,
				   /* Use the exact timing synchronized with MCUBoot */
				   .max = CONFIG_APP_WATCHDOG_TIMEOUT_MS},
		.callback = NULL,
		.flags = WDT_FLAG_RESET_SOC};

	/*
	 * Register the software channel in the application's RAM.
	 * We purposefully DO NOT call wdt_setup() here because
	 * MCUBoot already started the hardware timer.
	 */
	wdt_channel_id = wdt_install_timeout(wdt_dev, &wdt_config);
	if (wdt_channel_id < 0) {
		LOG_ERR("Watchdog install error\n");
		return;
	}

	/* Automatically calculate a safe feed interval */
	uint32_t feed_interval = CONFIG_APP_WATCHDOG_TIMEOUT_MS / 4;

#if defined(CONFIG_LOADER_TRIAL_WINDOW_MS) && (CONFIG_LOADER_TRIAL_WINDOW_MS > 0)
	/* A test image (MCUboot swap modes) must call ota_confirm() in time, or
	 * MCUboot swaps the previous image back on the reboot. */
	const int64_t deadline = k_uptime_get() + CONFIG_LOADER_TRIAL_WINDOW_MS;

	while (!boot_is_img_confirmed()) {
		if (k_uptime_get() >= deadline) {
			LOG_WRN("Test image not confirmed, reverting");
			sys_reboot(SYS_REBOOT_WARM);
		}
		wdt_feed(wdt_dev, wdt_channel_id);
		k_msleep(MIN(feed_interval, 1000U));
	}
#endif

	while (1) {
		wdt_feed(wdt_dev, wdt_channel_id);
		k_msleep(feed_interval);
	}
}

/*
 * Statically allocate and start the thread at boot with lowest priority.
 */
K_THREAD_DEFINE(wdt_thread_id, 1024, watchdog_feed_thread, NULL, NULL, NULL,
				K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);

#endif /* CONFIG_APP_WATCHDOG_ENFORCED */

