/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * The handoff word between a running image (loader or sketch) and MCUboot.
 *
 * MCUboot enters serial recovery mode only when it finds this cookie set at
 * PRE_KERNEL_1 - normally because the user physically double-tapped reset.
 * Writing the same magic from software and resetting is therefore
 * indistinguishable from a double-tap, and is how a 1200-bps touch reaches
 * MCUboot serial recovery.
 *
 * The storage is a reserved-memory region declared in recovery_cookie.dtsi,
 * which every image sees. Deriving the address from devicetree is what keeps
 * the two sides from drifting apart.
 *
 * The region sits outside .bss and .data, so it survives a warm reset but not a
 * power cycle - exactly the lifetime wanted.
 */

#pragma once

#include <zephyr/devicetree.h>
#include <zephyr/sys/util.h>

/* "DFRU" - a random cookie value, kept for compatibility with MCUboot. */
#define RECOVERY_DOUBLE_RESET_MAGIC 0x44524655U

#define RECOVERY_COOKIE_ADDR DT_REG_ADDR(DT_NODELABEL(recovery_cookie))

#define RECOVERY_COOKIE (*(volatile uint32_t *)RECOVERY_COOKIE_ADDR)
