/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once
#include <ZephyrUDP.h>

#if defined(CONFIG_NET_L2_ETHERNET)
using EthernetUDP = ZephyrUDP;
#endif
