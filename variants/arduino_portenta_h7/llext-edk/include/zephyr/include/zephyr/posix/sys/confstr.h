/*
 * Copyright (c) 2024, Meta
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_INCLUDE_POSIX_SYS_CONFSTR_H_
#define ZEPHYR_INCLUDE_POSIX_SYS_CONFSTR_H_

#ifdef __cplusplus
extern "C" {
#endif

enum {
	_CS_PATH,
	_CS_POSIX_V7_ILP32_OFF32_CFLAGS,
	_CS_POSIX_V7_ILP32_OFF32_LDFLAGS,
	_CS_POSIX_V7_ILP32_OFF32_LIBS,
	_CS_POSIX_V7_ILP32_OFFBIG_CFLAGS,
	_CS_POSIX_V7_ILP32_OFFBIG_LDFLAGS,
	_CS_POSIX_V7_ILP32_OFFBIG_LIBS,
	_CS_POSIX_V7_LP64_OFF64_CFLAGS,
	_CS_POSIX_V7_LP64_OFF64_LDFLAGS,
	_CS_POSIX_V7_LP64_OFF64_LIBS,
	_CS_POSIX_V7_LPBIG_OFFBIG_CFLAGS,
	_CS_POSIX_V7_LPBIG_OFFBIG_LDFLAGS,
	_CS_POSIX_V7_LPBIG_OFFBIG_LIBS,
	_CS_POSIX_V7_THREADS_CFLAGS,
	_CS_POSIX_V7_THREADS_LDFLAGS,
	_CS_POSIX_V7_WIDTH_RESTRICTED_ENVS,
	_CS_V7_ENV,
	_CS_POSIX_V6_ILP32_OFF32_CFLAGS,
	_CS_POSIX_V6_ILP32_OFF32_LDFLAGS,
	_CS_POSIX_V6_ILP32_OFF32_LIBS,
	_CS_POSIX_V6_ILP32_OFFBIG_CFLAGS,
	_CS_POSIX_V6_ILP32_OFFBIG_LDFLAGS,
	_CS_POSIX_V6_ILP32_OFFBIG_LIBS,
	_CS_POSIX_V6_LP64_OFF64_CFLAGS,
	_CS_POSIX_V6_LP64_OFF64_LDFLAGS,
	_CS_POSIX_V6_LP64_OFF64_LIBS,
	_CS_POSIX_V6_LPBIG_OFFBIG_CFLAGS,
	_CS_POSIX_V6_LPBIG_OFFBIG_LDFLAGS,
	_CS_POSIX_V6_LPBIG_OFFBIG_LIBS,
	_CS_POSIX_V6_WIDTH_RESTRICTED_ENVS,
	_CS_V6_ENV,
};

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_POSIX_SYS_CONFSTR_H_ */