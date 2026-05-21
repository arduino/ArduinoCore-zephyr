/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef CONFIG_DAC

void analogWrite(enum dacPins pinNumber, int value);

#endif

// In c++ mode, we also provide analogReadResolution and analogWriteResolution getters
#ifdef CONFIG_ADC

int analogReadResolution();

#endif

#if defined(CONFIG_PWM) || defined(CONFIG_DAC)

int analogWriteResolution();

#endif
