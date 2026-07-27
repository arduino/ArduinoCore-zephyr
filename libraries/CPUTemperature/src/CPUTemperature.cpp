/*
 * Copyright (c) 2026 KurtE
 * SPDX-License-Identifier: Apache-2.0
 */

/* This code is derived from the Zephyr sampple/sensor/die_temp_polling */

/*
 * Copyright (c) 2023 TOKITA Hiroshi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <Arduino.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#if defined(CONFIG_SENSOR) && defined(CONFIG_STM32_TEMP)
#define DIE_TEMP_ALIAS(i) DT_ALIAS(_CONCAT(die_temp, i))
#define DIE_TEMPERATURE_SENSOR(i, _)                                                               \
	IF_ENABLED(DT_NODE_EXISTS(DIE_TEMP_ALIAS(i)), (DEVICE_DT_GET(DIE_TEMP_ALIAS(i)), ))

/* support up to 16 cpu die temperature sensors */
static const struct device *const sensors[] = {LISTIFY(16, DIE_TEMPERATURE_SENSOR, ()) };

double CPUTemperature(uint8_t sensor_index = 0) {
	if (sensor_index >= ARRAY_SIZE(sensors)) {
		return 0.0 / 0.0;
	}
	const struct device *dev = sensors[sensor_index];

	/* Re-apply DEFAULT pinctrl state so shared pins
	 * are remuxed back to SPI after other peripherals have used them.
	 */
	(void)zephyr::arduino::init_dev_apply_pinctrl(dev);

	struct sensor_value val;
	int rc;

	// printk("get_die_temperature: %p\n", dev);
	/* fetch sensor samples */
	rc = sensor_sample_fetch(dev);
	// printk("sensor_sample_fetch: %d\n", rc);
	if (rc) {
		return 0.0 / 0.0;
	}

	rc = sensor_channel_get(dev, SENSOR_CHAN_DIE_TEMP, &val);
	if (rc) {
		return 0.0 / 0.0;
	}

	return sensor_value_to_double(&val);
}
#else
#warning "Current board does not support die_temp"

double CPUTemperature(uint8_t sensor_index = 0) {
	return 0.0 / 0.0;
}

#endif
