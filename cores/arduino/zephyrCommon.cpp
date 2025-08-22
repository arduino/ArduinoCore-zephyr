/*
 * Copyright (c) 2022 Dhruva Gole
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <Arduino.h>
#include "zephyrInternal.h"

static const struct gpio_dt_spec arduino_pins[] = {
	DT_FOREACH_PROP_ELEM_SEP(
	DT_PATH(zephyr_user), digital_pin_gpios, GPIO_DT_SPEC_GET_BY_IDX, (, ))};

namespace {

#if DT_PROP_LEN(DT_PATH(zephyr_user), digital_pin_gpios) > 0

/*
 * Calculate GPIO ports/pins number statically from devicetree configuration
 */

template <class N, class Head> constexpr N sum_of_list(const N sum, const Head &head) {
	return sum + head;
}

template <class N, class Head, class... Tail>
constexpr N sum_of_list(const N sum, const Head &head, const Tail &...tail) {
	return sum_of_list(sum + head, tail...);
}

template <class N, class Head> constexpr N max_in_list(const N max, const Head &head) {
	return (max >= head) ? max : head;
}

template <class N, class Head, class... Tail>
constexpr N max_in_list(const N max, const Head &head, const Tail &...tail) {
	return max_in_list((max >= head) ? max : head, tail...);
}

template <class Query, class Head>
constexpr size_t is_first_appearance(const size_t &idx, const size_t &at, const size_t &found,
									 const Query &query, const Head &head) {
	return ((found == ((size_t)-1)) && (query == head) && (idx == at)) ? 1 : 0;
}

template <class Query, class Head, class... Tail>
constexpr size_t is_first_appearance(const size_t &idx, const size_t &at, const size_t &found,
									 const Query &query, const Head &head, const Tail &...tail) {
	return ((found == ((size_t)-1)) && (query == head) && (idx == at)) ?
			   1 :
			   is_first_appearance(idx + 1, at, (query == head ? idx : found), query, tail...);
}

#define GET_DEVICE_VARGS(n, p, i, _) DEVICE_DT_GET(DT_GPIO_CTLR_BY_IDX(n, p, i))
#define FIRST_APPEARANCE(n, p, i)                                                                  \
	is_first_appearance(0, i, ((size_t)-1), DEVICE_DT_GET(DT_GPIO_CTLR_BY_IDX(n, p, i)),           \
						DT_FOREACH_PROP_ELEM_SEP_VARGS(n, p, GET_DEVICE_VARGS, (, ), 0))
const int port_num = sum_of_list(
	0, DT_FOREACH_PROP_ELEM_SEP(DT_PATH(zephyr_user), digital_pin_gpios,
            FIRST_APPEARANCE, (, )));

#define GPIO_NGPIOS(n, p, i) DT_PROP(DT_GPIO_CTLR_BY_IDX(n, p, i), ngpios)
const int max_ngpios = max_in_list(
	0, DT_FOREACH_PROP_ELEM_SEP(DT_PATH(zephyr_user), digital_pin_gpios, GPIO_NGPIOS, (, )));

#else

const int port_num = 1;
const int max_ngpios = 0;

#endif

/*
 * GPIO callback implementation
 */

struct arduino_callback {
	voidFuncPtr handler;
	bool enabled;
};

struct gpio_port_callback {
	struct gpio_callback callback;
	struct arduino_callback handlers[max_ngpios];
	gpio_port_pins_t pins;
	const struct device *dev;
} port_callback[port_num] = {0};

struct gpio_port_callback *find_gpio_port_callback(const struct device *dev) {
	for (size_t i = 0; i < ARRAY_SIZE(port_callback); i++) {
		if (port_callback[i].dev == dev) {
			return &port_callback[i];
		}
		if (port_callback[i].dev == nullptr) {
			port_callback[i].dev = dev;
			return &port_callback[i];
		}
	}

	return nullptr;
}

void setInterruptHandler(pin_size_t pinNumber, voidFuncPtr func) {
	struct gpio_port_callback *pcb = find_gpio_port_callback(arduino_pins[pinNumber].port);

	if (pcb) {
		pcb->handlers[arduino_pins[pinNumber].pin].handler = func;
	}
}

