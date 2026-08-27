/*
 * Copyright (c) 2022 Dhruva Gole
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#define LEDR (25u)
#define LEDG (26u)
#define LEDB (27u)

/*
 * This variant is shared by two boards.txt entries: the bare 'nano_chandler'
 * module and 'nano_chandler_monica', which is the same board plus the
 * "nano_chandler_monica" shield. On the carrier, sercom1 is dedicated to the
 * ESP8266 AT UART and is dropped from the "serials" list, so every Serial
 * object shifts down by one: RS485 (sercom0) is Serial3 on the bare module and
 * Serial2 on Monica. ARDUINO_NANO_CHANDLER_MONICA comes from the board's
 * build.board property in boards.txt.
 */
#ifdef ARDUINO_NANO_CHANDLER_MONICA
#define RS485_SERIAL_PORT Serial2
#else
#define RS485_SERIAL_PORT Serial3
#endif

#define RS485_DEFAULT_DE_PIN (28u)
#define RS485_DEFAULT_RE_PIN (29u)

/* Same values, not aliased to RS485_DEFAULT_*_PIN to avoid a circular
 * definition with RS485.h's own "#define RS485_DEFAULT_DE_PIN CUSTOM_...". */
#define CUSTOM_RS485_DEFAULT_DE_PIN (28u)
#define CUSTOM_RS485_DEFAULT_RE_PIN (29u)
