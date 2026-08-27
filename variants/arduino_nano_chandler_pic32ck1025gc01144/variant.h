/*
 * Copyright (c) 2022 Dhruva Gole
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#define LEDR (39u)
#define LEDG (40u)
#define LEDB (41u)

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

#define RS485_DEFAULT_DE_PIN (42u)
#define RS485_DEFAULT_RE_PIN (43u)

/* Literals, not aliases — RS485.h defines RS485_DEFAULT_*_PIN from these. */
#define CUSTOM_RS485_DEFAULT_DE_PIN (42u)
#define CUSTOM_RS485_DEFAULT_RE_PIN (43u)
