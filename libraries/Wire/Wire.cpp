/*
 * Copyright (c) 2022 Dhruva Gole
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <Wire.h>
#include <stddef.h>
#include <zephyr/sys/util_macro.h>

// Helper function to get ZephyrI2C instance from config pointer.
static arduino::ZephyrI2C *getInstance(struct i2c_target_config *config) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
	return reinterpret_cast<arduino::ZephyrI2C *>(reinterpret_cast<char *>(config) -
												  offsetof(arduino::ZephyrI2C, i2c_cfg));
#pragma GCC diagnostic pop
}

static int i2c_target_stop_cb(struct i2c_target_config *config) {
	arduino::ZephyrI2C *instance = getInstance(config);
	return instance->stopCallback(config);
}

static int i2c_target_write_requested_cb(struct i2c_target_config *config) {
	arduino::ZephyrI2C *instance = getInstance(config);
	return instance->writeRequestedCallback(config);
}

static int i2c_target_write_received_cb(struct i2c_target_config *config, uint8_t val) {
	arduino::ZephyrI2C *instance = getInstance(config);
	return instance->writeReceivedCallback(config, val);
}

static int i2c_target_read_requested_cb(struct i2c_target_config *config, uint8_t *val) {
	arduino::ZephyrI2C *instance = getInstance(config);
	return instance->readRequestedCallback(config, val);
}

static int i2c_target_read_processed_cb(struct i2c_target_config *config, uint8_t *val) {
	arduino::ZephyrI2C *instance = getInstance(config);
	return instance->readProcessedCallback(config, val);
}

// I2C target callback structure.
static struct i2c_target_callbacks target_callbacks = {
	.write_requested = i2c_target_write_requested_cb,
	.read_requested = i2c_target_read_requested_cb,
	.write_received = i2c_target_write_received_cb,
	.read_processed = i2c_target_read_processed_cb,
	.stop = i2c_target_stop_cb,
};

arduino::ZephyrI2C::ZephyrI2C(const struct device *i2c) : i2c_cfg({0}), i2c_dev(i2c) {
	ring_buf_init(&txRingBuffer.rb, sizeof(txRingBuffer.buffer), txRingBuffer.buffer);
	ring_buf_init(&rxRingBuffer.rb, sizeof(rxRingBuffer.buffer), rxRingBuffer.buffer);
}

void arduino::ZephyrI2C::begin() {

	/* Re-apply DEFAULT pinctrl state so shared pins
	 * are remuxed back to I2C after other peripherals have used them.
	 */
	(void)zephyr::arduino::init_dev_apply_pinctrl(i2c_dev);
}

void arduino::ZephyrI2C::begin(uint8_t slaveAddr) {
	begin();
	i2c_cfg.address = slaveAddr;
	i2c_cfg.callbacks = &target_callbacks;

	// Register I2C target
	i2c_target_register(i2c_dev, &i2c_cfg);
}

void arduino::ZephyrI2C::end() {
	// Unregister I2C target
	if (i2c_cfg.address) {
		i2c_target_unregister(i2c_dev, &i2c_cfg);
		memset(&i2c_cfg, 0, sizeof(i2c_cfg));
	}
#ifdef CONFIG_DEVICE_DEINIT_SUPPORT
	(void)device_deinit(i2c_dev);
#endif
}

void arduino::ZephyrI2C::setClock(uint32_t freq) {
	uint8_t speed;

	if (freq == 100000) {
		speed = I2C_SPEED_STANDARD;
	} else if (freq == 400000) {
		speed = I2C_SPEED_FAST;
	} else if (freq == 1000000) {
		speed = I2C_SPEED_FAST_PLUS;
	} else {
		speed = I2C_SPEED_STANDARD;
	}

	i2c_configure(i2c_dev, I2C_SPEED_SET(speed) | I2C_MODE_CONTROLLER);
}

void arduino::ZephyrI2C::beginTransmission(uint8_t address) {
	_address = address;
	ring_buf_reset(&txRingBuffer.rb);
	ring_buf_reset(&rxRingBuffer.rb);
}

uint8_t arduino::ZephyrI2C::endTransmission(bool stopBit) {
	int ret = -EIO;
	uint8_t *buf = NULL;
	size_t max = ring_buf_capacity_get(&txRingBuffer.rb);
	size_t len = ring_buf_get_claim(&txRingBuffer.rb, &buf, max);

	ARG_UNUSED(stopBit);

	ret = i2c_write(i2c_dev, buf, len, _address);

	// Must be called even if 0 bytes claimed.
	ring_buf_get_finish(&txRingBuffer.rb, len);

	return ret ? 1 : 0;
}

uint8_t arduino::ZephyrI2C::endTransmission(void) {
	return endTransmission(true);
}

size_t arduino::ZephyrI2C::requestFrom(uint8_t address, size_t len_in, bool stopBit) {
	int ret = -EIO;
	uint8_t *buf = NULL;
	size_t len;

	ARG_UNUSED(stopBit);

	len = ring_buf_put_claim(&rxRingBuffer.rb, &buf, len_in);
	if (len && buf) {
		ret = i2c_read(i2c_dev, buf, len, address);
	}

	if (ret != 0) {
		// In case i2c_read fails don't claim space in the ring buffer
		len = 0;
	}

	// Must be called even if 0 bytes claimed.
	ring_buf_put_finish(&rxRingBuffer.rb, len);

	return len;
}

