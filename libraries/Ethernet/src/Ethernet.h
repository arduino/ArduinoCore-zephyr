/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <Arduino.h>
#include "SocketHelpers.h"
#include <zephyr/net/ethernet.h>
#include "ZephyrClient.h"

#if DT_HAS_COMPAT_STATUS_OKAY(ethernet_phy) || DT_HAS_COMPAT_STATUS_OKAY(arduino_eth_phy)

using EthernetClient = ZephyrClient;

enum EthernetLinkStatus {
	Unknown,
	LinkON,
	LinkOFF
};

enum EthernetHardwareStatus {
	EthernetNoHardware,
	EthernetOk
};

class EthernetClass : public NetworkInterface {
public:
	EthernetClass() {
	}

	virtual ~EthernetClass() {
	}

	int begin(uint8_t *mac = nullptr, unsigned long timeout = 60000,
			  unsigned long responseTimeout = 4000);
	int maintain();
	EthernetLinkStatus linkStatus();
	EthernetHardwareStatus hardwareStatus();

	// Manual configuration
	int begin(uint8_t *mac, IPAddress ip);
	int begin(uint8_t *mac, IPAddress ip, IPAddress dns);
	int begin(uint8_t *mac, IPAddress ip, IPAddress dns, IPAddress gateway);
	int begin(uint8_t *mac, IPAddress ip, IPAddress dns, IPAddress gateway, IPAddress subnet,
			  unsigned long timeout = 60000, unsigned long responseTimeout = 4000);

	int begin(IPAddress ip) {
		return begin(nullptr, ip);
	}

	int begin(IPAddress ip, IPAddress dns) {
		return begin(nullptr, ip, dns);
	}

	int begin(IPAddress ip, IPAddress dns, IPAddress gateway) {
		return begin(nullptr, ip, dns, gateway);
	}

	int begin(IPAddress ip, IPAddress dns, IPAddress gateway, IPAddress subnet) {
		return begin(nullptr, ip, dns, gateway, subnet);
	}

	void setRetransmissionTimeout(uint16_t milliseconds);
	void setRetransmissionCount(uint8_t num);
};

extern EthernetClass Ethernet;

#endif
