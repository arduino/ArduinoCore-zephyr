/*
 * Manual Bits and Bytes API test for KIT-PSE84-AI (PSOC Edge E84)
 *
 * Verifies: bit, bitClear, bitRead, bitSet, bitWrite, highByte, lowByte
 * Results print on Serial as PASS/FAIL lines, repeating in loop().
 */

static int tests = 0;
static int failures = 0;

static void check(bool cond, const char *name) {
    tests++;
    if (!cond) failures++;
    Serial.print(cond ? "PASS " : "FAIL ");
    Serial.println(name);
    Serial.flush();
}

static void run_bits_tests() {
    tests = 0;
    failures = 0;
    Serial.println("\n=== Bits and Bytes API manual test ===");

    check(bit(0) == 1 && bit(3) == 8, "bit");

    uint8_t v1 = 0xFF;
    bitClear(v1, 0);
    check(v1 == 0xFE, "bitClear");

    check(bitRead(0x04, 2) == 1 && bitRead(0x04, 1) == 0, "bitRead");

    uint8_t v2 = 0x00;
    bitSet(v2, 4);
    check(v2 == 0x10, "bitSet");

    uint8_t v3 = 0x00;
    bitWrite(v3, 1, 1);
    bitWrite(v3, 1, 0);
    bitWrite(v3, 5, 1);
    check(v3 == 0x20, "bitWrite");

    check(highByte(0xABCD) == 0xAB, "highByte");
    check(lowByte(0xABCD) == 0xCD, "lowByte");

    Serial.print(tests);
    Serial.print(" Tests ");
    Serial.print(failures);
    Serial.println(failures == 0 ? " Failures - OK" : " Failures - FAIL");
    Serial.println("=== Bits and Bytes API manual test done ===");
    Serial.flush();
}

void setup() {
    Serial.begin(115200);
    delay(500);
}

void loop() {
    run_bits_tests();
    delay(3000);
}
