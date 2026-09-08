/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "SocketWrapper.h"
#include "api/Client.h"
#include "unistd.h"

class ZephyrClient : public arduino::Client, ZephyrSocketWrapper {
private:
	bool _connected = false;

protected:
	void setSocket(int sock) override;

public:
	int connect(const char *host, uint16_t port) override;
	int connect(IPAddress ip, uint16_t port);

#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
	int connectSSL(const char *host, uint16_t port, const char *cert);
#endif
	uint8_t connected() override;

	int available() override;
	int read() override;
	int read(uint8_t *buffer, size_t size) override;
	size_t write(uint8_t c) override;
	size_t write(const uint8_t *buffer, size_t size) override;

	void flush() override;

	int peek() override;
	void stop() override;

	operator bool();

	String remoteIP();
	friend class ZephyrServer;
};
