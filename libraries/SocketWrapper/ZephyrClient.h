/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "SocketWrapper.h"
#include "api/Client.h"
#include "unistd.h"
#include "zephyr/sys/printk.h"

class ZephyrClient : public arduino::Client, ZephyrSocketWrapper {
private:
	bool _connected = false;

protected:
	void setSocket(int sock) {
		sock_fd = std::shared_ptr<int>(sock < 0 ? nullptr : new int(sock), socket_deleter);
		_connected = true;
	}

public:
	int connect(const char *host, uint16_t port) override {
		auto ret = ZephyrSocketWrapper::connect((char *)host, port);
		if (ret) {
			_connected = true;
		}
		return ret;
	}

	int connect(IPAddress ip, uint16_t port) {
		auto ret = ZephyrSocketWrapper::connect(ip, port);
		if (ret) {
			_connected = true;
		}
		return ret;
	}
#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
	int connectSSL(const char *host, uint16_t port, const char *cert) {
		auto ret = ZephyrSocketWrapper::connectSSL(host, port, cert);
		if (ret) {
			_connected = true;
		}
		return ret;
	}
#endif
	uint8_t connected() override {
		uint8_t buf;
		int ret = ::recv(*sock_fd, &buf, 1, MSG_PEEK | MSG_DONTWAIT);
		if (ret == 0) {
			stop();
		}
		return _connected;
	}

	int available() override {
		return ZephyrSocketWrapper::available();
	}

	int read() override {
		uint8_t c;
		read(&c, 1);
		return c;
	}

	int read(uint8_t *buffer, size_t size) override {
		auto received = recv(buffer, size);

		if (received <= 0) {
			return 0;
		}
		return received;
	}

	size_t write(uint8_t c) override {
		return write(&c, 1);
	}

	size_t write(const uint8_t *buffer, size_t size) override {
		return send(buffer, size);
	}

	void flush() override {
		// No-op
	}

	int peek() override {
		uint8_t c;
		recv(&c, 1, MSG_PEEK);
		return c;
	}

	void stop() override {
		ZephyrSocketWrapper::close();
		_connected = false;
	}

	operator bool() {
		return (sock_fd != nullptr);
	}

	String remoteIP() {
		return ZephyrSocketWrapper::remoteIP();
	}
	friend class ZephyrServer;
};