void handleGpioCallback(const struct device *port, struct gpio_callback *cb, uint32_t pins) {
	(void)port; // unused
	struct gpio_port_callback *pcb = (struct gpio_port_callback *)cb;

	for (uint32_t i = 0; i < max_ngpios; i++) {
		if (pins & BIT(i) && pcb->handlers[i].enabled) {
			pcb->handlers[i].handler();
		}
	}
}

#ifdef CONFIG_PWM

#define PWM_DT_SPEC(n, p, i) PWM_DT_SPEC_GET_BY_IDX(n, i),
#define PWM_PINS(n, p, i)                                                                          \
	DIGITAL_PIN_GPIOS_FIND_PIN(DT_REG_ADDR(DT_PHANDLE_BY_IDX(DT_PATH(zephyr_user), p, i)),         \
							   DT_PHA_BY_IDX(DT_PATH(zephyr_user), p, i, pin)),

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

#define ADC_DT_SPEC(n, p, i) ADC_DT_SPEC_GET_BY_IDX(n, i),
#define ADC_PINS(n, p, i)                                                                          \
	DIGITAL_PIN_GPIOS_FIND_PIN(DT_REG_ADDR(DT_PHANDLE_BY_IDX(DT_PATH(zephyr_user), p, i)),         \
							   DT_PHA_BY_IDX(DT_PATH(zephyr_user), p, i, pin)),
#define ADC_CH_CFG(n, p, i) arduino_adc[i].channel_cfg,

static const struct adc_dt_spec arduino_adc[] = {
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

#define DAC_NODE       DT_PHANDLE(DT_PATH(zephyr_user), dac)
#define DAC_RESOLUTION DT_PROP(DT_PATH(zephyr_user), dac_resolution)
static const struct device *const dac_dev = DEVICE_DT_GET(DAC_NODE);

#define DAC_CHANNEL_DEFINE(n, p, i)                                                                \
	{                                                                                              \
		.channel_id = DT_PROP_BY_IDX(n, p, i),                                                     \
		.resolution = DAC_RESOLUTION,                                                              \
		.buffered = true,                                                                          \
	},

static const struct dac_channel_cfg dac_ch_cfg[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), dac_channels, DAC_CHANNEL_DEFINE)};

#endif

#endif // CONFIG_DAC

static unsigned int irq_key;
static bool interrupts_disabled = false;
} // namespace

void yield(void) {
	k_yield();
}

/*
 *  The ACTIVE_HIGH flag is set so that A low physical
 *  level on the pin will be interpreted as value 0.
 *  A high physical level will be interpreted as value 1
 */
void pinMode(pin_size_t pinNumber, PinMode pinMode) {
	if (pinMode == INPUT) { // input mode
		gpio_pin_configure_dt(&arduino_pins[pinNumber], GPIO_INPUT | GPIO_ACTIVE_HIGH);
	} else if (pinMode == INPUT_PULLUP) { // input with internal pull-up
		gpio_pin_configure_dt(&arduino_pins[pinNumber],
							  GPIO_INPUT | GPIO_PULL_UP | GPIO_ACTIVE_HIGH);
	} else if (pinMode == INPUT_PULLDOWN) { // input with internal pull-down
		gpio_pin_configure_dt(&arduino_pins[pinNumber],
							  GPIO_INPUT | GPIO_PULL_DOWN | GPIO_ACTIVE_HIGH);
	} else if (pinMode == OUTPUT) { // output mode
		gpio_pin_configure_dt(&arduino_pins[pinNumber], GPIO_OUTPUT_LOW | GPIO_ACTIVE_HIGH);
	}
}

void digitalWrite(pin_size_t pinNumber, PinStatus status) {
	gpio_pin_set_dt(&arduino_pins[pinNumber], status);
}

PinStatus digitalRead(pin_size_t pinNumber) {
	return (gpio_pin_get_dt(&arduino_pins[pinNumber]) == 1) ? HIGH : LOW;
}

struct k_timer arduino_pin_timers[ARRAY_SIZE(arduino_pins)];
struct k_timer arduino_pin_timers_timeout[ARRAY_SIZE(arduino_pins)];

void tone_expiry_cb(struct k_timer *timer) {
	const struct gpio_dt_spec *spec = (gpio_dt_spec *)k_timer_user_data_get(timer);
	gpio_pin_toggle_dt(spec);
}

void tone_timeout_cb(struct k_timer *timer) {
	pin_size_t pinNumber = (pin_size_t)(uintptr_t)k_timer_user_data_get(timer);
	noTone(pinNumber);
}

