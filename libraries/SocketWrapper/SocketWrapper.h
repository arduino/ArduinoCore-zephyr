/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "Arduino.h"
#include "zephyr/sys/printk.h"
#if defined(CONFIG_FILE_SYSTEM)
#include <zephyr/fs/fs.h>
#endif

#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
#include <zephyr/net/tls_credentials.h>
#define CA_CERTIFICATE_TAG_BASE 1
#endif

#include <zephyr/net/socket.h>
#include <memory>
#include <cstring>
#include <arpa/inet.h>
#include <netdb.h>

class ZephyrSocketWrapper {
protected:
	std::shared_ptr<int> sock_fd;
	bool is_ssl = false;
	int ssl_sock_temp_char = -1;

#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS) && defined(CONFIG_FILE_SYSTEM)
	inline static char *cadata = nullptr;
	inline static size_t cadata_len = 0;

	bool loadCADataFromFS(const char *cert_path = "/wlan:/cacert.pem") {
		struct fs_file_t file;
		fs_file_t_init(&file);

		if (fs_open(&file, cert_path, FS_O_READ) != 0) {
			return false;
		}

		// Get file size
		struct fs_dirent entry;
		if (fs_stat(cert_path, &entry) != 0) {
			fs_close(&file);
			return false;
		}

		size_t file_size = entry.size;

		// The new CA bundle (the one uploaded via the FlashFormat sketch) is
		// around 21 KB and fits in RAM. The old bundle shipped with the mbed
		// core is much larger and won't fit. Warn the user if the file looks
		// too big so they know they need to upload the new bundle.
		if (file_size > 32 * 1024 && Serial) {
			Serial.print("WARNING: ");
			Serial.print(cert_path);
			Serial.print(" is ");
			Serial.print((unsigned int)file_size);
			Serial.println(" bytes, which is larger than expected.");
			Serial.println("It looks like the board still has the old CA bundle (the one");
			Serial.println("that comes with the mbed core) loaded. That bundle is too big");
			Serial.println("and won't fit in RAM, so TLS will not work.");
			Serial.println("Please upload the new bundle using the FlashFormat sketch.");
		}

		// Allocate buffer for entire file (+1 for NULL terminator)
		cadata = (char *)malloc(file_size + 1);
		if (!cadata) {
			fs_close(&file);
			return false;
		}

		// Read entire file
		ssize_t bytes_read = fs_read(&file, cadata, file_size);
		fs_close(&file);

		if (bytes_read != (ssize_t)file_size) {
			free(cadata);
			cadata = nullptr;
			return false;
		}

		cadata[file_size] = '\0';
		cadata_len = file_size + 1;
		return true;
	}
#endif

	// custom deleter for shared_ptr to close automatically the socket
	static void socket_deleter(int *fd) {
		if (fd && *fd != -1) {
			::zsock_close(*fd);
		}
		delete fd;
	}

public:
	ZephyrSocketWrapper() = default;

	ZephyrSocketWrapper(int fd)
		: sock_fd(std::shared_ptr<int>(fd < 0 ? nullptr : new int(fd), socket_deleter)) {
	}

	~ZephyrSocketWrapper() = default; // socket close managed by shared_ptr

	bool connect(const char *host, uint16_t port) {

		// Resolve address
		struct addrinfo hints = {0};
		struct addrinfo *res = nullptr;
		bool rv = true;
		int raw_sock_fd;

		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;

		int resolve_attempts = 100;
		int ret;

		while (resolve_attempts--) {
			ret = getaddrinfo(host, String(port).c_str(), &hints, &res);

			if (ret == 0) {
				break;
			} else {
				k_sleep(K_MSEC(1));
			}
		}

		if (ret != 0) {
			rv = false;
			goto exit;
		}

		raw_sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		sock_fd =
			std::shared_ptr<int>(raw_sock_fd < 0 ? nullptr : new int(raw_sock_fd), socket_deleter);
		if (!sock_fd) {
			rv = false;

			goto exit;
		}

		if (::connect(*sock_fd, res->ai_addr, res->ai_addrlen) < 0) {
			sock_fd = nullptr;
			rv = false;
			goto exit;
		}

	exit:
		if (res != nullptr) {
			freeaddrinfo(res);
			res = nullptr;
		}

		return rv;
	}

