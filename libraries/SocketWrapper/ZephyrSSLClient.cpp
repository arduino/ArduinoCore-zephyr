/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <ZephyrSSLClient.h>
#if defined(ARDUINO_MANAGES_MBEDTLS)
#include "mbedtls/memory_buffer_alloc.h"
#if defined(CONFIG_MBEDTLS_DEBUG_LEVEL)
#include <mbedtls/debug.h>
#endif
#if defined(CONFIG_MBEDTLS_PSA_CRYPTO_CLIENT)
#include <psa/crypto.h>
#endif
#endif

#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)

#if defined(ARDUINO_MANAGES_MBEDTLS)
static unsigned char arduino_mbedtls_heap[CONFIG_MBEDTLS_HEAP_SIZE] __aligned(8);

static int arduino_mbedtls_init() {
	mbedtls_memory_buffer_alloc_init(arduino_mbedtls_heap, sizeof(arduino_mbedtls_heap));

#if defined(CONFIG_MBEDTLS_DEBUG_LEVEL)
	mbedtls_debug_set_threshold(CONFIG_MBEDTLS_DEBUG_LEVEL);
#endif

#if defined(CONFIG_MBEDTLS_PSA_CRYPTO_CLIENT)
	if (psa_crypto_init() != PSA_SUCCESS) {
		return -EIO;
	}
#endif

	return 0;
}
#endif

ZephyrSSLClient::ZephyrSSLClient() {
#if defined(ARDUINO_MANAGES_MBEDTLS)
	static int mbedtls_init_status = arduino_mbedtls_init();

	mbedtls_is_ready = (mbedtls_init_status == 0);
#endif
}

#endif