void tone(pin_size_t pinNumber, unsigned int frequency, unsigned long duration) {
	struct k_timer *timer = &arduino_pin_timers[pinNumber];
	const struct gpio_dt_spec *spec = &arduino_pins[pinNumber];
	k_timeout_t timeout;

	pinMode(pinNumber, OUTPUT);

	if (frequency == 0) {
		gpio_pin_set_dt(spec, 0);
		return;
	}

	timeout = K_NSEC(NSEC_PER_SEC / (2 * frequency));

	k_timer_init(timer, tone_expiry_cb, NULL);
	k_timer_user_data_set(timer, (void *)spec);
	gpio_pin_set_dt(spec, 1);
	k_timer_start(timer, timeout, timeout);

	if (duration > 0) {
		timer = &arduino_pin_timers_timeout[pinNumber];
		k_timer_init(timer, tone_timeout_cb, NULL);
		k_timer_user_data_set(timer, (void *)(uintptr_t)pinNumber);
		k_timer_start(timer, K_MSEC(duration), K_NO_WAIT);
	}
}

void noTone(pin_size_t pinNumber) {
	k_timer_stop(&arduino_pin_timers[pinNumber]);
	gpio_pin_set_dt(&arduino_pins[pinNumber], 0);
}

void delay(unsigned long ms) {
	k_sleep(K_MSEC(ms));
}

void delayMicroseconds(unsigned int us) {
	k_sleep(K_USEC(us));
}

unsigned long micros(void) {
#ifdef CONFIG_TIMER_HAS_64BIT_CYCLE_COUNTER
	return k_cyc_to_us_floor32(k_cycle_get_64());
#else
	return k_cyc_to_us_floor32(k_cycle_get_32());
#endif
}

unsigned long millis(void) {
	return k_uptime_get_32();
}

#if defined(CONFIG_DAC) || defined(CONFIG_PWM)
static int _analog_write_resolution = 8;

void analogWriteResolution(int bits) {
	_analog_write_resolution = bits;
}

int analogWriteResolution() {
	return _analog_write_resolution;
}
#endif

#ifdef CONFIG_PWM

void analogWrite(pin_size_t pinNumber, int value) {
	size_t idx = pwm_pin_index(pinNumber);

	if (idx >= ARRAY_SIZE(arduino_pwm)) {
		return;
	}

	if (!pwm_is_ready_dt(&arduino_pwm[idx])) {
		return;
	}

	value = map(value, 0, 1 << _analog_write_resolution, 0, arduino_pwm[idx].period);

	if (((uint32_t)value) > arduino_pwm[idx].period) {
		value = arduino_pwm[idx].period;
	} else if (value < 0) {
		value = 0;
	}

	/*
	 * A duty ratio determines by the period value defined in dts
	 * and the value arguments. So usually the period value sets as 255.
	 */
	(void)pwm_set_pulse_dt(&arduino_pwm[idx], value);
}

#endif

#ifdef CONFIG_DAC
void analogWrite(enum dacPins dacName, int value) {
	if (dacName >= NUM_OF_DACS) {
		return;
	}

	dac_channel_setup(dac_dev, &dac_ch_cfg[dacName]);

	const int max_dac_value = 1U << dac_ch_cfg[dacName].resolution;
	dac_write_value(dac_dev, dac_ch_cfg[dacName].channel_id,
					map(value, 0, 1 << _analog_write_resolution, 0, max_dac_value));
}
#endif

#ifdef CONFIG_ADC

void analogReference(uint8_t mode) {
	/*
	 * The Arduino API not clearly defined what means of
	 * the mode argument of analogReference().
	 * Treat the value as equivalent to zephyr's adc_reference.
	 */
	for (size_t i = 0; i < ARRAY_SIZE(channel_cfg); i++) {
		channel_cfg[i].reference = static_cast<adc_reference>(mode);
	}
}

// Note: We can not update the arduino_adc structure as it is read only...
static int read_resolution = 10;

void analogReadResolution(int bits) {
	read_resolution = bits;
}

int analogReadResolution() {
	return read_resolution;
}

