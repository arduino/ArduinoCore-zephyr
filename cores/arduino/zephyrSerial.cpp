/*
 * Copyright (c) 2022 Dhruva Gole
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>

#include <api/HardwareSerial.h>
#include <zephyrSerial.h>
#include <Arduino.h>

namespace {

enum uart_config_parity conf_parity(uint16_t conf) {
	switch (conf & SERIAL_PARITY_MASK) {
	case SERIAL_PARITY_EVEN:
		return UART_CFG_PARITY_EVEN;
	case SERIAL_PARITY_ODD:
		return UART_CFG_PARITY_ODD;
	default:
		return UART_CFG_PARITY_NONE;
	}
}

enum uart_config_stop_bits conf_stop_bits(uint16_t conf) {
	switch (conf & SERIAL_STOP_BIT_MASK) {
	case SERIAL_STOP_BIT_1_5:
		return UART_CFG_STOP_BITS_1_5;
	case SERIAL_STOP_BIT_2:
		return UART_CFG_STOP_BITS_2;
	default:
		return UART_CFG_STOP_BITS_1;
	}
}

enum uart_config_data_bits conf_data_bits(uint16_t conf) {
	switch (conf & SERIAL_DATA_MASK) {
	case SERIAL_DATA_5:
		return UART_CFG_DATA_BITS_5;
	case SERIAL_DATA_6:
		return UART_CFG_DATA_BITS_6;
	case SERIAL_DATA_7:
		return UART_CFG_DATA_BITS_7;
	default:
		return UART_CFG_DATA_BITS_8;
	}
}

} // anonymous namespace

void arduino::ZephyrSerial::begin(unsigned long baud, uint16_t conf) {
	struct uart_config config = {
		.baudrate = baud,
		.parity = conf_parity(conf),
		.stop_bits = conf_stop_bits(conf),
		.data_bits = conf_data_bits(conf),
		.flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
	};

	uart_configure(uart, &config);
	uart_irq_callback_user_data_set(uart, arduino::ZephyrSerial::IrqDispatch, this);
	k_sem_take(&rx.sem, K_FOREVER);
	ring_buf_reset(&rx.ringbuf);
	k_sem_give(&rx.sem);

	uart_irq_rx_enable(uart);
}

void arduino::ZephyrSerial::IrqHandler() {
	uint8_t buf[8];
	int length;
	int ret = 0;

	if (!uart_irq_update(uart)) {
		return;
	}

	k_sem_take(&rx.sem, K_NO_WAIT);
	while (uart_irq_rx_ready(uart) && ((length = uart_fifo_read(uart, buf, sizeof(buf))) > 0)) {
		length = min(sizeof(buf), static_cast<size_t>(length));
		ret = ring_buf_put(&rx.ringbuf, &buf[0], length);

		if (ret < 0) {
			break;
		}
	}
	k_sem_give(&rx.sem);

	k_sem_take(&tx.sem, K_NO_WAIT);

	if (ring_buf_size_get(&tx.ringbuf) == 0) {
		uart_irq_tx_disable(uart);
	}

	while (uart_irq_tx_ready(uart) && ((length = ring_buf_size_get(&tx.ringbuf)) > 0)) {
		length = min(sizeof(buf), static_cast<size_t>(length));
		ring_buf_peek(&tx.ringbuf, &buf[0], length);

		ret = uart_fifo_fill(uart, &buf[0], length);
		if (ret < 0) {
			break;
		} else {
			ring_buf_get(&tx.ringbuf, &buf[0], ret);
		}
	}
	k_sem_give(&tx.sem);
}

void arduino::ZephyrSerial::IrqDispatch(const struct device *dev, void *data) {
	(void)dev; // unused
	reinterpret_cast<ZephyrSerial *>(data)->IrqHandler();
}

int arduino::ZephyrSerial::available() {
	int ret;

	k_sem_take(&rx.sem, K_FOREVER);
	ret = ring_buf_size_get(&rx.ringbuf);
	k_sem_give(&rx.sem);

	return ret;
}

int arduino::ZephyrSerial::availableForWrite() {
	int ret;

	k_sem_take(&rx.sem, K_FOREVER);
	ret = ring_buf_space_get(&rx.ringbuf);
	k_sem_give(&rx.sem);

	return ret;
}

int arduino::ZephyrSerial::peek() {
	uint8_t data;

	k_sem_take(&rx.sem, K_FOREVER);
	uint32_t cb_ret = ring_buf_peek(&rx.ringbuf, &data, 1);
	k_sem_give(&rx.sem);

	return cb_ret ? data : -1;
}

int arduino::ZephyrSerial::read() {
	uint8_t data;

	k_sem_take(&rx.sem, K_FOREVER);
	uint32_t cb_ret = ring_buf_get(&rx.ringbuf, &data, 1);
	k_sem_give(&rx.sem);

	return cb_ret ? data : -1;
}

size_t arduino::ZephyrSerial::write(const uint8_t *buffer, size_t size) {
	size_t idx = 0;

	while (1) {
		k_sem_take(&tx.sem, K_FOREVER);
		auto ret = ring_buf_put(&tx.ringbuf, &buffer[idx], size - idx);
		k_sem_give(&tx.sem);
		if (ret < 0) {
			return 0;
		}
		idx += ret;
		if (ret == 0) {
			uart_irq_tx_enable(uart);
			yield();
		}
		if (idx == size) {
			break;
		}
	}

	uart_irq_tx_enable(uart);

	return size;
}

void arduino::ZephyrSerial::flush() {
	while (ring_buf_size_get(&tx.ringbuf) > 0) {
		k_yield();
	}
	while (uart_irq_tx_complete(uart) == 0) {
		k_yield();
	}
}

#if CONFIG_ARDUINO_SERIAL_PRINTF
void arduino::ZephyrSerial::printf(const char *fmt, ...) {
	char message[CONFIG_ARDUINO_SERIAL_PRINTF_BUFFER_SIZE];
	va_list args;
	va_start(args, fmt);
	vsnprintf(message, sizeof(message), fmt, args);
	va_end(args);
	this->write((uint8_t *)message, strlen(message));
}
#endif

#if (DT_NODE_HAS_PROP(DT_PATH(zephyr_user), cdc_acm))
#define FIRST_UART_INDEX 1
#else
#define FIRST_UART_INDEX 0
#endif

#if DT_NODE_HAS_PROP(DT_PATH(zephyr_user), serials)
#if !(DT_NODE_HAS_PROP(DT_PATH(zephyr_user), cdc_acm) &&                                           \
	  (CONFIG_USB_CDC_ACM || CONFIG_USBD_CDC_ACM_CLASS))
// If CDC USB, use that object as Serial (and SerialUSB)
arduino::ZephyrSerial
	Serial(DEVICE_DT_GET(DT_PHANDLE_BY_IDX(DT_PATH(zephyr_user), serials, FIRST_UART_INDEX)));
#endif
#if (DT_PROP_LEN(DT_PATH(zephyr_user), serials) > 1)
#define ARDUINO_SERIAL_DEFINED_0 1

#define DECL_SERIAL_0(n, p, i)
#define DECL_SERIAL_N(n, p, i)                                                                     \
	arduino::ZephyrSerial Serial##i(DEVICE_DT_GET(DT_PHANDLE_BY_IDX(n, p, i)));
#define DECLARE_SERIAL_N(n, p, i)                                                                  \
	COND_CODE_1(ARDUINO_SERIAL_DEFINED_##i, (DECL_SERIAL_0(n, p, i)), (DECL_SERIAL_N(n, p, i)))

#define CALL_EVENT_0(n, p, i)
#define CALL_EVENT_N(n, p, i)                                                                      \
	if (_CONCAT(Serial, i).available())                                                            \
		_CONCAT(_CONCAT(serial, i), Event)();
#define CALL_SERIALEVENT_N(n, p, i)                                                                \
	COND_CODE_1(ARDUINO_SERIAL_DEFINED_##i, (CALL_EVENT_0(n, p, i)), (CALL_EVENT_N(n, p, i)));

#define DECL_EVENT_0(n, p, i)
#define DECL_EVENT_N(n, p, i)                                                                      \
	__attribute__((weak)) void serial##i##Event() {                                                \
	}
#define DECLARE_SERIALEVENT_N(n, p, i)                                                             \
	COND_CODE_1(ARDUINO_SERIAL_DEFINED_##i, (DECL_EVENT_0(n, p, i)), (DECL_EVENT_N(n, p, i)));

DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), serials, DECLARE_SERIAL_N)
#endif // PROP_LEN(serials) > 1
#elif DT_NODE_HAS_STATUS(DT_NODELABEL(arduino_serial), okay)
/* If serials node is not defined, tries to use arduino_serial */
arduino::ZephyrSerial Serial(DEVICE_DT_GET(DT_NODELABEL(arduino_serial)));
#else
arduino::ZephyrSerialStub Serial;
#endif

__attribute__((weak)) void serialEvent() {
}
#if (DT_PROP_LEN(DT_PATH(zephyr_user), serials) > 1)
DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), serials, DECLARE_SERIALEVENT_N)
#endif

void arduino::serialEventRun(void) {
	if (Serial.available()) {
		serialEvent();
	}
#if (DT_PROP_LEN(DT_PATH(zephyr_user), serials) > 1)
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), serials, CALL_SERIALEVENT_N)
#endif
}
