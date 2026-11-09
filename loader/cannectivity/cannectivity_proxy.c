/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "cannectivity_proxy.h"

#include "../../cores/arduino/zephyrPinctrl.h"

#include <cannectivity/usb/class/gs_usb.h>
#include <zephyr/init.h>

int cannectivity_led_init(void);
int cannectivity_timestamp_init(void);

#define CHANNELS_NODE         DT_NODELABEL(cannectivity)
#define FOREACH_CHANNEL(fn)   DT_FOREACH_CHILD_STATUS_OKAY(CHANNELS_NODE, fn)
#define CONTROLLER(node)      DT_PHANDLE(node, can_controller)
#define PHYSICAL_DEVICE(node) DEVICE_DT_GET(CONTROLLER(node)),
static const struct device *const physical_devices[] = {FOREACH_CHANNEL(PHYSICAL_DEVICE)};

enum client {
	CLIENT_USB,
	CLIENT_SKETCH,
	CLIENT_COUNT
};

struct request {
	can_mode_t mode;
	uint32_t bitrate;
	uint32_t bitrate_data;
};

struct rx_filter {
	const struct device *proxy;
	can_rx_callback_t callback;
	void *user_data;
	int id;
};

struct channel {
	/* can_get_mode() expects this at the start of proxy->data. */
	struct can_driver_data common;
	const struct device *physical;
	struct device proxy;
	struct device_state state;
	struct can_driver_api api;
	struct k_mutex lock;
	/* RX runs in interrupt context, outside the channel mutex. */
	struct k_spinlock rx_lock;
	/* Protected by rx_lock, like the sketch RX subscription. */
	bool usb_rx_enabled;
	/* Keep Linux timings so the first USB client can apply them unchanged. */
	struct can_timing usb_timing;
#ifdef CONFIG_CAN_FD_MODE
	struct can_timing usb_timing_data;
#endif
	struct request active_request;
	bool active[CLIENT_COUNT];
	can_rx_callback_t sketch_rx;
	void *sketch_user_data;
	struct rx_filter filters[2];
};

static struct channel channels[ARRAY_SIZE(physical_devices)];
static const struct device *usb_devices[ARRAY_SIZE(channels)];

static struct channel *find_channel(const struct device *dev) {
	for (size_t i = 0; i < ARRAY_SIZE(channels); i++) {
		if (channels[i].physical == dev) {
			return &channels[i];
		}
	}
	return NULL;
}

static bool compatible(const struct request *a, const struct request *b) {
	return a->mode == b->mode && a->bitrate == b->bitrate &&
		   ((a->mode & CAN_MODE_FD) == 0U || a->bitrate_data == b->bitrate_data);
}

/* Called with the channel mutex held. Configuration is staged until acquisition
 * so a failed second client's request never changes the running controller.
 */
static int acquire(struct channel *ch, enum client client, const struct request *request) {
	int ret;
	if (request->bitrate == 0U ||
		((request->mode & CAN_MODE_FD) != 0U && request->bitrate_data == 0U)) {
		return -EINVAL;
	}
	if (ch->active[CLIENT_USB] || ch->active[CLIENT_SKETCH]) {
		if (!compatible(request, &ch->active_request)) {
			return -EBUSY;
		}
	} else {
		ret = can_set_mode(ch->physical, request->mode);
		if (ret != 0) {
			return ret;
		}
		ret = client == CLIENT_USB ? can_set_timing(ch->physical, &ch->usb_timing) :
									 can_set_bitrate(ch->physical, request->bitrate);
		if (ret != 0) {
			return ret;
		}
#ifdef CONFIG_CAN_FD_MODE
		if ((request->mode & CAN_MODE_FD) != 0U) {
			ret = client == CLIENT_USB ? can_set_timing_data(ch->physical, &ch->usb_timing_data) :
										 can_set_bitrate_data(ch->physical, request->bitrate_data);
			if (ret != 0) {
				return ret;
			}
		}
#endif
		ret = can_start(ch->physical);
		if (ret != 0) {
			return ret;
		}
		ch->active_request = *request;
	}
	ch->active[client] = true;
	return 0;
}

