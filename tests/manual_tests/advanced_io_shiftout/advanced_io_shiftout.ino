/*
 * Manual shiftOut() API test for KIT-PSE84-AI (PSOC Edge E84)
 *
 * shiftOut() sends 0x96 on D18 while clocking D19. A rising-edge interrupt
 * samples D18 eight times and reconstructs the byte for each bit order; the
 * test passes when the captured value is 0x96 for both MSBFIRST and LSBFIRST.
 * No external wiring is needed. Kept separate because combining this
 * interrupt capture with tone tests exceeds the sketch RAM limit with the
 * board's default tone-pool setting.
 */

static const uint8_t DATA_PIN = D18;
static const uint8_t CLOCK_PIN = D19;

static int tests = 0;
static int failures = 0;
static volatile uint8_t capturedValue = 0;
static volatile uint8_t capturedCount = 0;
static volatile bool captureLsbFirst = false;

// Record and print the result of one shiftOut assertion.
static void check(bool condition, const __FlashStringHelper *name) {
    tests++;
    if (!condition) failures++;
    Serial.print(condition ? F("PASS ") : F("FAIL "));
    Serial.println(name);
    Serial.flush();
}

// Sample the data pin on each rising clock edge and reconstruct the byte.
static void onClockRise() {
    if (capturedCount >= 8) return;

    uint8_t bitValue = digitalRead(DATA_PIN) == HIGH;
    if (captureLsbFirst) {
        capturedValue |= bitValue << capturedCount;
    } else {
        capturedValue = (capturedValue << 1) | bitValue;
    }
    capturedCount++;
}

// Capture one shiftOut() transfer and compare the received bits with 0x96.
static void test_shift_out_order(BitOrder bitOrder, bool lsbFirst,
                                 const __FlashStringHelper *name) {
    capturedValue = 0;
    capturedCount = 0;
    captureLsbFirst = lsbFirst;

    attachInterrupt(digitalPinToInterrupt(CLOCK_PIN), onClockRise, RISING);
    shiftOut(DATA_PIN, CLOCK_PIN, bitOrder, 0x96);
    detachInterrupt(digitalPinToInterrupt(CLOCK_PIN));

    check(capturedCount == 8 && capturedValue == 0x96, name);
}

// Start Serial and configure the data and clock GPIOs.
void setup() {
    Serial.begin(115200);
    pinMode(DATA_PIN, OUTPUT);
    pinMode(CLOCK_PIN, OUTPUT);
    digitalWrite(CLOCK_PIN, LOW);
    delay(500);
}

// Run both bit-order checks and print a summary every three seconds.
void loop() {
    tests = 0;
    failures = 0;
    Serial.println(F("\n=== shiftOut API manual test ==="));

    test_shift_out_order(MSBFIRST, false, F("shiftOut_MSB_first"));
    test_shift_out_order(LSBFIRST, true, F("shiftOut_LSB_first"));

    Serial.print(tests);
    Serial.print(F(" Tests "));
    Serial.print(failures);
    Serial.println(failures == 0 ? F(" Failures - OK") : F(" Failures - FAIL"));
    Serial.println(F("=== shiftOut API manual test done ==="));
    Serial.flush();
    delay(3000);
}