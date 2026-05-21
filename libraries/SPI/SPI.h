/*
 * Copyright (c) 2024 Ayush Singh <ayush@beagleboard.org>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <Arduino.h>
#include <api/HardwareSPI.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/spi.h>
#include <zephyrPinctrl.h>

#undef SPI
#undef SPI1

#define SPR0 0
#define SPR1 1
#define CPHA 2
#define CPOL 3
#define MSTR 4
#define DORD 5
#define SPE  6
#define SPIE 7

#define SPI_HAS_PERIPHERAL_MODE (1)

// TODO:
// This depends on the clock settings, can't be used for all boards.
#ifndef SPI_MIN_CLOCK_FREQUENCY
#define SPI_MIN_CLOCK_FREQUENCY 1000000
#endif

/* Count the number of GPIOs for limit of number of interrupts */
#define INTERRUPT_HELPER(n, p, i) 1
#define INTERRUPT_COUNT                                                                            \
	DT_FOREACH_PROP_ELEM_SEP(DT_PATH(zephyr_user), digital_pin_gpios,            \
                           INTERRUPT_HELPER, (+))

namespace arduino {
class ZephyrSPI : public HardwareSPI {
public:
	ZephyrSPI(const struct device *spi, const struct pinctrl_dev_config *pinctrl_config = nullptr,
			  uint8_t pinctrl_state = PINCTRL_STATE_DEFAULT);

	virtual uint8_t transfer(uint8_t data);
	virtual uint16_t transfer16(uint16_t data);
	virtual void transfer(void *buf, size_t count);

	// Transaction Functions
	virtual void usingInterrupt(int interruptNumber);
	virtual void notUsingInterrupt(int interruptNumber);
	virtual void beginTransaction(SPISettings settings);
	virtual void endTransaction(void);

	// SPI Configuration methods
	virtual void attachInterrupt();
	virtual void detachInterrupt();

	virtual void begin();
	virtual void end();

private:
	int transfer(void *buf, size_t len, const struct spi_config *config);
	int applyPinctrlState();

protected:
	const struct device *spi_dev;
	const struct pinctrl_dev_config *pinctrl_config;
	uint8_t pinctrl_state;
	struct spi_config config;
	struct spi_config config16;
	int interrupt[INTERRUPT_COUNT];
	size_t interrupt_pos = 0;
};

} // namespace arduino

#if DT_NODE_HAS_PROP(DT_PATH(zephyr_user), spis) && (DT_PROP_LEN(DT_PATH(zephyr_user), spis) > 1)
#define ARDUINO_SPI_DEFINED_0 1
#define DECL_EXTERN_SPI_0(i)  extern arduino::ZephyrSPI SPI
#define DECL_EXTERN_SPI_N(i)  extern arduino::ZephyrSPI SPI##i
#define DECLARE_EXTERN_SPI_N(n, p, i)                                                              \
	COND_CODE_1(ARDUINO_SPI_DEFINED_##i, (DECL_EXTERN_SPI_0(i);), (DECL_EXTERN_SPI_N(i);))

/* Declare SPI, SPI1, SPI2, ... */
DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), spis, DECLARE_EXTERN_SPI_N)

#undef DECLARE_EXTERN_SPI_N
#undef DECL_EXTERN_SPI_N
#undef DECL_EXTERN_SPI_0
#undef ARDUINO_SPI_DEFINED_0
#else
extern arduino::ZephyrSPI SPI;
#endif

#if DT_NODE_HAS_PROP(DT_PATH(zephyr_user), spi1_alt_default) &&                                     \
	(!DT_NODE_HAS_PROP(DT_PATH(zephyr_user), spis) ||                                               \
	 (DT_PROP_LEN(DT_PATH(zephyr_user), spis) <= 1))
extern arduino::ZephyrSPI SPI1;
#endif

/* Serial Peripheral Control Register */

using arduino::SPI_MODE0;
using arduino::SPI_MODE1;
using arduino::SPI_MODE2;
using arduino::SPI_MODE3;
using arduino::SPISettings;
