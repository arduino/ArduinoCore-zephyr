/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "ZephyrClient.h"
#include <errno.h>

#include <sys/socket.h>

void ZephyrClient::setSocket(int sock) {
	ZephyrSocketWrapper::setSocket(sock);
	_connected = true;
}

int ZephyrClient::connect(const char *host, uint16_t port) {
	auto ret = ZephyrSocketWrapper::connect((char *)host, port);
	if (ret) {
		_connected = true;
	}
	return ret;
}

int ZephyrClient::connect(IPAddress ip, uint16_t port) {
	auto ret = ZephyrSocketWrapper::connect(ip, port);
	if (ret) {
		_connected = true;
	}
	return ret;
}
#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
int ZephyrClient::connectSSL(const char *host, uint16_t port, const char *cert) {
	auto ret = ZephyrSocketWrapper::connectSSL(host, port, cert);
	if (ret) {
		_connected = true;
	}
	return ret;
}
#endif
uint8_t ZephyrClient::connected() {
	uint8_t buf;
	int ret = ::recv(*sock_fd, &buf, 1, MSG_PEEK | MSG_DONTWAIT);
	if (ret == 0) {
		stop();
	}
	return _connected;
}

int ZephyrClient::available() {
	return ZephyrSocketWrapper::available();
}

int ZephyrClient::read() {
	uint8_t c;
	read(&c, 1);
	return c;
}

int ZephyrClient::read(uint8_t *buffer, size_t size) {
	auto received = recv(buffer, size);

	if (received > 0) {
		return received;
	}
	if (received == 0) {
		return 0;
	}
	if (errno == EAGAIN || errno == EWOULDBLOCK) {
		return 0;
	}
	return -1;
}

size_t ZephyrClient::write(uint8_t c) {
	return write(&c, 1);
}

size_t ZephyrClient::write(const uint8_t *buffer, size_t size) {
	return send(buffer, size);
}

void ZephyrClient::flush() {
	/* No-op */
}

int ZephyrClient::peek() {
	uint8_t c;
	recv(&c, 1, MSG_PEEK);
	return c;
}

void ZephyrClient::stop() {
	ZephyrSocketWrapper::close();
	_connected = false;
}

ZephyrClient::operator bool() {
	return (sock_fd != nullptr);
}

String ZephyrClient::remoteIP() {
	return ZephyrSocketWrapper::remoteIP();
}
