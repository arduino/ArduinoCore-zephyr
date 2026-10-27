/*
 * Copyright (c) 2022 Dhruva Gole
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOADER_PROVIDES_EXTRA_USB_CLASSES

#define CRYPTO_WIRE Wire2

#define SECURE_ELEMENT_IS_ECCX08

// TODO: correctly handle these legacy defines
#define MOSI 0
#define MISO 0
#define SCK  0
#define SS   0
#define SDA  0
#define SCL  0

#define ST_VREF_MASK 0x80

#define AR_DEFAULT      0
#define AR_INTERNAL1V8  (SYSCFG_VREFBUF_VOLTAGE_SCALE2 | ST_VREF_MASK)
#define AR_INTERNAL2V5  (SYSCFG_VREFBUF_VOLTAGE_SCALE0 | ST_VREF_MASK)
#define AR_INTERNAL2V05 (SYSCFG_VREFBUF_VOLTAGE_SCALE1 | ST_VREF_MASK)
#define AR_EXTERNAL     5
#define AR_INTERNAL     AR_INTERNAL2V5
