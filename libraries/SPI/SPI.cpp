/*
 * Copyright (c) 2024 Ayush Singh <ayush@beagleboard.org>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "SPI.h"
#include <zephyr/kernel.h>

arduino::ZephyrSPI::ZephyrSPI(const struct device *spi) : spi_dev(spi) {
}

uint8_t arduino::ZephyrSPI::transfer(uint8_t data) {
	uint8_t rx = data;
	if (transfer(&rx, sizeof(rx), &config) < 0) {
		return 0;
	}
	return rx;
}

uint16_t arduino::ZephyrSPI::transfer16(uint16_t data) {
	uint16_t rx = data;
	if (transfer(&rx, sizeof(rx), &config16) < 0) {
		return 0;
	}
	return rx;
}

void arduino::ZephyrSPI::transfer(void *buf, size_t count) {
	int ret = transfer(buf, count, &config);
	(void)ret;
}

int arduino::ZephyrSPI::transfer(void *buf, size_t len, const struct spi_config *config) {
	const struct spi_buf tx_buf = {.buf = buf, .len = len};
	const struct spi_buf_set tx_buf_set = {
		.buffers = &tx_buf,
		.count = 1,
	};

	const struct spi_buf rx_buf = {.buf = buf, .len = len};
	const struct spi_buf_set rx_buf_set = {
		.buffers = &rx_buf,
		.count = 1,
	};

	return spi_transceive(spi_dev, config, &tx_buf_set, &rx_buf_set);
}

void arduino::ZephyrSPI::usingInterrupt(int interruptNumber) {
	ARG_UNUSED(interruptNumber);
}

void arduino::ZephyrSPI::notUsingInterrupt(int interruptNumber) {
	ARG_UNUSED(interruptNumber);
}

void arduino::ZephyrSPI::beginTransaction(SPISettings settings) {
	uint32_t mode = SPI_HOLD_ON_CS;

	// Set bus mode
	switch (settings.getBusMode()) {
	case SPI_CONTROLLER:
		break;
	case SPI_PERIPHERAL:
		mode |= SPI_OP_MODE_SLAVE;
		break;
	}

	// Set data format
	switch (settings.getBitOrder()) {
	case LSBFIRST:
		mode |= SPI_TRANSFER_LSB;
		break;
	case MSBFIRST:
		mode |= SPI_TRANSFER_MSB;
		break;
	}

	// Set data mode
	switch (settings.getDataMode()) {
	case SPI_MODE0:
		break;
	case SPI_MODE1:
		mode |= SPI_MODE_CPHA;
		break;
	case SPI_MODE2:
		mode |= SPI_MODE_CPOL;
		break;
	case SPI_MODE3:
		mode |= SPI_MODE_CPOL | SPI_MODE_CPHA;
		break;
	}

	// Set SPI configuration structure for 8-bit transfers
	memset(&config, 0, sizeof(struct spi_config));
	config.operation = mode | SPI_WORD_SET(8);
	config.frequency = max((uint32_t)SPI_MIN_CLOCK_FREQUENCY, settings.getClockFreq());

	// Set SPI configuration structure for 16-bit transfers
	memset(&config16, 0, sizeof(struct spi_config));
	config16.operation = mode | SPI_WORD_SET(16);
	config16.frequency = max((uint32_t)SPI_MIN_CLOCK_FREQUENCY, settings.getClockFreq());
}

void arduino::ZephyrSPI::endTransaction(void) {
	spi_release(spi_dev, &config);
}

void arduino::ZephyrSPI::attachInterrupt() {
}

void arduino::ZephyrSPI::detachInterrupt() {
}

void arduino::ZephyrSPI::begin() {

	/* Re-apply DEFAULT pinctrl state so shared pins
	 * are remuxed back to SPI after other peripherals have used them.
	 */
	(void)zephyr::arduino::init_dev_apply_pinctrl(spi_dev);
}

void arduino::ZephyrSPI::end() {
#ifdef CONFIG_DEVICE_DEINIT_SUPPORT
	(void)device_deinit(spi_dev);
#endif
}

#if DT_NODE_HAS_PROP(DT_PATH(zephyr_user), spis)
#if (DT_PROP_LEN(DT_PATH(zephyr_user), spis) > 1)
#define ARDUINO_SPI_DEFINED_0 1
#define DECL_SPI_0(n, p, i)   arduino::ZephyrSPI SPI(DEVICE_DT_GET(DT_PHANDLE_BY_IDX(n, p, i)));
#define DECL_SPI_N(n, p, i)   arduino::ZephyrSPI SPI##i(DEVICE_DT_GET(DT_PHANDLE_BY_IDX(n, p, i)));
#define DECLARE_SPI_N(n, p, i)                                                                     \
	COND_CODE_1(ARDUINO_SPI_DEFINED_##i, (DECL_SPI_0(n, p, i)), (DECL_SPI_N(n, p, i)))

/* Declare SPI, SPI1, SPI2, ... */
DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), spis, DECLARE_SPI_N)

#undef DECLARE_SPI_N
#undef DECL_SPI_N
#undef DECL_SPI_0
#undef ARDUINO_SPI_DEFINED_0
#else  // PROP_LEN(spis) > 1
/* When PROP_LEN(spis) == 1, DT_FOREACH_PROP_ELEM work not correctly. */
arduino::ZephyrSPI SPI(DEVICE_DT_GET(DT_PHANDLE_BY_IDX(DT_PATH(zephyr_user), spis, 0)));
#endif // HAS_PORP(spis)
/* If spis node is not defined, tries to use arduino_spi */
#elif DT_NODE_EXISTS(DT_NODELABEL(arduino_spi))
arduino::ZephyrSPI SPI(DEVICE_DT_GET(DT_NODELABEL(arduino_spi)));
#endif
