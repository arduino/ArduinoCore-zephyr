/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Shared handoff word between a running image and MCUboot. Writing this magic
 * and resetting is indistinguishable from a physical double-tap, so MCUboot
 * enters serial recovery. Address derived from recovery_cookie.dtsi; the
 * reserved-memory region survives warm resets but not power cycles.
 */

#pragma once

#include <zephyr/devicetree.h>
#include <zephyr/sys/util.h>

/* "DFRU" - a random cookie value, kept for compatibility with MCUboot. */
#define RECOVERY_DOUBLE_RESET_MAGIC 0x44524655U

#define RECOVERY_COOKIE_ADDR DT_REG_ADDR(DT_NODELABEL(recovery_cookie))

#define RECOVERY_COOKIE (*(volatile uint32_t *)RECOVERY_COOKIE_ADDR)
