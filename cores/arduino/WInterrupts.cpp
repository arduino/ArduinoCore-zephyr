/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 * Copyright (c) 2022 TOKITA Hiroshi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <Arduino.h>
#include "wiring_private.h"

using namespace zephyr::arduino;

namespace {

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
} port_callback[port_num] = {};

unsigned int irq_key;
bool interrupts_disabled = false;

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

void enableInterrupt(pin_size_t pinNumber) {
	RETURN_ON_INVALID_PIN(pinNumber);

	struct gpio_port_callback *pcb = find_gpio_port_callback(arduino_pins[pinNumber].port);

	if (pcb) {
		pcb->handlers[arduino_pins[pinNumber].pin].enabled = true;
	}
}

void disableInterrupt(pin_size_t pinNumber) {
	RETURN_ON_INVALID_PIN(pinNumber);

	gpio_pin_interrupt_configure(arduino_pins[pinNumber].port, arduino_pins[pinNumber].pin,
								 GPIO_INT_DISABLE);

	struct gpio_port_callback *pcb = find_gpio_port_callback(arduino_pins[pinNumber].port);

	if (pcb) {
		pcb->handlers[arduino_pins[pinNumber].pin].enabled = false;
	}
}

} // anonymous namespace

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

	disableInterrupt(pinNumber);
	setInterruptHandler(pinNumber, nullptr);
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