	bool connect(IPAddress host, uint16_t port) {

		const String _host = host.toString();
		int raw_sock_fd;

		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		inet_pton(AF_INET, _host.c_str(), &addr.sin_addr);

		raw_sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		sock_fd =
			std::shared_ptr<int>(raw_sock_fd < 0 ? nullptr : new int(raw_sock_fd), socket_deleter);
		if (!sock_fd) {
			return false;
		}

		if (::connect(*sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			sock_fd = nullptr;
			return false;
		}

		return true;
	}

#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
	bool connectSSL(const char *host, uint16_t port, const char *cert = nullptr) {

		// Resolve address
		struct addrinfo hints = {0};
		struct addrinfo *res = nullptr;

		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;

		int resolve_attempts = 100;
		int ret;
		bool rv = false;
		int raw_sock_fd;

		sec_tag_t sec_tag_opt[2];
		int tag_count = 0;
		int tag = CA_CERTIFICATE_TAG_BASE;

		struct timeval timeout_opt = {
			.tv_sec = 0,
			.tv_usec = 100000,
		};

		while (resolve_attempts--) {
			ret = getaddrinfo(host, String(port).c_str(), &hints, &res);

			if (ret == 0) {
				break;
			} else {
				k_sleep(K_MSEC(1));
			}
		}

		if (ret != 0) {
			goto exit;
		}

#if defined(CONFIG_FILE_SYSTEM)
		// Try to load builtin CA from filesystem (once)
		if (cadata == nullptr && loadCADataFromFS()) {
			// Successfully loaded, add with tag (ignore errors)
			if (tls_credential_add(tag++, TLS_CREDENTIAL_CA_CERTIFICATE, cadata, cadata_len)) {
				goto exit;
			}
		}
#endif

		// Add custom CA if provided (uses next available tag)
		if (cert != nullptr) {
			if (tls_credential_add(tag++, TLS_CREDENTIAL_CA_CERTIFICATE, cert, strlen(cert) + 1) !=
				0) {
				goto exit;
			}
		}

		// Build sequential tag list
		tag_count = tag - CA_CERTIFICATE_TAG_BASE;
		for (int i = 0; i < tag_count; i++) {
			sec_tag_opt[i] = CA_CERTIFICATE_TAG_BASE + i;
		}

		raw_sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TLS_1_2);
		sock_fd =
			std::shared_ptr<int>(raw_sock_fd < 0 ? nullptr : new int(raw_sock_fd), socket_deleter);
		if (!sock_fd) {
			goto exit;
		}

		if (setsockopt(*sock_fd, SOL_TLS, TLS_HOSTNAME, host, strlen(host)) ||
			setsockopt(*sock_fd, SOL_TLS, TLS_SEC_TAG_LIST, sec_tag_opt, sizeof(sec_tag_opt)) ||
			setsockopt(*sock_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout_opt, sizeof(timeout_opt))) {
			goto exit;
		}

		if (::connect(*sock_fd, res->ai_addr, res->ai_addrlen) < 0) {
			goto exit;
		}

		rv = true;
		is_ssl = true;

	exit:
		if (res != nullptr) {
			freeaddrinfo(res);
			res = nullptr;
		}

		if (!rv && *sock_fd >= 0) {
			sock_fd = nullptr;
		}
		return rv;
	}
#endif

	int available() {
		int count = 0;
		if (is_ssl) {
			/*
				TODO: HACK:
				The correct colution would be to call
				::recv(sock_fd, &ssl_sock_temp_char, 1, MSG_PEEK | MSG_DONTWAIT);
				but it doesn't seem to work. Instead, save a temporary variable
				and use it in read()
			*/
			if (ssl_sock_temp_char != -1) {
				return 1;
			}
			count = ::recv(*sock_fd, &ssl_sock_temp_char, 1, MSG_DONTWAIT);
		} else {
			zsock_ioctl(*sock_fd, ZFD_IOCTL_FIONREAD, &count);
		}
		if (count <= 0) {
			delay(1);
			count = 0;
		}
		return count;
	}

	int recv(uint8_t *buffer, size_t size, int flags = MSG_DONTWAIT) {
		if (sock_fd == nullptr || *sock_fd == -1) {
			return -1;
		}

		// TODO: see available()
		if (ssl_sock_temp_char != -1) {
			int ret = ::recv(*sock_fd, &buffer[1], size - 1, flags);
			buffer[0] = ssl_sock_temp_char;
			ssl_sock_temp_char = -1;
			return ret + 1;
		}
		return ::recv(*sock_fd, buffer, size, flags);
	}

	int send(const uint8_t *buffer, size_t size) {
		if (sock_fd == nullptr || *sock_fd == -1) {
			return -1;
		}

		return ::send(*sock_fd, buffer, size, 0);
	}

	void close() {
		if (sock_fd) {
			sock_fd = nullptr;
		}
	}

	bool bind(uint16_t port) {
		struct sockaddr_in addr;
		int raw_sock_fd;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		addr.sin_addr.s_addr = INADDR_ANY;

		raw_sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		sock_fd =
			std::shared_ptr<int>(raw_sock_fd < 0 ? nullptr : new int(raw_sock_fd), socket_deleter);
		if (!sock_fd) {
			return false;
		}

		zsock_ioctl(*sock_fd, ZFD_IOCTL_FIONBIO);

		if (::bind(*sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			sock_fd = nullptr;
			return false;
		}

		return true;
	}

	bool listen(int backlog = 5) {
		if (sock_fd == nullptr || *sock_fd == -1) {
			return false;
		}

		if (::listen(*sock_fd, backlog) < 0) {
			sock_fd = nullptr;
			return false;
		}

		return true;
	}

	int accept() {
		if (sock_fd == nullptr || *sock_fd == -1) {
			return -1;
		}

		return ::accept(*sock_fd, nullptr, nullptr);
	}

	String remoteIP() {
		if (sock_fd == nullptr || *sock_fd == -1) {
			return {};
		}

		struct sockaddr_storage addr;
		socklen_t addr_len = sizeof(addr);
		char ip_str[INET6_ADDRSTRLEN] = {0};

		if (::getpeername(*sock_fd, (struct sockaddr *)&addr, &addr_len) == 0) {
			if (addr.ss_family == AF_INET) {
				struct sockaddr_in *s = (struct sockaddr_in *)&addr;
				::inet_ntop(AF_INET, &s->sin_addr, ip_str, sizeof(ip_str));
			} else if (addr.ss_family == AF_INET6) {
				struct sockaddr_in6 *s6 = (struct sockaddr_in6 *)&addr;
				::inet_ntop(AF_INET6, &s6->sin6_addr, ip_str, sizeof(ip_str));
			}
			return String(ip_str);
		}

		return {};
	}
	friend class ZephyrClient;
};
