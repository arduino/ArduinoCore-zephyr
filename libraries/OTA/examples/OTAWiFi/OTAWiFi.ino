/*
 * OTA update over WiFi.
 *
 * Downloads an update from an HTTP server and reboots to install it.  The
 * downloaded sketch replaces this one, so it must contain OTA code too if
 * you want to keep updating it over the air.
 *
 * Build the update (e.g. Blink) with Sketch > Export Compiled Binary
 * (arduino-cli: compile -e) and serve its build folder:
 *
 *   python3 -m http.server 8000 --directory <sketch folder>/build/<fqbn>
 *
 * See libraries/OTA/README.md for details.
 *
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ota_download.h>
#include <ZephyrClient.h>
#include <WiFi.h>
#include "arduino_secrets.h"

// 0: update the sketch only, 1: update loader + sketch
#define OTA_FULL 0

// IP address of the computer running the HTTP server (not the board's)
const char server[] = "192.168.1.100";
const uint16_t port = 8000;

// File to download from the server.  "Blink" is only an example: replace it
// with the name of the sketch you exported as the update, keeping the
// ".ino.elf-zsk.bin" suffix (sketch only) or ".ino.elf-zsk.bin-bundle.bin"
// suffix (loader + sketch) that the build adds.
#if OTA_FULL
const char path[] = "/Blink.ino.elf-zsk.bin-bundle.bin";
#define ota_download ota_loader_download
#define ota_ready    ota_loader_ready
#define ota_start    ota_loader_start
#else
const char path[] = "/Blink.ino.elf-zsk.bin";
#define ota_download ota_sketch_download
#define ota_ready    ota_sketch_ready
#define ota_start    ota_sketch_start
#endif

ZephyrClient client;

void setup() {
  Serial.begin(115200);
  while (!Serial) {
  }

  while (WiFi.begin(SECRET_SSID, SECRET_PASS) != WL_CONNECTED) {
    Serial.println("WiFi connection failed, retrying...");
    delay(5000);
  }

  Serial.print("Downloading ");
  Serial.println(path);
  long len = ota_download(client, server, port, path);
  if (len < 0) {
    Serial.print("Download failed, error ");
    Serial.println(len);
    return;
  }

  Serial.print("Downloaded ");
  Serial.print(len);
  Serial.println(" bytes, rebooting to install the update");
  Serial.flush();
  delay(100); // let the host read the output before the reboot

  if (ota_ready() == 0) {
    ota_start();
  }
  Serial.println("Failed to mark the update as ready");
}

void loop() {
}
