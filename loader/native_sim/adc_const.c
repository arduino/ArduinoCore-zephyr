/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Give each channel of the emulated ADC a constant input, so that the
 * readings of the sketches are known: channel n reads 1650 + 100 * n mV
 * (A0 = 1650 mV, half of the 3300 mV reference).
 */

#include <zephyr/device.h>
#include <zephyr/drivers/adc/adc_emul.h>
#include <zephyr/init.h>

#define ADC_NODE DT_NODELABEL(adc0)

static int adc_const_init(void) {
	const struct device *dev = DEVICE_DT_GET(ADC_NODE);

	if (!device_is_ready(dev)) {
		return -ENODEV;
	}
	for (unsigned int chan = 0; chan < DT_PROP(ADC_NODE, nchannels); chan++) {
		(void)adc_emul_const_value_set(dev, chan, 1650 + 100 * chan);
	}

	return 0;
}

SYS_INIT(adc_const_init, APPLICATION, 99);
