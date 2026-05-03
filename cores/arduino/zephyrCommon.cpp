/*
 * Copyright (c) 2022 Dhruva Gole
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <Arduino.h>
#include "zephyrInternal.h"

#include <zephyr/spinlock.h>

// create an array of arduino_pins with functions to reinitialize pins if needed
static const struct device *pinmux_array[DT_PROP_LEN(DT_PATH(zephyr_user), digital_pin_gpios)] = {
	nullptr};

void _reinit_peripheral_if_needed(pin_size_t pin, const struct device *dev) {
	if (pinmux_array[pin] != dev) {
		pinmux_array[pin] = dev;
		if (dev != NULL) {
			device_init(dev);
		}
	}
}

static const struct gpio_dt_spec arduino_pins[] = {
	DT_FOREACH_PROP_ELEM_SEP(
	DT_PATH(zephyr_user), digital_pin_gpios, GPIO_DT_SPEC_GET_BY_IDX, (, ))};

#define RETURN_ON_INVALID_PIN(pinNumber, ...)                                                      \
	do {                                                                                           \
		if ((pin_size_t)(pinNumber) >= ARRAY_SIZE(arduino_pins)) {                                 \
			return __VA_ARGS__;                                                                    \
		}                                                                                          \
	} while (0)

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
	RETURN_ON_INVALID_PIN(pinNumber);

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

#if DT_PROP_LEN_OR(DT_PATH(zephyr_user), dac_channels, 0) > 0
static const struct dac_channel_cfg dac_ch_cfg[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), dac_channels, DAC_CHANNEL_DEFINE)};

static bool dac_channel_initialized[NUM_OF_DACS];
#endif

#endif

#endif // CONFIG_DAC

static unsigned int irq_key;
static bool interrupts_disabled = false;
} // namespace

void yield(void) {
	k_yield();
}

const struct device *digitalPinToPortDevice(pin_size_t pinNumber) {
	RETURN_ON_INVALID_PIN(pinNumber, nullptr);

	return arduino_pins[pinNumber].port;
}

int digitalPinToPinIndex(pin_size_t pinNumber) {
	RETURN_ON_INVALID_PIN(pinNumber, -1);

	return arduino_pins[pinNumber].pin;
}

/*
 *  The ACTIVE_HIGH flag is set so that A low physical
 *  level on the pin will be interpreted as value 0.
 *  A high physical level will be interpreted as value 1
 */
void pinMode(pin_size_t pinNumber, PinMode pinMode) {
	RETURN_ON_INVALID_PIN(pinNumber);

	_reinit_peripheral_if_needed(pinNumber, NULL);
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
	RETURN_ON_INVALID_PIN(pinNumber);

	gpio_pin_set_dt(&arduino_pins[pinNumber], status);
}

PinStatus digitalRead(pin_size_t pinNumber) {
	RETURN_ON_INVALID_PIN(pinNumber, LOW);

	return (gpio_pin_get_dt(&arduino_pins[pinNumber]) == 1) ? HIGH : LOW;
}

#if CONFIG_ARDUINO_MAX_TONES < 0
#define MAX_TONE_PINS DT_PROP_LEN(DT_PATH(zephyr_user), digital_pin_gpios)
#else
#define MAX_TONE_PINS CONFIG_ARDUINO_MAX_TONES
#endif

#define TOGGLES_PER_CYCLE 2ULL

static struct pin_timer {
	struct k_timer timer;
	uint32_t count{0};
	pin_size_t pin{pin_size_t(-1)};
	bool infinity{false};
	bool timer_initialized{false};
	struct k_spinlock lock{};
} arduino_pin_timers[MAX_TONE_PINS];

K_MUTEX_DEFINE(timer_cfg_lock);

void tone_expiry_cb(struct k_timer *timer);

