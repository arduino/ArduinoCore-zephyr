/*
 * Manual Time API test for KIT-PSE84-AI (PSOC Edge E84)
 *
 * Measures elapsed time around delays and compares it with expected ranges.
 * Verifies millis(), micros(), delay(), and delayMicroseconds(); no external
 * hardware is required. Results print on Serial every three seconds.
 */

static int tests = 0;
static int failures = 0;

// Count and print the result of one timing assertion.
static void check(bool cond, const char *name) {
    tests++;
    if (!cond) failures++;
    Serial.print(cond ? "PASS " : "FAIL ");
    Serial.println(name);
    Serial.flush();
}

// millis() must advance by ~delay amount
static void test_millis() {
    unsigned long start = millis();
    delay(100);
    unsigned long elapsed = millis() - start;
    check(elapsed >= 95 && elapsed <= 115, "millis_advances_100ms");
}

// micros() must advance and be monotonic
static void test_micros() {
    unsigned long t0 = micros();
    delayMicroseconds(2000);
    unsigned long dt = micros() - t0;
    check(dt >= 1700 && dt <= 2600, "micros_advances_2000us");
}

// delay(ms) accuracy measured via micros()
static void test_delay() {
    unsigned long t0 = micros();
    delay(50);
    unsigned long dt = micros() - t0;
    check(dt >= 48000 && dt <= 56000, "delay_50ms_accurate");
}

// delayMicroseconds(us) accuracy measured via micros()
static void test_delay_micros() {
    unsigned long t0 = micros();
    delayMicroseconds(1000);
    unsigned long dt = micros() - t0;
    check(dt >= 850 && dt <= 1400, "delayMicroseconds_1000us_accurate");
}

void setup() {
    Serial.begin(115200);
    delay(500);
}

// Run all timing checks and print their combined result.
void loop() {
    tests = 0;
    failures = 0;
    Serial.println("\n=== Time API manual test ===");

    test_millis();
    test_micros();
    test_delay();
    test_delay_micros();

    Serial.print(tests);
    Serial.print(" Tests ");
    Serial.print(failures);
    Serial.println(failures == 0 ? " Failures - OK" : " Failures - FAIL");
    Serial.println("=== Time API manual test done ===");
    Serial.flush();
    delay(3000);
}