static int release(struct channel *ch, enum client client) {
	if (!ch->active[client]) {
		return 0;
	}
	enum client other = client == CLIENT_USB ? CLIENT_SKETCH : CLIENT_USB;
	if (!ch->active[other]) {
		int ret = can_stop(ch->physical);
		if (ret != 0 && ret != -EALREADY) {
			return ret;
		}
	}
	ch->active[client] = false;
	return 0;
}

int proxy_can_init(const struct device *dev, can_mode_t mode, uint32_t bitrate,
				   uint32_t bitrate_data, can_rx_callback_t callback, void *user_data) {
	struct channel *ch = find_channel(dev);
	struct request request = {.mode = mode, .bitrate = bitrate, .bitrate_data = bitrate_data};
	int ret;
	if (ch == NULL) {
		return -ENODEV;
	}
	if (callback == NULL) {
		return -EINVAL;
	}
	k_mutex_lock(&ch->lock, K_FOREVER);
	if (ch->active[CLIENT_SKETCH] && ch->sketch_user_data != user_data) {
		ret = -EBUSY;
	} else {
		ret = acquire(ch, CLIENT_SKETCH, &request);
		if (ret == 0) {
			k_spinlock_key_t key = k_spin_lock(&ch->rx_lock);
			ch->sketch_user_data = user_data;
			ch->sketch_rx = callback;
			k_spin_unlock(&ch->rx_lock, key);
		}
	}
	k_mutex_unlock(&ch->lock);
	return ret;
}

int proxy_can_end(const struct device *dev, void *user_data) {
	struct channel *ch = find_channel(dev);
	int ret;
	if (ch == NULL) {
		return -ENODEV;
	}
	k_mutex_lock(&ch->lock, K_FOREVER);
	if (ch->active[CLIENT_SKETCH] && ch->sketch_user_data != user_data) {
		ret = -EBUSY;
	} else {
		ret = release(ch, CLIENT_SKETCH);
		if (ret == 0) {
			k_spinlock_key_t key = k_spin_lock(&ch->rx_lock);
			ch->sketch_rx = NULL;
			ch->sketch_user_data = NULL;
			k_spin_unlock(&ch->rx_lock, key);
		}
	}
	k_mutex_unlock(&ch->lock);
	return ret;
}

static uint32_t timing_bitrate(const struct can_timing *timing, uint32_t clock) {
	uint32_t divisor =
		timing->prescaler * (1U + timing->prop_seg + timing->phase_seg1 + timing->phase_seg2);
	return divisor != 0U ? clock / divisor : 0U;
}

static int proxy_start(const struct device *dev) {
	struct channel *ch = dev->data;
	uint32_t clock;
	int ret;
	k_mutex_lock(&ch->lock, K_FOREVER);
	if (!device_is_ready(ch->physical)) {
		ret = device_init(ch->physical);
		if (ret != 0) {
			goto out;
		}
	}
	/* Linux selects the standard pins unless a sketch already owns the routing. */
	if (!ch->active[CLIENT_SKETCH]) {
		ret = zephyr_init_dev_apply_pinctrl(ch->physical);
		if (ret != 0) {
			goto out;
		}
	}
	ret = can_get_core_clock(ch->physical, &clock);
	if (ret != 0) {
		goto out;
	}
	struct request request = {
		.mode = ch->common.mode,
		.bitrate = timing_bitrate(&ch->usb_timing, clock),
#ifdef CONFIG_CAN_FD_MODE
		.bitrate_data = timing_bitrate(&ch->usb_timing_data, clock),
#endif
	};
	ret = acquire(ch, CLIENT_USB, &request);
	if (ret == 0) {
		ch->common.started = true;
		k_spinlock_key_t key = k_spin_lock(&ch->rx_lock);
		ch->usb_rx_enabled = true;
		k_spin_unlock(&ch->rx_lock, key);
	}
out:
	k_mutex_unlock(&ch->lock);
	return ret;
}

