/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ARDUINO_ZEPHYR_PDM_H
#define ARDUINO_ZEPHYR_PDM_H

#if !defined(ARDUINO_NANO33BLE) && !defined(ARDUINO_GIGA)
#error "Only Nano 33 BLE and Arduino GIGA boards are currently supported"
#endif

#include <Arduino.h>
#include <cstdint>
#include "PDMConfig.h"

namespace arduino {

class PDMClass {
public:
	PDMClass();
	virtual ~PDMClass();
	/* support 1 or 2 channels, sampleRate can be 16000 or 41667 */
	int begin(int channels = 1, int sampleRate = 16000);
	void end();
	virtual int available();
	virtual int read(void *buffer, size_t size);
	void onReceive(void (*)(void));
	void setGain(int gain);
	size_t getBufferSize();

private:
	bool pdm_init;
	bool active;

	struct {
		void *data;
		size_t size;
		size_t offset;
	} active_block;
};

} // namespace arduino

typedef arduino::PDMClass PDMClass;

extern PDMClass PDM;

#endif // ARDUINO_ZEPHYR_PDM_H
