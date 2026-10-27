#ifdef CONFIG_USE_NETWORK_LED_TRACKER

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_stats.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(network_leds);

static const struct gpio_dt_spec eth_led = GPIO_DT_SPEC_GET(DT_ALIAS(led_ethernet), gpios);
static const struct gpio_dt_spec wifi_led = GPIO_DT_SPEC_GET(DT_ALIAS(led_wifi), gpios);

struct iface_activity_tracker {
	struct net_if *iface;
	const struct gpio_dt_spec *led;
	uint32_t last_tx;
	uint32_t last_rx;
};

static struct iface_activity_tracker eth_tracker = {.led = &eth_led};
static struct iface_activity_tracker wifi_tracker = {.led = &wifi_led};

static void poll_net_activity_handler(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(net_poll_work, poll_net_activity_handler);

/* Helper function to check and update a single interface */
static void update_iface_led(struct iface_activity_tracker *trk) {
	struct net_stats stats;
	bool activity = false;

	/* Use NET_REQUEST_STATS_GET_ALL instead of GET_IFACE */
	if (trk->iface && net_mgmt(NET_REQUEST_STATS_GET_ALL, trk->iface, &stats, sizeof(stats)) == 0) {
		if ((stats.bytes.sent > trk->last_tx) || (stats.bytes.received > trk->last_rx)) {
			activity = true;
			trk->last_tx = stats.bytes.sent;
			trk->last_rx = stats.bytes.received;
		}
	}

	gpio_pin_set_dt(trk->led, activity ? 1 : 0);
}

static void poll_net_activity_handler(struct k_work *work) {
	/* Update both LEDs based on their respective interface stats */
	update_iface_led(&eth_tracker);
	update_iface_led(&wifi_tracker);

	/* Reschedule to run again in 100ms */
	k_work_reschedule(&net_poll_work, K_MSEC(100));
}

void start_network_led_monitor(void) {
	/* Find interfaces by their Layer 2 type */

	// eth_tracker.iface = net_if_get_first_by_type(&NET_L2_GET_NAME(ETHERNET));
	// wifi_tracker.iface = net_if_get_first_by_type(&NET_L2_GET_NAME(WIFI));

	eth_tracker.iface = net_if_get_first_ethernet();
	wifi_tracker.iface = net_if_get_wifi_sta();

	if (eth_tracker.iface) {
		LOG_INF("led tracker: ethernet if found\n");
	} else {
		LOG_WRN("led tracked: ethernet if not found\n");
	}

	if (wifi_tracker.iface) {
		LOG_INF("led tracker: wifi if found\n");
	} else {
		LOG_WRN("led tracker: wifi if not found\n");
	}

	/* Initialize Ethernet LED */
	if (device_is_ready(eth_led.port)) {
		gpio_pin_configure_dt(&eth_led, GPIO_OUTPUT_INACTIVE);
	}

	/* Initialize Wi-Fi LED */
	if (device_is_ready(wifi_led.port)) {
		gpio_pin_configure_dt(&wifi_led, GPIO_OUTPUT_INACTIVE);
	}

	/* Kick off the infinite polling loop */
	k_work_schedule(&net_poll_work, K_NO_WAIT);
}

#endif

