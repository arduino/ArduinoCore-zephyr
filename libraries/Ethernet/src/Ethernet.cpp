/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "Ethernet.h"
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/pwm.h>
#include <zephyrClockInit.hpp>
#include <zephyrPinctrl.h>

#if DT_HAS_COMPAT_STATUS_OKAY(ethernet_phy)

static inline int init_eth_clock() {
#if DT_HAS_CHOSEN(arduino_eth_clock)
	static bool clock_enabled = false;
	static const struct device *eth_clk_dev = DEVICE_DT_GET_OR_NULL(DT_CHOSEN(arduino_eth_clock));
	static const struct pwm_dt_spec eth_pwm = PWM_DT_SPEC_GET_OR(DT_CHOSEN(arduino_eth_clock), {});

	if (clock_enabled) {
		return 0;
	}

	int ret = zephyr::arduino::init_dev_apply_pinctrl(eth_clk_dev);
	if (ret < 0) {
		return ret;
	}

	ret = zephyr::arduino::init_pwm_ref_clock(eth_clk_dev, eth_pwm);
	if (ret < 0) {
		return ret;
	}

	// Keep the clock running across retries if Ethernet device initialization fails.
	clock_enabled = true;
#endif
	return 0;
}

static int init_eth_hardware(struct net_if *&netif) {
	static bool initialized = false;

	if (netif == nullptr) {
		netif = net_if_get_first_ethernet();
	}
	if (netif == nullptr) {
		return -ENODEV;
	}
	if (initialized) {
		return 0;
	}

	int ret = init_eth_clock();
	if (ret < 0) {
		return ret;
	}

	const struct device *dev = net_if_get_device(netif);
	if (!device_is_ready(dev)) {
		ret = zephyr::arduino::init_dev_apply_pinctrl(dev);
		if (ret < 0) {
			return ret;
		}
	}
	if (!device_is_ready(dev)) {
		return -ENODEV;
	}

	initialized = true;
	return 0;
}

int EthernetClass::begin(uint8_t *mac, unsigned long timeout, unsigned long responseTimeout) {
	(void)timeout;
	(void)responseTimeout;
	if (init_eth_hardware(netif) < 0) {
		return 0;
	}
	setMACAddress(mac);
	return NetworkInterface::begin();
}

int EthernetClass::maintain() {
	return 0; // DHCP_CHECK_NONE
}

int EthernetClass::begin(uint8_t *mac, IPAddress ip) {
	IPAddress dns = ip;
	dns[3] = 1;

	auto ret = begin(mac, ip, dns);
	return ret;
}

int EthernetClass::begin(uint8_t *mac, IPAddress ip, IPAddress dns) {
	IPAddress gateway = ip;
	gateway[3] = 1;

	auto ret = begin(mac, ip, dns, gateway);
	return ret;
}

int EthernetClass::begin(uint8_t *mac, IPAddress ip, IPAddress dns, IPAddress gateway) {
	IPAddress subnet(255, 255, 255, 0);
	auto ret = begin(mac, ip, dns, gateway, subnet);
	return ret;
}

int EthernetClass::begin(uint8_t *mac, IPAddress ip, IPAddress dns, IPAddress gateway,
						 IPAddress subnet, unsigned long timeout, unsigned long responseTimeout) {
	(void)timeout;
	(void)responseTimeout;
	if (init_eth_hardware(netif) < 0) {
		return 0;
	}
	setMACAddress(mac);
	config(ip, dns, gateway, subnet);
	return 1;
}

EthernetLinkStatus EthernetClass::linkStatus() {
	// Initialize hardware on first use, then just read the carrier status at subsequent calls.
	if (init_eth_hardware(netif) < 0) {
		return LinkOFF;
	}

	if (net_if_is_carrier_ok(netif)) {
		return LinkON;
	}

	return LinkOFF;
}

EthernetHardwareStatus EthernetClass::hardwareStatus() {
	// Check device readiness without initializing hardware.
	struct net_if *iface = netif != nullptr ? netif : net_if_get_first_ethernet();
	if (iface == nullptr || !device_is_ready(net_if_get_device(iface))) {
		return EthernetNoHardware;
	}

#if DT_HAS_CHOSEN(arduino_eth_clock)
	if (!device_is_ready(DEVICE_DT_GET_OR_NULL(DT_CHOSEN(arduino_eth_clock)))) {
		return EthernetNoHardware;
	}
#endif

	return EthernetOk;
}

void EthernetClass::setRetransmissionTimeout(uint16_t milliseconds) {
	(void)milliseconds;
}

void EthernetClass::setRetransmissionCount(uint8_t num) {
	(void)num;
}

EthernetClass Ethernet;
#endif
