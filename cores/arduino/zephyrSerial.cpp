/*
 * Copyright (c) 2022 Dhruva Gole
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
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
		.baudrate = static_cast<uint32_t>(baud),
		.parity = conf_parity(conf),
		.stop_bits = conf_stop_bits(conf),
		.data_bits = conf_data_bits(conf),
		.flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
	};

	/* Re-apply DEFAULT pinctrl state so shared pins
	 * are remuxed back to Serial after other peripherals have used them.
	 */
	(void)zephyr::arduino::init_dev_apply_pinctrl(uart);

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

	k_sem_take(&tx.sem, K_FOREVER);
	ret = ring_buf_space_get(&tx.ringbuf);
	k_sem_give(&tx.sem);

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

#if DT_NODE_HAS_PROP(DT_PATH(zephyr_user), serials)
#define DEFINE_SERIAL_N(n, p, i)                                                                   \
	arduino::ZephyrSerial ZARD_SERIAL_NAME(i)(DEVICE_DT_GET(DT_PHANDLE_BY_IDX(n, p, i)));
DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), serials, DEFINE_SERIAL_N)

#elif DT_NODE_HAS_STATUS_OKAY(DT_NODELABEL(arduino_serial))
arduino::ZephyrSerial ZARD_SERIAL_NAME(0)(DEVICE_DT_GET(DT_NODELABEL(arduino_serial)));
#endif

#if ZARD_FIRST_SERIAL_IS_STUB
arduino::ZephyrSerialStub Serial;
#endif

#define DEFINE_WEAK_SERIALEVENT_N(n, s)                                                            \
	__attribute__((weak)) void CONCAT(serial, ZARD_SERIAL_STEM(n), Event)() {                      \
	}

#define CALL_SERIALEVENT_N(n, s)                                                                   \
	if (ZARD_SERIAL_NAME(n).available())                                                           \
		CONCAT(serial, ZARD_SERIAL_STEM(n), Event)();

LISTIFY(ZARD_GENERIC_SERIAL_COUNT, DEFINE_WEAK_SERIALEVENT_N, ())

void arduino::serialEventRun(void) {
	LISTIFY(ZARD_GENERIC_SERIAL_COUNT, CALL_SERIALEVENT_N, ())
}
