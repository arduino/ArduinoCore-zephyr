/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 * Copyright (c) 2022 TOKITA Hiroshi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <Arduino.h>
#include "wiring_private.h"

using namespace zephyr::arduino;

#define PWM_DT_SPEC(n, p, i) PWM_DT_SPEC_GET_BY_IDX(n, i),
#define PWM_PINS(n, p, i)                                                                          \
	DIGITAL_PIN_GPIOS_FIND_PIN(DT_REG_ADDR(DT_PHANDLE_BY_IDX(DT_PATH(zephyr_user), p, i)),         \
							   DT_PHA_BY_IDX(DT_PATH(zephyr_user), p, i, pin)),

#define ADC_DT_SPEC(n, p, i) ADC_DT_SPEC_GET_BY_IDX(n, i),
#define ADC_PINS(n, p, i)                                                                          \
	DIGITAL_PIN_GPIOS_FIND_PIN(DT_REG_ADDR(DT_PHANDLE_BY_IDX(DT_PATH(zephyr_user), p, i)),         \
							   DT_PHA_BY_IDX(DT_PATH(zephyr_user), p, i, pin)),
#define ADC_CH_CFG(n, p, i) arduino_adc[i].channel_cfg,

#define DAC_NODE       DT_PHANDLE(DT_PATH(zephyr_user), dac)
#define DAC_RESOLUTION DT_PROP(DT_PATH(zephyr_user), dac_resolution)
#define DAC_CHANNEL_DEFINE(n, p, i)                                                                \
	{                                                                                              \
		.channel_id = DT_PROP_BY_IDX(n, p, i),                                                     \
		.resolution = DAC_RESOLUTION,                                                              \
		.buffered = true,                                                                          \
		.internal = false,                                                                         \
	},

namespace {

#ifdef CONFIG_PWM

const struct pwm_dt_spec arduino_pwm[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), pwms, PWM_DT_SPEC)};

/* pwm-pins node provides a mapping digital pin numbers to pwm channels */
const pin_size_t arduino_pwm_pins[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), pwm_pin_gpios, PWM_PINS)};

size_t pwm_pin_index(pin_size_t pinNumber) {
	for (size_t i = 0; i < ARRAY_SIZE(arduino_pwm_pins); i++) {
		if (arduino_pwm_pins[i] == pinNumber) {
			return i;
		}
	}
	return (size_t)-1;
}

#endif // CONFIG_PWM

#ifdef CONFIG_ADC

const struct adc_dt_spec arduino_adc[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), io_channels, ADC_DT_SPEC)};

/* io-channel-pins node provides a mapping digital pin numbers to adc channels */
const pin_size_t arduino_analog_pins[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), adc_pin_gpios, ADC_PINS)};

struct adc_channel_cfg channel_cfg[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), io_channels, ADC_CH_CFG)};

size_t analog_pin_index(pin_size_t pinNumber) {
	for (size_t i = 0; i < ARRAY_SIZE(arduino_analog_pins); i++) {
		if (arduino_analog_pins[i] == pinNumber) {
			return i;
		}
	}
	return (size_t)-1;
}

#endif // CONFIG_ADC

#ifdef CONFIG_DAC

#if (DT_NODE_HAS_PROP(DT_PATH(zephyr_user), dac))

const struct device *const dac_dev = DEVICE_DT_GET(DAC_NODE);

#if DT_PROP_LEN_OR(DT_PATH(zephyr_user), dac_channels, 0) > 0
static const struct dac_channel_cfg dac_ch_cfg[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), dac_channels, DAC_CHANNEL_DEFINE)};

#endif

#endif

#endif // CONFIG_DAC

#if defined(CONFIG_DAC) || defined(CONFIG_PWM)
int _analog_write_resolution = 8;
#endif

// Note: We can not update the arduino_adc structure as it is read only...
static int read_resolution = 10;

uint32_t map64(uint32_t x, uint32_t in_min, uint32_t in_max, uint32_t out_min, uint32_t out_max) {
	return ((uint64_t)(x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min);
}

} // anonymous namespace

#if defined(CONFIG_DAC) || defined(CONFIG_PWM)
void analogWriteResolution(int bits) {
	_analog_write_resolution = bits;
}

int analogWriteResolution() {
	return _analog_write_resolution;
}
#endif

#ifdef CONFIG_PWM

