#pragma once

#include "zephyr/sys/printk.h"
#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
#include <zephyr/net/tls_credentials.h>
#define CA_CERTIFICATE_TAG 1
#endif

#include <zephyr/net/socket.h>

#include <api/Common.h>
#include <api/IPAddress.h>

class ZephyrSocketWrapper {
protected:
    int sock_fd;
    bool is_ssl = false;
    int ssl_sock_temp_char = -1;

public:
    ZephyrSocketWrapper() : sock_fd(-1) {}
    ZephyrSocketWrapper(int sock_fd) : sock_fd(sock_fd) {}

    ~ZephyrSocketWrapper() {
        if (sock_fd != -1) {
            ::close(sock_fd);
        }
    }

    bool connect(const char* host, uint16_t port) {

        // Resolve address
        struct addrinfo hints;
	    struct addrinfo *res;
        bool rv = true;

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

        sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock_fd < 0) {
            rv = false;

            goto exit;
        }

        if (::connect(sock_fd, res->ai_addr, res->ai_addrlen) < 0) {
            ::close(sock_fd);
            sock_fd = -1;
            rv =  false;
            goto exit;
        }

    exit:
        if(res != nullptr) {
            freeaddrinfo(res);
            res = nullptr;
        }

        return rv;
    }

    bool connect(IPAddress host, uint16_t port) {

        const char* _host = host.toString().c_str();

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, _host, &addr.sin_addr);

        sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock_fd < 0) {
            return false;
        }

        if (::connect(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            ::close(sock_fd);
            sock_fd = -1;
            return false;
        }

        return true;
    }

#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
    bool connectSSL(const char* host, uint16_t port, char* ca_certificate_pem = nullptr) {

        // Resolve address
        struct addrinfo hints;
        struct addrinfo *res;

        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        int resolve_attempts = 100;
        int ret;
        bool rv = true;

        sec_tag_t sec_tag_opt[] = {
            CA_CERTIFICATE_TAG,
        };

        uint32_t timeo_optval = 100;

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

        if (ca_certificate_pem != nullptr) {
            ret = tls_credential_add(CA_CERTIFICATE_TAG, TLS_CREDENTIAL_CA_CERTIFICATE,
                ca_certificate_pem, strlen(ca_certificate_pem) + 1);
        }

        sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TLS_1_2);
        if (sock_fd < 0) {
            rv = false;
            goto exit;
        }

        setsockopt(sock_fd, SOL_TLS, TLS_SEC_TAG_LIST,
                sec_tag_opt, sizeof(sec_tag_opt));

        setsockopt(sock_fd, SOL_TLS, TLS_HOSTNAME, host, strlen(host));

        setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &timeo_optval, sizeof(timeo_optval));

        if (::connect(sock_fd, res->ai_addr, res->ai_addrlen) < 0) {
            ::close(sock_fd);
            sock_fd = -1;
            rv = false;
            goto exit;
        }
        is_ssl = true;

    exit:
        if(res != nullptr) {
            freeaddrinfo(res);
            res = nullptr;
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
            count = ::recv(sock_fd, &ssl_sock_temp_char, 1, MSG_DONTWAIT);
        } else {
            zsock_ioctl(sock_fd, ZFD_IOCTL_FIONREAD, &count);
        }
        if (count <= 0) {
            delay(1);
            count = 0;
        }
        return count;
    }

    int recv(uint8_t* buffer, size_t size, int flags = MSG_DONTWAIT) {
        if (sock_fd == -1) {
            return -1;
        }
        // TODO: see available()
        if (ssl_sock_temp_char != -1) {
            int ret = ::recv(sock_fd, &buffer[1], size - 1, flags);
            buffer[0] = ssl_sock_temp_char;
            ssl_sock_temp_char = -1;
            return ret + 1;
        }
        return ::recv(sock_fd, buffer, size, flags);
    }

    int send(const uint8_t* buffer, size_t size) {
        if (sock_fd == -1) {
            return -1;
        }
        return ::send(sock_fd, buffer, size, 0);
    }

    void close() {
        if (sock_fd != -1) {
            ::close(sock_fd);
            sock_fd = -1;
        }
    }

    bool bind(uint16_t port) {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock_fd < 0) {
            return false;
        }

        zsock_ioctl(sock_fd, ZFD_IOCTL_FIONBIO);

        if (::bind(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            ::close(sock_fd);
            sock_fd = -1;
            return false;
        }

        return true;
    }

    bool listen(int backlog = 5) {
        if (sock_fd == -1) {
            return false;
        }

        if (::listen(sock_fd, backlog) < 0) {
            ::close(sock_fd);
            sock_fd = -1;
            return false;
        }

        return true;
    }

    int accept() {
        if (sock_fd == -1) {
            return -1;
        }

        return ::accept(sock_fd, nullptr, nullptr);
    }

    friend class ZephyrClient;
};
