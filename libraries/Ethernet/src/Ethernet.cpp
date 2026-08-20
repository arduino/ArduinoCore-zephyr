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

#if DT_HAS_COMPAT_STATUS_OKAY(ethernet_phy) || DT_HAS_COMPAT_STATUS_OKAY(arduino_eth_phy)
static inline int init_eth_clock() {
	if (!DT_HAS_CHOSEN(arduino_eth_clock)) {
		return 0;
	}

	int ret = 0;
	static const struct device *eth_clk_dev = DEVICE_DT_GET_OR_NULL(DT_CHOSEN(arduino_eth_clock));
	static const struct pwm_dt_spec eth_pwm = PWM_DT_SPEC_GET_OR(DT_CHOSEN(arduino_eth_clock), {});

	if (!device_is_ready(eth_clk_dev)) {
		ret = zephyr::arduino::init_pwm_ref_clock(eth_clk_dev, eth_pwm);
	}

	return ret;
}

int EthernetClass::begin(uint8_t *mac, unsigned long timeout, unsigned long responseTimeout) {
	(void)timeout;
	(void)responseTimeout;
	if (hardwareStatus() != EthernetOk) {
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
	if (hardwareStatus() != EthernetOk) {
		return 0;
	}
	setMACAddress(mac);
	config(ip, dns, gateway, subnet);
	return 1;
}

EthernetLinkStatus EthernetClass::linkStatus() {
	if (hardwareStatus() == EthernetOk) {
		if (net_if_is_up(netif)) {
			return LinkON;
		} else {
			return LinkOFF;
		}
	}

	return LinkOFF;
}

EthernetHardwareStatus EthernetClass::hardwareStatus() {
	int ret = 0;

	if (netif == nullptr) {
		netif = net_if_get_first_ethernet();
	}

	if (netif == nullptr) {
		return EthernetNoHardware;
	}

	/* performing ethernet devices initialization here, because we need it to check the hw
	 * presence and status of the link. Internally they are performed only once, if
	 * device_is_ready returns false. NOTE, eth_clock device could be set as a dependency of
	 * netif device
	 * */
	ret = init_eth_clock();
	if (ret < 0) {
		return EthernetNoHardware;
	}

	if (!device_is_ready(net_if_get_device(netif))) {
		ret = zephyr::arduino::init_dev_apply_pinctrl(net_if_get_device(netif));
		if (ret < 0) {
			return EthernetNoHardware;
		}
	}

	if (!net_if_is_up(netif)) {
		net_if_up(netif);
	}

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
