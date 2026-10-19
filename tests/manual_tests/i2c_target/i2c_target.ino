/*
 * Manual I2C target test for KIT-PSE84-AI (PSOC Edge E84)
 *
 * Two-board loopback: run this sketch on the TARGET board and
 * i2c_controller on the CONTROLLER board. See i2c_controller.ino for the
 * wiring and required pull-up resistors.
 *
 * The target registers address 0x30 and reacts to the controller:
 *   - onReceive() stores the last command byte.
 *   - onRequest() replies with {command ^ 0xFF, 0x5A}.
 * This exercises begin(address), onReceive(), onRequest(), and write().
 * Status prints on Serial; the controller board reports PASS/FAIL.
 */

#include <Wire.h>

static const uint8_t TARGET_ADDR = 0x30;
static const uint8_t SIGNATURE = 0x5A;

static volatile uint8_t lastCommand = 0;
static volatile uint32_t receiveCount = 0;
static volatile uint32_t requestCount = 0;

// Store the latest command byte(s) sent by the controller.
static void onReceiveHandler(int numBytes) {
    while (Wire.available() > 0) {
        lastCommand = (uint8_t)Wire.read();
    }
    receiveCount++;
}

// Reply to a read request with the complement of the command and a signature.
static void onRequestHandler() {
    Wire.write((uint8_t)(lastCommand ^ 0xFF));
    Wire.write(SIGNATURE);
    requestCount++;
}

// Start Serial and register this board as an I2C target at 0x30.
void setup() {
    Serial.begin(115200);
    Wire.begin(TARGET_ADDR);
    Wire.onReceive(onReceiveHandler);
    Wire.onRequest(onRequestHandler);
    delay(500);
    Serial.println("I2C target ready at 0x30");
}

// Periodically report the latest command and callback counts.
void loop() {
    Serial.print("last cmd 0x");
    Serial.print(lastCommand, HEX);
    Serial.print(" receives ");
    Serial.print(receiveCount);
    Serial.print(" requests ");
    Serial.println(requestCount);
    Serial.flush();
    delay(3000);
}
