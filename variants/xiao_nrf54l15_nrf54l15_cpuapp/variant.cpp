/*
 * Copyright (c) 2026 Seeed Technology Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Arduino.h"

/* No _on_1200_bps() override: this board has no serial-DFU bootloader.
 * Upload goes through the onboard SAMD11 CMSIS-DAP probe, so the weak
 * default in cores/arduino/USB.cpp is left in place.
 * The Sense sensor rails (IMU + PDM microphone) are powered at boot by
 * the board devicetree (imu-power-enable hog and pdm-imu-pwr regulator),
 * so no initVariant() is required either. */
