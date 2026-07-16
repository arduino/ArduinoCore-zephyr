/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once
#include <ZephyrUDP.h>

#if DT_NODE_HAS_STATUS(DT_NODELABEL(mdio), okay)
using EthernetUDP = ZephyrUDP;
#endif
