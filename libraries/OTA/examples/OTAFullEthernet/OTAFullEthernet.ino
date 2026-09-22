/*
 * Full OTA update: loader + sketch over Ethernet.
 *
 * Downloads a "-bundle.bin" file that the build system already produces
 * (loader + 0xFF padding + sketch, without the OTA header).  The mbed
 * bootloader writes the payload to internal flash, updating both the
 * loader and the sketch in one shot.
 *
 * Serve the file with any HTTP server, e.g.:
 *
 *   cp Blink.ino.elf-zsk.bin-bundle.bin UPDATE.bin
 *   python3 -m http.server 8000
 *
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ZephyrClient.h>
#include <ZephyrEthernet.h>
#include <zephyr/fs/fs.h>
#include <zephyr/autoconf.h>

#ifndef CONFIG_OTA
#error "This variant's loader was built without CONFIG_OTA."
#endif

#define STAGING_PATH CONFIG_OTA_LOADER_UPDATE_PATH CONFIG_OTA_LOADER_TEMP_PATH_POSTFIX

extern "C" {
int ota_loader_ready(void);
int ota_loader_start(void);
}

const char *server = "10.10.10.10";
const uint16_t port = 8000;
const char *path = "/UPDATE.bin";

ZephyrClient client;

static long httpGet(Client &c, const char *host, uint16_t port,
                    const char *path, struct fs_file_t *out) {
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

    if (fs_write(out, buf, got) != got) {
      Serial.println("Filesystem write failed");
      c.stop();
      return -1;
    }
    received += got;
    last = millis();
  }

  c.stop();
  return received == content_length ? received : -1;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("Full OTA update (loader + sketch) over Ethernet");

  while (Ethernet.linkStatus() != LinkON) {
    Serial.println("Waiting for link...");
    delay(100);
  }

  if (Ethernet.begin() == 0) {
    Serial.println("DHCP failed");
    return;
  }

  Serial.print("IP: ");
  Serial.println(Ethernet.localIP());

  fs_unlink(STAGING_PATH);

  struct fs_file_t file;
  fs_file_t_init(&file);
  if (fs_open(&file, STAGING_PATH, FS_O_CREATE | FS_O_WRITE) < 0) {
    Serial.println("Cannot open staging file");
    return;
  }

  Serial.print("Downloading http://");
  Serial.print(server);
  Serial.print(":");
  Serial.print(port);
  Serial.println(path);

  long len = httpGet(client, server, port, path, &file);
  fs_close(&file);

  if (len < 0) {
    Serial.println("Download failed");
    fs_unlink(STAGING_PATH);
    return;
  }

  Serial.print("Downloaded ");
  Serial.print(len);
  Serial.println(" bytes");

  if (ota_loader_ready() < 0) {
    Serial.println("Failed to mark update as ready");
    return;
  }

  Serial.println("Rebooting...");
  Serial.flush();
  delay(100);

  ota_loader_start();
}

void loop() {
  delay(1000);
}
