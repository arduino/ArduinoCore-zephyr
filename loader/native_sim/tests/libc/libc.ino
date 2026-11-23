/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Uses common libc, libm and atomic operations; exits 0 if all agree. */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <zephyr/sys/atomic.h>

static atomic_t counter;

void setup() {
	char buf[32];
	volatile double two = 2.0;
	int ok = 1;

	snprintf(buf, sizeof(buf), "%d-%s", 42, "ok");
	ok &= (strcmp(buf, "42-ok") == 0);
	ok &= (strtol("123", NULL, 10) == 123);
	ok &= (fabs(sqrt(two) * sqrt(two) - 2.0) < 1e-9);
	ok &= (floor(pow(two, 10)) == 1024.0);
	ok &= (sin(0.0) == 0.0);
	atomic_inc(&counter);
	ok &= (atomic_get(&counter) == 1);

	Serial.begin(115200);
	Serial.println(buf);
	exit(ok ? 0 : 1);
}

void loop() {
}
