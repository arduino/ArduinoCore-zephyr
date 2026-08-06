/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "SocketWrapper.h"
#include "ZephyrClient.h"
#include "api/Server.h"
#include "unistd.h"

class ZephyrServer : public arduino::Server, ZephyrSocketWrapper {
private:
	int _port;

public:
	ZephyrServer();
	ZephyrServer(uint16_t port);

	virtual ~ZephyrServer();

	void end();
	void begin(uint16_t port);
	void begin();

	uint8_t status();

	explicit operator bool();

	ZephyrClient accept(uint8_t *status = nullptr);
	ZephyrClient available(uint8_t *status = nullptr) __attribute__((deprecated("Use accept().")));
	size_t write(uint8_t c) override;
	size_t write(const uint8_t *buffer, size_t size) override;

	friend class ZephyrClient;
};
