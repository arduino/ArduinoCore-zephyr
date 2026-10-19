/*
 * Manual Digital I/O API test for KIT-PSE84-AI (PSOC Edge E84)
 *
 * Connect D16 to D17 with a jumper. Verifies pinMode(), digitalWrite(),
 * and digitalRead() by reading back LOW and HIGH output levels.
 * Results are reported over Serial; no other hardware is needed.
 */

static int tests = 0;
static int failures = 0;

static const uint8_t TEST_OUTPUT_PIN = D16;
static const uint8_t TEST_INPUT_PIN = D17;

// Count and print the result of one pin readback assertion.
static void check(bool condition, const char *name) {
    tests++;
    if (!condition) failures++;
    Serial.print(condition ? "PASS " : "FAIL ");
    Serial.println(name);
    Serial.flush();
}

// Configure the loopback pins, test both levels, and summarize the results.
static void run_digital_io_tests() {
    tests = 0;
    failures = 0;
    Serial.println("\n=== Digital I/O API manual test ===");

    // Set one pin to drive the signal and the jumper-connected pin to read it.
    pinMode(TEST_OUTPUT_PIN, OUTPUT);
    pinMode(TEST_INPUT_PIN, INPUT);

    // Drive LOW, then verify the connected input reads LOW.
    digitalWrite(TEST_OUTPUT_PIN, LOW);
    delay(5);
    check(digitalRead(TEST_INPUT_PIN) == LOW, "digitalWrite_LOW_readback");

    // Drive HIGH, then verify the connected input reads HIGH.
    digitalWrite(TEST_OUTPUT_PIN, HIGH);
    delay(5);
    check(digitalRead(TEST_INPUT_PIN) == HIGH, "digitalWrite_HIGH_readback");

    digitalWrite(TEST_OUTPUT_PIN, LOW);
    Serial.print(tests);
    Serial.print(" Tests ");
    Serial.print(failures);
    Serial.println(failures == 0 ? " Failures - OK" : " Failures - FAIL");
    Serial.println("=== Digital I/O API manual test done ===");
    Serial.flush();
}

// Start Serial; pin modes are configured for each test run.
void setup() {
    Serial.begin(115200);
    delay(500);
}

// Repeat the loopback checks every three seconds.
void loop() {
    run_digital_io_tests();
    delay(3000);
}