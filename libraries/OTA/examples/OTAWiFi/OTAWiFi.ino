/*
 * OTA sketch update over WiFi.
 *
 * Downloads a raw .bin sketch image over plain HTTP and installs it on
 * the next boot.  No .ota container — the server hosts the plain
 * "<sketch>.elf-zsk.bin" artifact produced by the build.  Serve it with:
 *
 *   python3 -m http.server 8000
 *
 * On boards booted by MCUboot (Nano XL Link) sketch and full updates are the
 * same: serve "<sketch>.ino.signed.elf-zsk.bin" instead.
 *
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ota.h>
#include <ZephyrClient.h>
#include <WiFi.h>
#include <zephyr/autoconf.h>

#ifndef CONFIG_OTA
#error "This variant's loader was built without CONFIG_OTA."
#endif

#define UPDATE_TYPE OTA_SKETCH

#include "arduino_secrets.h"
char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

const char *server = "10.10.10.10";
const uint16_t port = 8000;
const char *path = "/UPDATE.bin";

ZephyrClient client;

static long httpGet(Client &c, const char *host, uint16_t port,
                    const char *path, enum ota_type type) {
  if (!c.connect(host, port)) {
    Serial.println("Connection failed");
    return -1;
  }

  c.print("GET ");
  c.print(path);
  c.print(" HTTP/1.1\r\nHost: ");
  c.print(host);
  c.print("\r\nConnection: close\r\n\r\n");

  char line[128];
  bool first = true;
  long content_length = -1;
  uint32_t last = millis();

  while (true) {
    if (!c.available()) {
      if (!c.connected() || millis() - last > 10000) {
        c.stop();
        return -1;
      }
      continue;
    }
    last = millis();

    size_t n = c.readBytesUntil('\n', line, sizeof(line) - 1);
    line[n] = '\0';
    if (n > 0 && line[n - 1] == '\r') {
      line[n - 1] = '\0';
    }

    if (first) {
      if (strncmp(line, "HTTP/1.", 7) != 0 || atoi(line + 9) != 200) {
        Serial.print("HTTP error: ");
        Serial.println(line);
        c.stop();
        return -1;
      }
      first = false;
      continue;
    }

    if (line[0] == '\0') {
      break;
    }

    if (strncasecmp(line, "Content-Length:", 15) == 0) {
      content_length = atol(line + 15);
    }
  }

  if (content_length <= 0) {
    Serial.println("Missing or zero Content-Length");
    c.stop();
    return -1;
  }

  if (ota_write_begin(type, content_length) < 0) {
    Serial.println("Cannot start the update (too large?)");
    c.stop();
    return -1;
  }

  uint8_t buf[512];
  long received = 0;
  last = millis();

  while (received < content_length) {
    int avail = c.available();
    if (avail <= 0) {
      if (!c.connected() || millis() - last > 10000) {
        break;
      }
      continue;
    }

    size_t want = (size_t)(content_length - received);
    if (want > sizeof(buf)) {
      want = sizeof(buf);
    }
    int got = c.read(buf, want);
    if (got <= 0) {
      continue;
    }

    if (ota_write(buf, got) < 0) {
      Serial.println("Update write failed");
      c.stop();
      return -1;
    }
    received += got;
    last = millis();
  }

  c.stop();
  if (ota_write_end() < 0) {
    return -1;
  }
  return received;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("OTA update over WiFi");

  Serial.print("Connecting to ");
  Serial.println(ssid);
  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    Serial.println("Connection failed, retrying...");
    delay(5000);
  }

  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // Online again: keep this sketch if it was installed as a test.
  ota_confirm();

  Serial.print("Downloading http://");
  Serial.print(server);
  Serial.print(":");
  Serial.print(port);
  Serial.println(path);

  long len = httpGet(client, server, port, path, UPDATE_TYPE);

  if (len < 0) {
    Serial.println("Download failed");
    return;
  }

  Serial.print("Downloaded ");
  Serial.print(len);
  Serial.println(" bytes");

  if (ota_sketch_ready() < 0) {
    Serial.println("Failed to mark update as ready");
    return;
  }

  Serial.println("Rebooting...");
  Serial.flush();
  delay(100);

  ota_sketch_start();
}

void loop() {
  delay(1000);
}
