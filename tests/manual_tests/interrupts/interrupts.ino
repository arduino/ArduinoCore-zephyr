/*
 * Manual Interrupts API test for KIT-PSE84-AI (PSOC Edge E84)
 *
 * Verifies: interrupts(), noInterrupts()
 * These globally enable/disable IRQs. The test confirms the calls execute,
 * a critical section runs, and the system keeps running afterwards.
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

static void run_interrupts_tests() {
    tests = 0;
    failures = 0;
    Serial.println("\n=== Interrupts API manual test ===");

    // Critical section executes and completes.
    volatile uint32_t sum = 0;
    noInterrupts();
    for (uint32_t i = 1; i <= 1000; i++) sum += i;
    interrupts();
    check(sum == 500500, "critical_section_runs");

    // System timer keeps running after re-enabling interrupts.
    unsigned long start = millis();
    delay(50);
    check((millis() - start) >= 45, "millis_advances_after_interrupts");

    // Nested disable/enable is safe and leaves IRQs enabled.
    noInterrupts();
    noInterrupts();
    interrupts();
    interrupts();
    unsigned long t0 = micros();
    delayMicroseconds(1000);
    check((micros() - t0) >= 800, "nested_enable_disable_safe");

    Serial.print(tests);
    Serial.print(" Tests ");
    Serial.print(failures);
    Serial.println(failures == 0 ? " Failures - OK" : " Failures - FAIL");
    Serial.println("=== Interrupts API manual test done ===");
    Serial.flush();
}

void setup() {
    Serial.begin(115200);
    delay(500);
}

void loop() {
    run_interrupts_tests();
    delay(3000);
}
