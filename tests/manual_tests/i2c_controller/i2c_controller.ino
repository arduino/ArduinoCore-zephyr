/*
 * Manual I2C controller test for KIT-PSE84-AI (PSOC Edge E84)
 *
 * Two-board loopback: run this sketch on the CONTROLLER board and
 * i2c_target on the TARGET board, then cross-connect their Wire buses.
 *
 * Wiring (both boards use Wire = SCB0 on the P8 header, 1.8V domain):
 *     controller P8_0 (SCL) <-> target P8_0 (SCL)
 *     controller P8_1 (SDA) <-> target P8_1 (SDA)
 *     controller GND        <-> target GND
 * Add external pull-ups (2.2k-4.7k) from SDA and SCL to 1.8V; the pins are
 * open-drain and the board does not enable internal pull-ups.
 *
 * Each cycle the controller writes a one-byte command, then requests two
 * bytes back. The target replies with {command ^ 0xFF, 0x5A}. This exercises
 * begin(), setClock(), beginTransmission(), write(), endTransmission(),
 * requestFrom(), available(), and read(). Results print on Serial.
 */

#include <Wire.h>

static const uint8_t TARGET_ADDR = 0x30;
static const uint8_t SIGNATURE = 0x5A;

static int tests = 0;
static int failures = 0;
static uint8_t command = 0;

// Count and print the result of one I2C assertion.
static void check(bool cond, const char *name) {
    tests++;
    if (!cond) failures++;
    Serial.print(cond ? "PASS " : "FAIL ");
    Serial.println(name);
    Serial.flush();
}

// Send one command byte and confirm the target ACKs the transfer.
static bool send_command(uint8_t value) {
    Wire.beginTransmission(TARGET_ADDR);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

static void run_i2c_controller_tests() {
    tests = 0;
    failures = 0;
    Serial.println("\n=== I2C controller manual test ===");

    // Write a command; endTransmission() == 0 means the target ACKed.
    bool acked = send_command(command);
    check(acked, "endTransmission_acked");

    // Request the two-byte reply and confirm both bytes arrived.
    size_t got = Wire.requestFrom(TARGET_ADDR, (size_t)2);
    check(got == 2, "requestFrom_returned_2");
    check(Wire.available() == 2, "available_reports_2");

    // Verify the reply matches the agreed {command ^ 0xFF, 0x5A} protocol.
    int b0 = Wire.read();
    int b1 = Wire.read();
    check(b0 == (uint8_t)(command ^ 0xFF), "reply_byte0_matches");
    check(b1 == SIGNATURE, "reply_byte1_signature");

    Serial.print("sent cmd 0x");
    Serial.print(command, HEX);
    Serial.print(" got 0x");
    Serial.print(b0, HEX);
    Serial.print(" 0x");
    Serial.println(b1, HEX);

    Serial.print(tests);
    Serial.print(" Tests ");
    Serial.print(failures);
    Serial.println(failures == 0 ? " Failures - OK" : " Failures - FAIL");
    Serial.println("=== I2C controller manual test done ===");
    Serial.flush();

    command++;
}

// Start Serial and the I2C controller at 100 kHz.
void setup() {
    Serial.begin(115200);
    Wire.begin();
    Wire.setClock(100000);
    delay(500);
}

// Repeat the controller exchange every three seconds.
void loop() {
    run_i2c_controller_tests();
    delay(3000);
}
