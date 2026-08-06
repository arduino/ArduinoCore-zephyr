/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "ZephyrServer.h"

ZephyrServer::ZephyrServer() : _port(80) {};
ZephyrServer::ZephyrServer(uint16_t port) : _port(port) {};

ZephyrServer::~ZephyrServer() {
	end();
}

void ZephyrServer::end() {
	ZephyrSocketWrapper::close();
}

void ZephyrServer::begin(uint16_t port) {
	_port = port;
	begin();
}

void ZephyrServer::begin() {
	ZephyrSocketWrapper::bind(_port);
	ZephyrSocketWrapper::listen(5);
}

uint8_t ZephyrServer::status() {
	return 0;
}

ZephyrServer::operator bool() {
	return (sock_fd != nullptr);
}

ZephyrClient ZephyrServer::accept(uint8_t *status) {
	ARG_UNUSED(status);

	ZephyrClient client;
	int sock = ZephyrSocketWrapper::accept();
	client.setSocket(sock);
	return client;
}

ZephyrClient ZephyrServer::available(uint8_t *status) {
	return accept(status);
}

size_t ZephyrServer::write(uint8_t c) {
	return write(&c, 1);
}

size_t ZephyrServer::write(const uint8_t *buffer, size_t size) {
	return send(buffer, size);
}
