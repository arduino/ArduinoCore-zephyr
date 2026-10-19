/*
 * Manual Bits and Bytes API test for KIT-PSE84-AI (PSOC Edge E84)
 *
 * This hardware-free test checks bit masks and byte extraction using known
 * values, then prints PASS/FAIL results to Serial every three seconds.
 * Verifies: bit, bitClear, bitRead, bitSet, bitWrite, highByte, lowByte.
 */

static int tests = 0;
static int failures = 0;

// Count and print the result of one assertion.
static void check(bool cond, const char *name) {
    tests++;
    if (!cond) failures++;
    Serial.print(cond ? "PASS " : "FAIL ");
    Serial.println(name);
    Serial.flush();
}

// Run each bit/byte operation against a known value, then print a summary.
static void run_bits_tests() {
    tests = 0;
    failures = 0;
    Serial.println("\n=== Bits and Bytes API manual test ===");

    // Build masks with one bit set at positions 0 and 3.
    check(bit(0) == 1 && bit(3) == 8, "bit");

    uint8_t v1 = 0xFF;
    // Clear bit 0 while leaving the other bits set.
    bitClear(v1, 0);
    check(v1 == 0xFE, "bitClear");

    // Read a set bit and an unset bit from the same value.
    check(bitRead(0x04, 2) == 1 && bitRead(0x04, 1) == 0, "bitRead");

    uint8_t v2 = 0x00;
    // Set bit 4 in an initially clear byte.
    bitSet(v2, 4);
    check(v2 == 0x10, "bitSet");

    uint8_t v3 = 0x00;
    // Write bits 1 and 5, clearing bit 1 again to verify both write values.
    bitWrite(v3, 1, 1);
    bitWrite(v3, 1, 0);
    bitWrite(v3, 5, 1);
    check(v3 == 0x20, "bitWrite");

    // Extract the upper and lower bytes from a known 16-bit word.
    check(highByte(0xABCD) == 0xAB, "highByte");
    check(lowByte(0xABCD) == 0xCD, "lowByte");

    Serial.print(tests);
    Serial.print(" Tests ");
    Serial.print(failures);
    Serial.println(failures == 0 ? " Failures - OK" : " Failures - FAIL");
    Serial.println("=== Bits and Bytes API manual test done ===");
    Serial.flush();
}

// Start the serial console used to display test results.
void setup() {
    Serial.begin(115200);
    delay(500);
}

// Repeat the checks every three seconds.
void loop() {
    run_bits_tests();
    delay(3000);
}