int analogRead(pin_size_t pinNumber) {
	int err;
	uint16_t buf;
	struct adc_sequence seq = {.buffer = &buf, .buffer_size = sizeof(buf)};
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

void attachInterrupt(pin_size_t pinNumber, voidFuncPtr callback, PinStatus pinStatus) {
	struct gpio_port_callback *pcb;
	gpio_flags_t intmode = 0;

	if (!callback) {
		return;
	}

	if (pinStatus == LOW) {
		intmode |= GPIO_INT_LEVEL_LOW;
	} else if (pinStatus == HIGH) {
		intmode |= GPIO_INT_LEVEL_HIGH;
	} else if (pinStatus == CHANGE) {
		intmode |= GPIO_INT_EDGE_BOTH;
	} else if (pinStatus == FALLING) {
		intmode |= GPIO_INT_EDGE_FALLING;
	} else if (pinStatus == RISING) {
		intmode |= GPIO_INT_EDGE_RISING;
	} else {
		return;
	}

	pcb = find_gpio_port_callback(arduino_pins[pinNumber].port);
	__ASSERT(pcb != nullptr, "gpio_port_callback not found");

	pcb->pins |= BIT(arduino_pins[pinNumber].pin);
	setInterruptHandler(pinNumber, callback);
	enableInterrupt(pinNumber);

	gpio_pin_interrupt_configure(arduino_pins[pinNumber].port, arduino_pins[pinNumber].pin,
								 intmode);
	gpio_init_callback(&pcb->callback, handleGpioCallback, pcb->pins);
	gpio_add_callback(arduino_pins[pinNumber].port, &pcb->callback);
}

void detachInterrupt(pin_size_t pinNumber) {
	setInterruptHandler(pinNumber, nullptr);
	disableInterrupt(pinNumber);
}

#ifndef CONFIG_MINIMAL_LIBC_RAND

#include <stdlib.h>

void randomSeed(unsigned long seed) {
	srand(seed);
}

long random(long min, long max) {
	return rand() % (max - min) + min;
}

long random(long max) {
	return rand() % max;
}

#endif

#ifdef CONFIG_GPIO_GET_DIRECTION

unsigned long pulseIn(pin_size_t pinNumber, uint8_t state, unsigned long timeout) {
	struct k_timer timer;
	int64_t start, end, delta = 0;
	const struct gpio_dt_spec *spec = &arduino_pins[pinNumber];

	k_timer_init(&timer, NULL, NULL);
	k_timer_start(&timer, K_MSEC(timeout), K_NO_WAIT);

	if (!gpio_is_ready_dt(spec)) {
		goto cleanup;
	}

	if (!gpio_pin_is_input_dt(spec)) {
		goto cleanup;
	}

	while (gpio_pin_get_dt(spec) == state && k_timer_status_get(&timer) == 0)
		;
	if (k_timer_status_get(&timer) > 0) {
		goto cleanup;
	}

	while (gpio_pin_get_dt(spec) != state && k_timer_status_get(&timer) == 0)
		;
	if (k_timer_status_get(&timer) > 0) {
		goto cleanup;
	}

	start = k_uptime_ticks();
	while (gpio_pin_get_dt(spec) == state && k_timer_status_get(&timer) == 0)
		;
	if (k_timer_status_get(&timer) > 0) {
		goto cleanup;
	}
	end = k_uptime_ticks();

	delta = k_ticks_to_us_floor64(end - start);

cleanup:
	k_timer_stop(&timer);
	return (unsigned long)delta;
}

#endif // CONFIG_GPIO_GET_DIRECTION

void enableInterrupt(pin_size_t pinNumber) {
	struct gpio_port_callback *pcb = find_gpio_port_callback(arduino_pins[pinNumber].port);

	if (pcb) {
		pcb->handlers[arduino_pins[pinNumber].pin].enabled = true;
	}
}

void disableInterrupt(pin_size_t pinNumber) {
	struct gpio_port_callback *pcb = find_gpio_port_callback(arduino_pins[pinNumber].port);

	if (pcb) {
		pcb->handlers[arduino_pins[pinNumber].pin].enabled = false;
	}
}

void interrupts(void) {
	if (interrupts_disabled) {
		irq_unlock(irq_key);
		interrupts_disabled = false;
	}
}

void noInterrupts(void) {
	if (!interrupts_disabled) {
		irq_key = irq_lock();
		interrupts_disabled = true;
	}
}

int digitalPinToInterrupt(pin_size_t pin) {
	struct gpio_port_callback *pcb = find_gpio_port_callback(arduino_pins[pin].port);

	return (pcb) ? pin : -1;
}
