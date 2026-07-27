/*
 * Copyright (c) 2026 KurtE
 * SPDX-License-Identifier: Apache-2.0
 */
#include <CPUTemperature.h>

void setup() {
  Serial.begin(115200);

  while (!Serial && millis() < 5000) {
  }
}

void loop() {
  double temp = CPUTemperature();
  Serial.print("CPU Temp C:");
  Serial.print(temp, 2);
  if (!isnan(temp)) {
    Serial.print(" F:");
    Serial.print(temp * 1.8 + 32.0, 2);
  }
  Serial.println();
  delay(5000);
}
