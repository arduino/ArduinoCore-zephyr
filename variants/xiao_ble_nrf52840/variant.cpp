/*
 * Copyright (c) 2026 Seeed Technology Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Arduino.h"
#include <hal/nrf_power.h>

void _on_1200_bps() {
	/* Reboot into the factory Adafruit bootloader, CDC (serial DFU) only.
	 * Value is DFU_MAGIC_SERIAL_ONLY_RESET from the bootloader source. */
	nrf_power_gpregret_set(NRF_POWER, 0, 0x4e);
	NVIC_SystemReset();
}