/* Callers must hold timer_cfg_lock while using this helper. */
static struct pin_timer *find_pin_timer(pin_size_t pinNumber, bool active_only) {
	for (size_t i = 0; i < ARRAY_SIZE(arduino_pin_timers); i++) {
		k_spinlock_key_t key = k_spin_lock(&arduino_pin_timers[i].lock);

		if (arduino_pin_timers[i].pin == pinNumber) {
			k_spin_unlock(&arduino_pin_timers[i].lock, key);
			return &arduino_pin_timers[i];
		}

		k_spin_unlock(&arduino_pin_timers[i].lock, key);
	}

	if (active_only) {
		return nullptr;
	}

	for (size_t i = 0; i < ARRAY_SIZE(arduino_pin_timers); i++) {
		k_spinlock_key_t key = k_spin_lock(&arduino_pin_timers[i].lock);

		if (arduino_pin_timers[i].pin == pin_size_t(-1)) {
			arduino_pin_timers[i].pin = pinNumber;
			k_spin_unlock(&arduino_pin_timers[i].lock, key);
			return &arduino_pin_timers[i];
		}

		k_spin_unlock(&arduino_pin_timers[i].lock, key);
	}

	return nullptr;
}

void tone_expiry_cb(struct k_timer *timer) {
	struct pin_timer *pt = CONTAINER_OF(timer, struct pin_timer, timer);
	k_spinlock_key_t key = k_spin_lock(&pt->lock);
	pin_size_t pin = pt->pin;

	if (pt->count == 0 && !pt->infinity) {
		if (pin != pin_size_t(-1)) {
			gpio_pin_set_dt(&arduino_pins[pin], 0);
		}

		k_timer_stop(timer);
		pt->count = 0;
		pt->infinity = false;
		pt->pin = pin_size_t(-1);
	} else {
		if (pin != pin_size_t(-1)) {
			gpio_pin_toggle_dt(&arduino_pins[pin]);
		}
		pt->count--;
	}

	k_spin_unlock(&pt->lock, key);
}

void tone(pin_size_t pinNumber, unsigned int frequency, unsigned long duration) {
	RETURN_ON_INVALID_PIN(pinNumber);

	k_spinlock_key_t key;
	uint64_t toggles_count;
	struct pin_timer *pt;
	k_timeout_t timeout;

	if (k_is_in_isr()) {
		return;
	}

	k_mutex_lock(&timer_cfg_lock, K_FOREVER);

	pt = find_pin_timer(pinNumber, false);

	if (pt == nullptr) {
		k_mutex_unlock(&timer_cfg_lock);
		return;
	}

	if (!pt->timer_initialized) {
		k_timer_init(&pt->timer, tone_expiry_cb, NULL);
		pt->timer_initialized = true;
	}

	pinMode(pinNumber, OUTPUT);
	k_timer_stop(&pt->timer);

	toggles_count = ((uint64_t)duration * frequency / (MSEC_PER_SEC / TOGGLES_PER_CYCLE));
	if (frequency == 0 || (toggles_count == 0 && duration != 0)) {
		key = k_spin_lock(&pt->lock);
		pt->count = 0;
		pt->infinity = false;
		pt->pin = pin_size_t(-1);
		k_spin_unlock(&pt->lock, key);

		gpio_pin_set_dt(&arduino_pins[pinNumber], 0);

		k_mutex_unlock(&timer_cfg_lock);
		return;
	}

	timeout = K_NSEC(NSEC_PER_SEC / (TOGGLES_PER_CYCLE * frequency));
	if (timeout.ticks == 0) {
		timeout.ticks = 1;
	}

	key = k_spin_lock(&pt->lock);
	pt->infinity = (duration == 0);
	pt->count = min(toggles_count, UINT32_MAX);
	pt->pin = pinNumber;
	k_spin_unlock(&pt->lock, key);

	gpio_pin_set_dt(&arduino_pins[pinNumber], 1);
	k_timer_start(&pt->timer, timeout, timeout);

	k_mutex_unlock(&timer_cfg_lock);
}

