/*
 * Copyright (c) 2026 Seeed Studio
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Arduino.h"
#include <hal/nrf_power.h>

void _on_1200_bps() {
    nrf_power_gpregret_set(NRF_POWER, 0, 0xb0);
    NVIC_SystemReset();
}