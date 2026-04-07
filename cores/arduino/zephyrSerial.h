/*
 * Copyright (c) 2022 Dhruva Gole
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
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
#ifdef CONFIG_DEVICE_DEINIT_SUPPORT
		(void)device_deinit(uart);
#endif
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

/* Return the index of it if matched, oterwise return an empty string. */
#define ZARD_SERIAL_MATCH(n, p, i, node)                                                           \
	COND_CODE_1(DT_SAME_NODE(DT_PHANDLE_BY_IDX(n, p, i), node), (i), ())

/* Only matched device returns non-empty value, so the overall expansion is
 * matched device's index.
 */
#define ZARD_SERIAL_INDEXOF(node)                                                                  \
	DT_FOREACH_PROP_ELEM_VARGS(DT_PATH(zephyr_user), serials, ZARD_SERIAL_MATCH, node)

/* Serial object associated with the Zephyr console. */
#define ARDUINO_CONSOLE_SERIAL ZARD_SERIAL_NAME(ZARD_SERIAL_INDEXOF(DT_CHOSEN(zephyr_console)))

/* Serial object associated with the first HW serial (usually on D0/D1). */
#define ARDUINO_HARDWARE_SERIAL ZARD_SERIAL_NAME(0)

#if DT_NODE_HAS_PROP(DT_PATH(zephyr_user), cdc_acm_serial)
/* Devicetree requires a SerialUSB object for 'Serial'. */
#define ZARD_SKIP_FIRST_SERIAL 1
#if (CONFIG_USB_CDC_ACM || CONFIG_USBD_CDC_ACM_CLASS)
/* SerialUSB can be compiled in the project. */
#define ZARD_FIRST_SERIAL_IS_SERIALUSB 1
#else
/* SerialUSB is required but no driver was enabled for the USB CDC ACM device.
 * Define a stub Serial object to avoid build errors.
 */
#define ZARD_FIRST_SERIAL_IS_STUB 1
#endif
#endif

#if DT_NODE_HAS_PROP(DT_PATH(zephyr_user), arduino_router_serial)
/* If the board has an arduino,router-serial node, and the currently used
 * library has the support, then the 'Serial' object is actually the same as
 * the Monitor object in the Arduino_RouterBridge library.
 */
#define ZARD_SKIP_FIRST_SERIAL              1
#define ZARD_FIRST_SERIAL_IS_ARDUINO_ROUTER 1
#define ARDUINO_ROUTER_PHANDLE              DT_PROP(DT_PATH(zephyr_user), arduino_router_serial)
#define ARDUINO_ROUTER_SERIAL               ZARD_SERIAL_NAME(ZARD_SERIAL_INDEXOF(ARDUINO_ROUTER_PHANDLE))
#endif

/* Name of a Serial object for a given index. */
#define ZARD_SERIAL_NAME(n) CONCAT(Serial, ZARD_SERIAL_STEM(n))

/*
 * Determine the number of generic ZephyrSerial objects to instantiate.
 */

#if DT_NODE_HAS_PROP(DT_PATH(zephyr_user), serials)
/* The array property; the total number is the length of the array. */
#define ZARD_GENERIC_SERIAL_COUNT DT_PROP_LEN(DT_PATH(zephyr_user), serials)

#elif DT_NODE_HAS_STATUS_OKAY(DT_NODELABEL(arduino_serial))
/* A single node with compatible "arduino,serial" is present. */
#define ZARD_GENERIC_SERIAL_COUNT 1

#elif ZARD_SKIP_FIRST_SERIAL
/* Only a special Serial is present. */
#define ZARD_GENERIC_SERIAL_COUNT 0

#else
/* Neither 'serials' property, nor 'arduino,serial' node, nor a special Serial
 * is present. Define a stub Serial object to avoid build errors.
 */
#define ZARD_GENERIC_SERIAL_COUNT 0
#define ZARD_SKIP_FIRST_SERIAL    1
#define ZARD_FIRST_SERIAL_IS_STUB 1
#endif

#if ZARD_SKIP_FIRST_SERIAL
// Map index 0 to 'Serial1', index 1 to 'Serial2', etc.
#define ZARD_SERIAL_STEM(i) UTIL_INC(i)
#else
// Map index 0 to 'Serial', index 1 to 'Serial1', etc.
#define ZARD_SERIAL_STEM(i) COND_CODE_0(i, (), (i))
#endif

#if ZARD_FIRST_SERIAL_IS_STUB
extern arduino::ZephyrSerialStub Serial;
#endif

#if ZARD_GENERIC_SERIAL_COUNT > 0
/* Declare all generic ZephyrSerial objects */
#define DECLARE_SERIAL_N(n, s) extern arduino::ZephyrSerial ZARD_SERIAL_NAME(n);
LISTIFY(ZARD_GENERIC_SERIAL_COUNT, DECLARE_SERIAL_N, ())
#undef DECLARE_SERIAL_N
#endif
