/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "Arduino.h"
#include <memory>
#include <zephyr/net/socket.h>

class ZephyrSocketWrapper {
protected:
	std::shared_ptr<int> sock_fd;
	bool is_ssl = false;
	int ssl_sock_temp_char = -1;

#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS) && defined(CONFIG_FILE_SYSTEM)
	inline static char *cadata = nullptr;
	inline static size_t cadata_len = 0;

	bool loadCADataFromFS(const char *cert_path = "/wlan:/cacert.pem");
#endif

	virtual void setSocket(int sock);

public:
	ZephyrSocketWrapper() = default;
	ZephyrSocketWrapper(int fd);
	~ZephyrSocketWrapper() = default; // socket close managed by shared_ptr

	bool connect(const char *host, uint16_t port);
	bool connect(IPAddress host, uint16_t port);

#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
	bool connectSSL(const char *host, uint16_t port, const char *cert = nullptr);
#endif

	int available();
	int recv(uint8_t *buffer, size_t size, int flags = MSG_DONTWAIT);
	int send(const uint8_t *buffer, size_t size);
	void close();

	bool bind(uint16_t port);
	bool listen(int backlog = 5);
	int accept();

	String remoteIP();
	friend class ZephyrClient;
};
