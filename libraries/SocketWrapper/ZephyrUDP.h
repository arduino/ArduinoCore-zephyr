/*
  ZephyrUDP.h - UDP implementation using Zephyr Sockets
  Copyright (c) Arduino s.r.l. and/or its affiliated companies
  SPDX-License-Identifier: LGPL-2.1-or-later

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#pragma once

#include "SocketWrapper.h"
#include "api/Udp.h"

#include <list>
#include <deque>
#include <vector>
#include <memory>

#define UDP_TX_PACKET_MAX_SIZE 24

class ZephyrUDP : public arduino::UDP {
private:
	int _socket;

public:
	ZephyrUDP();
	~ZephyrUDP();

	// initialize, start listening on specified port. Returns 1 if successful, 0 if there are no
	// sockets available to use
	uint8_t begin(uint16_t port) override;

	// initialize, start listening on specified multicast IP address and port. Returns 1 if
	// successful, 0 if there are no sockets available to use
	uint8_t beginMulticast(IPAddress ip, uint16_t port) override;

	// Finish with the UDP socket
	void stop() override;

	// Sending UDP packets

	// Start building up a packet to send to the remote host specific in ip and port
	// Returns 1 if successful, 0 if there was a problem with the supplied IP address or port
	int beginPacket(IPAddress ip, uint16_t port) override;

	// Start building up a packet to send to the remote host specific in host and port
	// Returns 1 if successful, 0 if there was a problem resolving the hostname or port
	int beginPacket(const char *host, uint16_t port) override;

	// Finish off this packet and send it
	// Returns 1 if the packet was sent successfully, 0 if there was an error
	int endPacket() override;

	// Write a single byte into the packet
	size_t write(uint8_t data) override;

	// Write size bytes from buffer into the packet
	size_t write(const uint8_t *buffer, size_t size) override;

	using Print::write;

	int parsePacket() override;

	int available() override;

	int read() override;

	int read(unsigned char *buffer, size_t len) override;

	int read(char *buffer, size_t len) override;

	int peek() override;

	void flush() override;

	IPAddress remoteIP() override;

	uint16_t remotePort() override;

private:
	/* UDP TRANSMISSION */
	IPAddress _send_to_ip;
	uint16_t _send_to_port;
	std::vector<uint8_t> _tx_data;
	size_t _rx_pkt_list_size = 10;

	/* UDP RECEPTION */
	class UdpRxPacket {
	private:
		IPAddress const _remote_ip;
		uint16_t const _remote_port;
		size_t const _rx_data_len;
		std::deque<uint8_t> _rx_data;

	public:
		UdpRxPacket(IPAddress const remote_ip, uint16_t const remote_port, uint8_t const *p_data,
					size_t const data_len);

		typedef std::shared_ptr<UdpRxPacket> SharedPtr;

		IPAddress remoteIP() const;

		uint16_t remotePort() const;

		size_t totalSize() const;

		int available();

		int read();

		int read(unsigned char *buffer, size_t len);
		int read(char *buffer, size_t len);

		int peek();
	};

	std::list<UdpRxPacket::SharedPtr> _rx_pkt_list;
	UdpRxPacket::SharedPtr _rx_pkt;
};
