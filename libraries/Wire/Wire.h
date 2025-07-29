/*
 * Copyright (c) 2022 Dhruva Gole
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <Arduino.h>
#include <api/HardwareI2C.h>
#include <api/Print.h>
#include <zephyr/sys/ring_buffer.h>

typedef void (*voidFuncPtrParamInt)(int);

namespace arduino {

struct i2c_ring {
  struct ring_buf rb;
  uint8_t buffer[256];
};

class ZephyrI2C : public HardwareI2C {
public:
  ZephyrI2C(const struct device* i2c);

  virtual void begin();
  virtual void end();
  virtual void begin(uint8_t address);
  virtual void setClock(uint32_t freq);

  virtual void beginTransmission(uint8_t address);
  virtual uint8_t endTransmission(bool stopBit);
  virtual uint8_t endTransmission(void);

  virtual size_t requestFrom(uint8_t address, size_t len, bool stopBit);
  virtual size_t requestFrom(uint8_t address, size_t len);

  virtual void onReceive(void (*)(int));
  virtual void onRequest(void (*)(void));

  virtual size_t write(uint8_t data);
  virtual size_t write(int data) { return write((uint8_t)data); };
  virtual size_t write(const uint8_t *buffer, size_t size);
  using Print::write;
  virtual int read();
  virtual int peek();
  virtual void flush();
  virtual int available();

  // I2C target callbacks
  int writeRequestedCallback(struct i2c_target_config *config);
  int writeReceivedCallback(struct i2c_target_config *config, uint8_t val);
  int readRequestedCallback(struct i2c_target_config *config, uint8_t *val);
  int readProcessedCallback(struct i2c_target_config *config, uint8_t *val);
  int stopCallback(struct i2c_target_config *config);

  struct i2c_target_config i2c_cfg;

private:
  int _address;

  struct i2c_ring txRingBuffer;
  struct i2c_ring rxRingBuffer;
  const struct device *i2c_dev;

  voidFuncPtr onRequestCb = NULL;
  voidFuncPtrParamInt onReceiveCb = NULL;
};

} // namespace arduino

#if DT_NODE_HAS_PROP(DT_PATH(zephyr_user), i2cs) && (DT_PROP_LEN(DT_PATH(zephyr_user), i2cs) > 1)
#define ARDUINO_WIRE_DEFINED_0 1
#define DECL_EXTERN_WIRE_0(i)  extern arduino::ZephyrI2C Wire;
#define DECL_EXTERN_WIRE_N(i)  extern arduino::ZephyrI2C Wire##i;
#define DECLARE_EXTERN_WIRE_N(n, p, i)                                                             \
	COND_CODE_1(ARDUINO_WIRE_DEFINED_##i, (DECL_EXTERN_WIRE_0(i)), (DECL_EXTERN_WIRE_N(i)))

/* Declare Wire, Wire1, Wire2, ... */
DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), i2cs, DECLARE_EXTERN_WIRE_N)

#undef DECLARE_EXTERN_WIRE_N
#undef DECL_EXTERN_WIRE_N
#undef DECL_EXTERN_WIRE_0
#undef ARDUINO_WIRE_DEFINED_0
#else
extern arduino::ZephyrI2C Wire;
#endif

typedef arduino::ZephyrI2C TwoWire;