void analogWrite(pin_size_t pinNumber, int value) {
	const int maxInput = BIT(_analog_write_resolution) - 1U;
	size_t idx = pwm_pin_index(pinNumber);

	if (idx >= ARRAY_SIZE(arduino_pwm)) {
		pinMode(pinNumber, OUTPUT);
		digitalWrite(pinNumber, value > 127 ? HIGH : LOW);
		return;
	}

	(void)init_dev_apply_channel_pinctrl(arduino_pwm[idx].dev,
										 state_pin_index_from_spec_index(arduino_pwm, idx));

	if (!pwm_is_ready_dt(&arduino_pwm[idx])) {
		pinMode(pinNumber, OUTPUT);
		digitalWrite(pinNumber, value > 127 ? HIGH : LOW);
		return;
	}

	value = CLAMP(value, 0, maxInput);

	const uint32_t pulse = map64(value, 0, maxInput, 0, arduino_pwm[idx].period);

	/*
	 * A duty ratio determines by the period value defined in dts
	 * and the value arguments. So usually the period value sets as 255.
	 */
	(void)pwm_set_pulse_dt(&arduino_pwm[idx], pulse);
}

#endif

#ifdef CONFIG_DAC
void analogWrite(enum dacPins dacName, int value) {
#if DT_PROP_LEN_OR(DT_PATH(zephyr_user), dac_channels, 0) > 0
	const int maxInput = BIT(_analog_write_resolution) - 1U;
	int ret = 0;

	if (dacName >= NUM_OF_DACS) {
		return;
	}

	// TODO: add reverse map to find pin name from DAC* define
	// In the meantime, consider A0 == DAC0
	(void)init_dev_apply_pinctrl(dac_dev);
	ret = dac_channel_setup(dac_dev, &dac_ch_cfg[dacName]);
	if (ret != 0) {
		return;
	}

	value = CLAMP(value, 0, maxInput);

	const int max_dac_value = BIT(dac_ch_cfg[dacName].resolution) - 1;
	const uint32_t output = map(value, 0, maxInput, 0, max_dac_value);

	(void)dac_write_value(dac_dev, dac_ch_cfg[dacName].channel_id, output);
#else
	ARG_UNUSED(dacName);
	ARG_UNUSED(value);
#endif
}
#endif

#ifdef CONFIG_ADC

void __attribute__((weak)) analogReference(uint8_t mode) {
	/*
	 * The Arduino API not clearly defined what means of
	 * the mode argument of analogReference().
	 * Treat the value as equivalent to zephyr's adc_reference.
	 */
	for (size_t i = 0; i < ARRAY_SIZE(channel_cfg); i++) {
		channel_cfg[i].reference = static_cast<adc_reference>(mode);
	}
}

void analogReadResolution(int bits) {
	read_resolution = bits;
}

int analogReadResolution() {
	return read_resolution;
}

int analogRead(pin_size_t pinNumber) {
	int err;
	uint16_t buf;
	struct adc_sequence seq = {.options = nullptr,
							   .channels = 0,
							   .buffer = &buf,
							   .buffer_size = sizeof(buf),
#if defined(CONFIG_ADC_SEQUENCE_PRIORITY)
							   .priority = 0,
#endif
							   .resolution = 0,
							   .oversampling = 0,
							   .calibrate = false};
	size_t idx = analog_pin_index(pinNumber);

	if (idx >= ARRAY_SIZE(arduino_adc)) {
		return -EINVAL;
	}

	/*
	 * ADC that is on MCU supported by Zephyr exists
	 * only 16bit resolution, currently.
	 */
	if (arduino_adc[idx].resolution > 16) {
		return -ENOTSUP;
	}

	/*
	 * Init the ADC device for the first acquisition and restore only the required pin to analog
	 * mode for following operations. The pin is selected from the ADC device "arduino" pinctrl
	 * state. Not checking the return value because the device might not have pinctrl (e.g. nRF
	 * SAADC).
	 */
	(void)init_dev_apply_channel_pinctrl(arduino_adc[idx].dev,
										 state_pin_index_from_spec_index(arduino_adc, idx));

	err = adc_channel_setup(arduino_adc[idx].dev, &arduino_adc[idx].channel_cfg);
	if (err < 0) {
		return err;
	}

	seq.channels = BIT(arduino_adc[idx].channel_id);
	seq.resolution = arduino_adc[idx].resolution;
	seq.oversampling = arduino_adc[idx].oversampling;

	err = adc_read(arduino_adc[idx].dev, &seq);
	if (err < 0) {
		return err;
	}

	/*
	 * If necessary map the return value to the
	 * number of bits the user has asked for
	 */
	if (read_resolution == seq.resolution) {
		return buf;
	}
	if (read_resolution < seq.resolution) {
		return buf >> (seq.resolution - read_resolution);
	}
	return buf << (read_resolution - seq.resolution);
}

#endif