static int proxy_stop(const struct device *dev) {
	struct channel *ch = dev->data;
	k_mutex_lock(&ch->lock, K_FOREVER);
	int ret = release(ch, CLIENT_USB);
	if (ret == 0) {
		ch->common.started = false;
		k_spinlock_key_t key = k_spin_lock(&ch->rx_lock);
		ch->usb_rx_enabled = false;
		k_spin_unlock(&ch->rx_lock, key);
	}
	k_mutex_unlock(&ch->lock);
	return ret;
}

static int proxy_set_mode(const struct device *dev, can_mode_t mode) {
	struct channel *ch = dev->data;
	can_mode_t caps;
	int ret = can_get_capabilities(ch->physical, &caps);
	if (ret != 0 || (mode & ~caps) != 0U) {
		return ret != 0 ? ret : -ENOTSUP;
	}
	k_mutex_lock(&ch->lock, K_FOREVER);
	ret = ch->active[CLIENT_USB] ? -EBUSY : 0;
	if (ret == 0) {
		ch->common.mode = mode;
	}
	k_mutex_unlock(&ch->lock);
	return ret;
}

static int stage_timing(const struct device *dev, const struct can_timing *timing, bool data) {
	struct channel *ch = dev->data;
	k_mutex_lock(&ch->lock, K_FOREVER);
	int ret = ch->active[CLIENT_USB] ? -EBUSY : 0;
	if (ret == 0) {
#ifdef CONFIG_CAN_FD_MODE
		if (data) {
			ch->usb_timing_data = *timing;
		} else
#endif
		{
			ch->usb_timing = *timing;
		}
	}
	ARG_UNUSED(data);
	k_mutex_unlock(&ch->lock);
	return ret;
}

static int proxy_set_timing(const struct device *dev, const struct can_timing *timing) {
	return stage_timing(dev, timing, false);
}
#ifdef CONFIG_CAN_FD_MODE
static int proxy_set_timing_data(const struct device *dev, const struct can_timing *timing) {
	return stage_timing(dev, timing, true);
}
#endif

static int proxy_state(const struct device *dev, enum can_state *state,
					   struct can_bus_err_cnt *err) {
	struct channel *ch = dev->data;
	return can_get_state(ch->physical, state, err);
}

static int proxy_send(const struct device *dev, const struct can_frame *frame, k_timeout_t timeout,
					  can_tx_callback_t callback, void *user_data) {
	struct channel *ch = dev->data;
	if (!ch->common.started) {
		return -ENETDOWN;
	}
	return can_send(ch->physical, frame, timeout, callback, user_data);
}

static void proxy_rx_callback(const struct device *dev, struct can_frame *frame, void *user_data) {
	struct rx_filter *filter = user_data;
	struct channel *ch = filter->proxy->data;
	/* Deliver to the sketch before USB buffering can fail. */
	k_spinlock_key_t key = k_spin_lock(&ch->rx_lock);
	if (ch->sketch_rx != NULL) {
		ch->sketch_rx(dev, frame, ch->sketch_user_data);
	}
	bool usb_rx_enabled = ch->usb_rx_enabled;
	k_spin_unlock(&ch->rx_lock, key);
	/* The sketch may keep the controller running while the USB client is stopped.
	 * An in-flight USB callback may still finish after proxy_stop().
	 */
	if (usb_rx_enabled) {
		filter->callback(filter->proxy, frame, filter->user_data);
	}
}

static int proxy_add_filter(const struct device *dev, can_rx_callback_t callback, void *user_data,
							const struct can_filter *filter) {
	struct channel *ch = dev->data;
	struct rx_filter *slot = &ch->filters[(filter->flags & CAN_FILTER_IDE) != 0U];
	/* These private proxies are only used by gs_usb's two accept-all filters. */
	if (slot->id >= 0) {
		return -ENOSPC;
	}
	slot->proxy = dev;
	slot->callback = callback;
	slot->user_data = user_data;
	int ret = can_add_rx_filter(ch->physical, proxy_rx_callback, slot, filter);
	if (ret >= 0) {
		slot->id = ret;
	}
	return ret;
}