size_t arduino::ZephyrI2C::requestFrom(uint8_t address, size_t len) {
	return requestFrom(address, len, true);
}

size_t arduino::ZephyrI2C::write(uint8_t data) {
	return ring_buf_put(&txRingBuffer.rb, &data, 1);
}

size_t arduino::ZephyrI2C::write(const uint8_t *buffer, size_t size) {
	return ring_buf_put(&txRingBuffer.rb, buffer, size);
}

int arduino::ZephyrI2C::read() {
	uint8_t buf;
	if (ring_buf_get(&rxRingBuffer.rb, &buf, 1)) {
		return (int)buf;
	}
	return -1;
}

int arduino::ZephyrI2C::available() {
	return ring_buf_size_get(&rxRingBuffer.rb);
}

int arduino::ZephyrI2C::peek() {
	uint8_t buf;
	if (ring_buf_peek(&rxRingBuffer.rb, &buf, 1)) {
		return (int)buf;
	}
	return -1;
}

void arduino::ZephyrI2C::flush() {
}

void arduino::ZephyrI2C::onReceive(voidFuncPtrParamInt cb) {
	onReceiveCb = cb;
}

void arduino::ZephyrI2C::onRequest(voidFuncPtr cb) {
	onRequestCb = cb;
}

int arduino::ZephyrI2C::writeRequestedCallback(struct i2c_target_config *config) {
	ARG_UNUSED(config);

	// Reset the buffer on write requests.
	ring_buf_reset(&rxRingBuffer.rb);
	return 0;
}

int arduino::ZephyrI2C::writeReceivedCallback(struct i2c_target_config *config, uint8_t val) {
	size_t len = ring_buf_size_get(&rxRingBuffer.rb);
	size_t max = ring_buf_capacity_get(&rxRingBuffer.rb);

	ARG_UNUSED(config);

	// If the buffer is about to overflow, invoke the callback
	// with the current length.
	if (onReceiveCb && ((len + 1) > max)) {
		onReceiveCb(len);
	}

	return ring_buf_put(&rxRingBuffer.rb, &val, 1) ? 0 : -1;
}

int arduino::ZephyrI2C::readRequestedCallback(struct i2c_target_config *config, uint8_t *val) {
	// Reset the buffer on read requests.
	ring_buf_reset(&txRingBuffer.rb);

	if (onRequestCb) {
		onRequestCb();
	}

	return readProcessedCallback(config, val);
}

int arduino::ZephyrI2C::readProcessedCallback(struct i2c_target_config *config, uint8_t *val) {
	ARG_UNUSED(config);

	*val = 0xFF;
	ring_buf_get(&txRingBuffer.rb, val, 1);
	// Returning a negative value here is not handled gracefully and
	// causes the target/board to stop responding (at least with ST).
	return 0;
}

int arduino::ZephyrI2C::stopCallback(struct i2c_target_config *config) {
	ARG_UNUSED(config);

	// If the RX buffer is not empty invoke the callback with the
	// remaining data length.
	if (onReceiveCb) {
		size_t len = ring_buf_size_get(&rxRingBuffer.rb);
		if (len) {
			onReceiveCb(len);
		}
	}
	return 0;
}

#if DT_NODE_HAS_PROP(DT_PATH(zephyr_user), i2cs)
#if (DT_PROP_LEN(DT_PATH(zephyr_user), i2cs) > 1)
#define ARDUINO_WIRE_DEFINED_0 1
#define DECL_WIRE_0(n, p, i)   arduino::ZephyrI2C Wire(DEVICE_DT_GET(DT_PHANDLE_BY_IDX(n, p, i)));
#define DECL_WIRE_N(n, p, i)   arduino::ZephyrI2C Wire##i(DEVICE_DT_GET(DT_PHANDLE_BY_IDX(n, p, i)));
#define DECLARE_WIRE_N(n, p, i)                                                                    \
	COND_CODE_1(ARDUINO_WIRE_DEFINED_##i, (DECL_WIRE_0(n, p, i)), (DECL_WIRE_N(n, p, i)))

/* Declare Wire, Wire1, Wire2, ... */
DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), i2cs, DECLARE_WIRE_N)

#undef DECLARE_WIRE_N
#undef DECL_WIRE_N
#undef DECL_WIRE_0
#undef ARDUINO_WIRE_DEFINED_0
#elif (DT_PROP_LEN(DT_PATH(zephyr_user), i2cs) == 1)
/* When PROP_LEN(i2cs) == 1, DT_FOREACH_PROP_ELEM work not correctly. */
arduino::ZephyrI2C Wire(DEVICE_DT_GET(DT_PHANDLE_BY_IDX(DT_PATH(zephyr_user), i2cs, 0)));
#endif // HAS_PORP(i2cs)
/* If i2cs node is not defined, tries to use arduino_i2c */
#elif DT_NODE_EXISTS(DT_NODELABEL(arduino_i2c))
arduino::ZephyrI2C Wire(DEVICE_DT_GET(DT_NODELABEL(arduino_i2c)));
#endif
