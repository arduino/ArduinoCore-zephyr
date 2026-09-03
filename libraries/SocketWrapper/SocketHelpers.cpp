/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "SocketHelpers.h"
#include "zephyr/net/net_if.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(sketch, LOG_LEVEL_NONE);

int NetworkInterface::dhcp() {
	// If dhcp was never started on this interface, start it, otherwise restart it, so that we force
	// to ask the dhcp server for a new lease
	if (netif->config.dhcpv4.state == NET_DHCPV4_DISABLED) {
		net_dhcpv4_start(netif);
	} else {
		net_dhcpv4_restart(netif);
	}

	LOG_INF("DHCPv4 started...\n");

	return 0;
}

void NetworkInterface::enable_dhcpv4_server(struct net_if *netif, const char *_netmask) {
	static struct in_addr addr;
	static struct in_addr netmaskAddr;

	if (net_addr_pton(AF_INET, String(localIP()).c_str(), &addr)) {
		LOG_ERR("Invalid address: %s", String(localIP()).c_str());
		return;
	}

	if (net_addr_pton(AF_INET, _netmask, &netmaskAddr)) {
		LOG_ERR("Invalid netmask: %s", _netmask);
		return;
	}

	net_if_ipv4_set_gw(netif, &addr);

	if (net_if_ipv4_addr_add(netif, &addr, NET_ADDR_MANUAL, 0) == NULL) {
		LOG_ERR("unable to set IP address for AP interface");
	}

	if (!net_if_ipv4_set_netmask_by_addr(netif, &addr, &netmaskAddr)) {
		LOG_ERR("Unable to set netmask for AP interface: %s", _netmask);
	}

	addr.s4_addr[3] += 10; /* Starting IPv4 address for DHCPv4 address pool. */

	if (net_dhcpv4_server_start(netif, &addr) != 0) {
		LOG_ERR("DHCP server is not started for desired IP");
		return;
	}

	LOG_INF("DHCPv4 server started...\n");
}

void NetworkInterface::MACAddress(uint8_t *mac) {
	if (netif == NULL || netif->if_dev == NULL || mac == NULL) {
		return;
	}
	memcpy(mac, netif->if_dev->link_addr.addr, netif->if_dev->link_addr.len);
}

IPAddress NetworkInterface::localIP() {
	return IPAddress(netif->config.ip.ipv4->unicast[0].ipv4.address.in_addr.s_addr);
}

IPAddress NetworkInterface::subnetMask() {
	return IPAddress(netif->config.ip.ipv4->unicast[0].netmask.s_addr);
}

IPAddress NetworkInterface::gatewayIP() {
	return IPAddress(netif->config.ip.ipv4->gw.s_addr);
}

IPAddress NetworkInterface::dnsServerIP() {
	return arduino::INADDR_NONE;
}

void NetworkInterface::setMACAddress(const uint8_t *mac) {
	struct ethernet_req_params params = {0};

	if (mac == nullptr) {
		LOG_ERR("MAC address pointer is null");
		return;
	}
	memcpy(&params.mac_address, mac, sizeof(struct net_eth_addr));
	if (!net_eth_is_addr_valid(&params.mac_address)) {
		LOG_ERR("Invalid MAC address");
		return;
	}

	net_if_down(netif); // Ensure the interface is down before changing the MAC address

	int ret = net_mgmt(NET_REQUEST_ETHERNET_SET_MAC_ADDRESS, netif, &params, sizeof(params));
	if (ret != 0) {
		LOG_ERR("Failed to set MAC address via net_mgmt, ret=%d", ret);
	} else {
		LOG_INF("MAC address set successfully via net_mgmt");
	}

	net_if_up(netif); // Bring the interface back up after changing the MAC address
}

int NetworkInterface::begin(bool blocking, uint64_t additional_event_mask) {
	if (!net_if_is_up(netif)) {
		net_if_up(netif);
	}

	dhcp();

	// FIXME: additional_event_mask cannot be ORed here due to how Zephyr
	// events are handled internally. Must be reworked to wait on a sem
	// and register multiple event masks with event_handler instead.
	ARG_UNUSED(additional_event_mask);

	int ret = 0;
	if (blocking && netif->config.dhcpv4.state != NET_DHCPV4_BOUND) {
		ret = net_mgmt_event_wait_on_iface(netif, NET_EVENT_IPV4_DHCP_BOUND, NULL, NULL, NULL,
										   K_SECONDS(10));
	}
	return (ret == 0) ? 1 : 0;
}

void NetworkInterface::end() {
	disconnect();
}

bool NetworkInterface::disconnect() {
	return (net_if_down(netif) == 0);
}

void NetworkInterface::config(const IPAddress ip, const IPAddress dns_server,
							  const IPAddress gateway, const IPAddress subnet) {
	setLocalIP(ip);
	setDnsServerIP(dns_server);
	setGatewayIP(gateway);
	setSubnetMask(subnet);
}

void NetworkInterface::setLocalIP(const IPAddress ip) {
	local_ip.s_addr = ip;

	if (!net_if_ipv4_addr_add(netif, &local_ip, NET_ADDR_MANUAL, 0)) {
		LOG_ERR("Failed to set local IP address");
		return;
	}
	LOG_INF("Local IP address set: %s", ip.toString().c_str());
}

void NetworkInterface::setSubnetMask(const IPAddress subnet) {
	struct in_addr netmask_addr;
	netmask_addr.s_addr = subnet;
	net_if_ipv4_set_netmask_by_addr(netif, &local_ip, &netmask_addr);
	LOG_INF("Subnet mask set: %s", subnet.toString().c_str());
	return;
}

void NetworkInterface::setGatewayIP(const IPAddress gateway) {
	struct in_addr gw_addr;
	gw_addr.s_addr = gateway;
	net_if_ipv4_set_gw(netif, &gw_addr);
	LOG_INF("Gateway IP set: %s", gateway.toString().c_str());
}

void NetworkInterface::setDnsServerIP(const IPAddress dns_server) {
	// DNS server dynamic configuration is not supported
	ARG_UNUSED(dns_server);
}