void noTone(pin_size_t pinNumber) {
	RETURN_ON_INVALID_PIN(pinNumber);

	struct pin_timer *pt;
	k_spinlock_key_t key;

	if (k_is_in_isr()) {
		return;
	}

	k_mutex_lock(&timer_cfg_lock, K_FOREVER);

	pt = find_pin_timer(pinNumber, true);

	if (pt == nullptr) {
		k_mutex_unlock(&timer_cfg_lock);
		return;
	}

	key = k_spin_lock(&pt->lock);
	k_timer_stop(&pt->timer);
	pt->count = 0;
	pt->infinity = false;
	pt->pin = pin_size_t(-1);
	k_spin_unlock(&pt->lock, key);

	gpio_pin_set_dt(&arduino_pins[pinNumber], 0);

	k_mutex_unlock(&timer_cfg_lock);
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

static uint32_t map64(uint32_t x, uint32_t in_min, uint32_t in_max, uint32_t out_min,
					  uint32_t out_max) {
	return ((uint64_t)(x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min);
}

void analogWrite(pin_size_t pinNumber, int value) {
	const int maxInput = BIT(_analog_write_resolution) - 1U;
	size_t idx = pwm_pin_index(pinNumber);

	if (idx >= ARRAY_SIZE(arduino_pwm)) {
		pinMode(pinNumber, OUTPUT);
		digitalWrite(pinNumber, value > 127 ? HIGH : LOW);
		return;
	}

	if (!pwm_is_ready_dt(&arduino_pwm[idx])) {
		pinMode(pinNumber, OUTPUT);
		digitalWrite(pinNumber, value > 127 ? HIGH : LOW);
		return;
	}

	_reinit_peripheral_if_needed(pinNumber, arduino_pwm[idx].dev);
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

	if (!dac_channel_initialized[dacName]) {
		if (!device_is_ready(dac_dev)) {
			return;
		}

		// TODO: add reverse map to find pin name from DAC* define
		// In the meantime, consider A0 == DAC0
		_reinit_peripheral_if_needed((pin_size_t)(dacName + A0), dac_dev);
		ret = dac_channel_setup(dac_dev, &dac_ch_cfg[dacName]);
		if (ret != 0) {
			return;
		}
		dac_channel_initialized[dacName] = true;
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

	_reinit_peripheral_if_needed(pinNumber, arduino_adc[idx].dev);

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
	RETURN_ON_INVALID_PIN(pinNumber);

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
	RETURN_ON_INVALID_PIN(pinNumber);

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

unsigned long pulseIn(pin_size_t pinNumber, uint8_t state, unsigned long timeout) {
	RETURN_ON_INVALID_PIN(pinNumber, LOW);

	struct k_timer timer;
	int64_t start, end, delta = 0;
	const struct gpio_dt_spec *spec = &arduino_pins[pinNumber];

	if (!gpio_is_ready_dt(spec)) {
		return 0;
	}

	k_timer_init(&timer, NULL, NULL);
	k_timer_start(&timer, K_MSEC(timeout), K_NO_WAIT);

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

void enableInterrupt(pin_size_t pinNumber) {
	RETURN_ON_INVALID_PIN(pinNumber);

	struct gpio_port_callback *pcb = find_gpio_port_callback(arduino_pins[pinNumber].port);

	if (pcb) {
		pcb->handlers[arduino_pins[pinNumber].pin].enabled = true;
	}
}

void disableInterrupt(pin_size_t pinNumber) {
	RETURN_ON_INVALID_PIN(pinNumber);

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

int digitalPinToInterrupt(pin_size_t pinNumber) {
	RETURN_ON_INVALID_PIN(pinNumber, -1);

	struct gpio_port_callback *pcb = find_gpio_port_callback(arduino_pins[pinNumber].port);

	return (pcb) ? pinNumber : -1;
}
