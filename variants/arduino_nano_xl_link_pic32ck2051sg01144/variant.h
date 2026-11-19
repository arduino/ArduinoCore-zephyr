/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#define LEDR (39u)
#define LEDG (40u)
#define LEDB (41u)

#define RS485_SERIAL_PORT Serial3

/* RS485.h derives RS485_DEFAULT_*_PIN from these. */
#define CUSTOM_RS485_DEFAULT_DE_PIN (42u)
#define CUSTOM_RS485_DEFAULT_RE_PIN (43u)