static void proxy_remove_filter(const struct device *dev, int id) {
	struct channel *ch = dev->data;
	for (size_t i = 0; i < ARRAY_SIZE(ch->filters); i++) {
		if (ch->filters[i].id == id) {
			can_remove_rx_filter(ch->physical, id);
			ch->filters[i].id = -1;
			return;
		}
	}
}

static void proxy_state_callback(const struct device *dev, enum can_state state,
								 struct can_bus_err_cnt err, void *user_data) {
	struct channel *ch = user_data;
	ARG_UNUSED(dev);
	if (ch->common.state_change_cb != NULL) {
		ch->common.state_change_cb(&ch->proxy, state, err, ch->common.state_change_cb_user_data);
	}
}

static void proxy_set_state_callback(const struct device *dev, can_state_change_callback_t callback,
									 void *user_data) {
	struct channel *ch = dev->data;
	ch->common.state_change_cb_user_data = user_data;
	ch->common.state_change_cb = callback;
	can_set_state_change_callback(ch->physical, proxy_state_callback, ch);
}

static int cannectivity_channel_event(const struct device *dev, uint16_t ch,
									  enum gs_usb_event event, void *user_data) {
	if (ch >= ARRAY_SIZE(channels)) {
		return -EINVAL;
	}
#ifdef CONFIG_CANNECTIVITY_LED
	return cannectivity_led_event(dev, ch, event, user_data);
#else
	ARG_UNUSED(dev);
	ARG_UNUSED(event);
	ARG_UNUSED(user_data);
	return 0;
#endif
}

static const struct gs_usb_ops gs_usb_ops = {
#ifdef CONFIG_CANNECTIVITY_TIMESTAMP
	.timestamp = cannectivity_timestamp_get,
#endif
	.event = cannectivity_channel_event,
};

/* Create private proxy devices for gs_usb_register().
 * These fake devices are not registered for device_get_binding() or the CAN shell.
 */
static int proxy_can_register(void) {
	for (size_t i = 0; i < ARRAY_SIZE(channels); i++) {
		struct channel *ch = &channels[i];
		ch->physical = physical_devices[i];
		/* gs_usb_register() installs hardware filters before either client starts. */
		if (!device_is_ready(ch->physical)) {
			int ret = device_init(ch->physical);
			if (ret != 0) {
				return ret;
			}
		}
		k_mutex_init(&ch->lock);
		ch->api = *DEVICE_API_GET(can, ch->physical);
		ch->api.start = proxy_start;
		ch->api.stop = proxy_stop;
		ch->api.set_mode = proxy_set_mode;
		ch->api.set_timing = proxy_set_timing;
		ch->api.send = proxy_send;
		ch->api.add_rx_filter = proxy_add_filter;
		ch->api.remove_rx_filter = proxy_remove_filter;
		ch->api.get_state = proxy_state;
		ch->api.set_state_change_callback = proxy_set_state_callback;
#ifdef CONFIG_CAN_FD_MODE
		if (ch->api.set_timing_data != NULL) {
			ch->api.set_timing_data = proxy_set_timing_data;
		}
#endif
		ch->state.initialized = true;
		ch->proxy = (struct device){.name = ch->physical->name,
									.config = ch->physical->config,
									.api = &ch->api,
									.state = &ch->state,
									.data = ch};
		ch->filters[0].id = -1;
		ch->filters[1].id = -1;
		usb_devices[i] = &ch->proxy;
	}
	int ret;
	if (IS_ENABLED(CONFIG_CANNECTIVITY_LED)) {
		ret = cannectivity_led_init();
		if (ret != 0) {
			return ret;
		}
	}
	if (IS_ENABLED(CONFIG_CANNECTIVITY_TIMESTAMP)) {
		ret = cannectivity_timestamp_init();
		if (ret != 0) {
			return ret;
		}
	}
	return gs_usb_register(DEVICE_DT_GET(DT_NODELABEL(gs_usb0)), usb_devices,
						   ARRAY_SIZE(usb_devices), &gs_usb_ops, NULL);
}

SYS_INIT(proxy_can_register, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
