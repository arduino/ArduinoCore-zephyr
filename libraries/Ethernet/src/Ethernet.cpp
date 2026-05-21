/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "Ethernet.h"

#if DT_HAS_COMPAT_STATUS_OKAY(ethernet_phy)

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
	if (!net_if_is_up(netif)) {
		net_if_up(netif);
	}
	return 1;
}

EthernetLinkStatus EthernetClass::linkStatus() {
	if ((hardwareStatus() == EthernetOk) && net_if_is_up(netif)) {
		return LinkON;
	}
	return LinkOFF;
}

EthernetHardwareStatus EthernetClass::hardwareStatus() {
	if (netif == nullptr) {
		netif = net_if_get_first_ethernet();
	}

	if (netif == nullptr) {
		return EthernetNoHardware;
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
