/*
 * Copyright (c) 2022 Dhruva Gole
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <zephyr/sys/ring_buffer.h>
#include <zephyr/kernel.h>
#include <api/HardwareSerial.h>

namespace arduino {

class ZephyrSerialStub : public HardwareSerial {
public:
	void begin(__attribute__((unused)) unsigned long baudRate) {
	}

	void begin(__attribute__((unused)) unsigned long baudrate,
			   __attribute__((unused)) uint16_t config) {
	}

	void end() {
	}

	int available() {
		return 0;
	}

	int peek() {
		return 0;
	}

	int read() {
		return 0;
	}

	void flush() {
	}

	size_t write(const uint8_t data) {
		printk("%c", static_cast<char>(data));
		return 1;
	}

	operator bool() {
		return true;
	}
};

class ZephyrSerial : public HardwareSerial {
public:
	template <int SZ> class ZephyrSerialBuffer {
		friend arduino::ZephyrSerial;
		struct ring_buf ringbuf;
		uint8_t buffer[SZ];
		struct k_sem sem;

		ZephyrSerialBuffer() {
			k_sem_init(&sem, 1, 1);
			ring_buf_init(&ringbuf, sizeof(buffer), buffer);
		}
	};

	ZephyrSerial(const struct device *dev) : uart(dev) {
	}

	void begin(unsigned long baudrate, uint16_t config);

	void begin(unsigned long baudrate) {
		begin(baudrate, SERIAL_8N1);
	}

	void flush();

	void end() {
	}

	size_t write(const uint8_t *buffer, size_t size);

	size_t write(const uint8_t data) {
		return write(&data, 1);
	}

	using Print::write; // pull in write(str) and write(buf, size) from Print
	int available();
	int availableForWrite();
	int peek();
	int read();

#if CONFIG_ARDUINO_SERIAL_PRINTF
	void printf(const char *fmt, ...);
#endif

	operator bool() {
		return true;
	}

	friend class SerialUSB_;

protected:
	void IrqHandler();
	static void IrqDispatch(const struct device *dev, void *data);

	const struct device *uart;
	ZephyrSerialBuffer<CONFIG_ARDUINO_API_SERIAL_BUFFER_SIZE> tx;
	ZephyrSerialBuffer<CONFIG_ARDUINO_API_SERIAL_BUFFER_SIZE> rx;
};

} // namespace arduino

#if DT_NODE_HAS_PROP(DT_PATH(zephyr_user), serials)
#if !(DT_NODE_HAS_PROP(DT_PATH(zephyr_user), cdc_acm) &&                                           \
	  (CONFIG_USB_CDC_ACM || CONFIG_USBD_CDC_ACM_CLASS))
// If CDC USB, use that object as Serial (and SerialUSB)
extern arduino::ZephyrSerial Serial;
#endif
#if (DT_PROP_LEN(DT_PATH(zephyr_user), serials) > 1)
#define SERIAL_DEFINED_0                 1
#define EXTERN_SERIAL_N(i)               extern arduino::ZephyrSerial Serial##i;
#define DECLARE_EXTERN_SERIAL_N(n, p, i) COND_CODE_1(SERIAL_DEFINED_##i, (), (EXTERN_SERIAL_N(i)))

/* Declare Serial1, Serial2, ... */
DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), serials, DECLARE_EXTERN_SERIAL_N)

#undef DECLARE_EXTERN_SERIAL_N
#undef EXTERN_SERIAL_N
#undef SERIAL_DEFINED_0
#endif
#elif DT_NODE_HAS_STATUS(DT_NODELABEL(arduino_serial), okay)
extern arduino::ZephyrSerial Serial;
#else
extern arduino::ZephyrSerialStub Serial;
#endif
