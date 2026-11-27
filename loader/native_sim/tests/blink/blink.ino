/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

void setup() {
	pinMode(LED_BUILTIN, OUTPUT);
	/* inputs must not show up in the GPIO write log */
	pinMode(7, INPUT_PULLUP);
}

void loop() {
	digitalWrite(LED_BUILTIN, HIGH);
	delay(100);
	digitalWrite(LED_BUILTIN, LOW);
	delay(100);
}
