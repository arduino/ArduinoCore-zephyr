/*
 * Manual Advanced I/O API test for KIT-PSE84-AI (PSOC Edge E84)
 *
 * Connect D16 to D17 with a jumper. Do not use Serial1 during this test.
 *
 * tone() drives D16; the jumper lets D17 check its output level and lets
 * pulseIn() measure its HIGH pulse. shiftIn() clocks D21 while D20's internal
 * pull-up and pull-down provide known HIGH and LOW input values. Results are
 * printed to Serial. pulseInLong() is reported unavailable because the core
 * has no implementation.
 */

static const uint8_t TONE_OUTPUT_PIN = D16;
static const uint8_t TONE_INPUT_PIN = D17;
static const uint8_t SHIFT_IN_DATA_PIN = D20;
static const uint8_t SHIFT_IN_CLOCK_PIN = D21;

static int tests = 0;
static int failures = 0;
static int unavailable = 0;

// Count and print one assertion result.
static void check(bool condition, const __FlashStringHelper *name) {
    tests++;
    if (!condition) failures++;
    Serial.print(condition ? F("PASS ") : F("FAIL "));
    Serial.println(name);
    Serial.flush();
}

// Verify that a finite tone starts HIGH and returns LOW after its duration.
static void test_tone() {
    tone(TONE_OUTPUT_PIN, 100, 300);
    delay(1);
    bool startedHigh = digitalRead(TONE_INPUT_PIN) == HIGH;
    delay(350);
    bool stoppedLow = digitalRead(TONE_INPUT_PIN) == LOW;
    check(startedHigh, F("tone_starts_high"));
    check(stoppedLow, F("tone_stops_after_duration"));
}

// Verify that noTone() immediately stops an active continuous tone.
static void test_no_tone() {
    tone(TONE_OUTPUT_PIN, 100);
    delay(1);
    bool startedHigh = digitalRead(TONE_INPUT_PIN) == HIGH;
    noTone(TONE_OUTPUT_PIN);
    delay(2);
    check(startedHigh && digitalRead(TONE_INPUT_PIN) == LOW,
          F("noTone_stops_output"));
}

// Generate a 50 Hz tone and return one HIGH pulse width measured by pulseIn().
static unsigned long measure_tone_pulse() {
    tone(TONE_OUTPUT_PIN, 50);
    unsigned long pulseWidth = pulseIn(TONE_INPUT_PIN, HIGH, 100);
    noTone(TONE_OUTPUT_PIN);
    return pulseWidth;
}

// Check that pulseIn() measures the tone's approximately 10 ms HIGH phase.
static void test_pulse_in() {
    unsigned long pulseWidth = measure_tone_pulse();
    check(pulseWidth >= 5000 && pulseWidth <= 15000,
          F("pulseIn_measures_tone_high_pulse"));
    Serial.print(F("pulseIn measured us: "));
    Serial.println(pulseWidth);
}

// Report pulseInLong() as unavailable; no implementation exists in this core.
static void test_pulse_in_long() {
    unavailable++;
    Serial.println(F("UNAVAILABLE pulseInLong_missing_core_implementation"));
}

// Read eight bits with shiftIn() while D20 is pulled HIGH, then LOW.
static void test_shift_in() {
    pinMode(SHIFT_IN_CLOCK_PIN, OUTPUT);
    digitalWrite(SHIFT_IN_CLOCK_PIN, LOW);

    pinMode(SHIFT_IN_DATA_PIN, INPUT_PULLUP);
    uint8_t pulledHigh = shiftIn(SHIFT_IN_DATA_PIN, SHIFT_IN_CLOCK_PIN, MSBFIRST);
    check(pulledHigh == 0xFF, F("shiftIn_reads_high_bits"));

    pinMode(SHIFT_IN_DATA_PIN, INPUT_PULLDOWN);
    uint8_t pulledLow = shiftIn(SHIFT_IN_DATA_PIN, SHIFT_IN_CLOCK_PIN, LSBFIRST);
    check(pulledLow == 0x00, F("shiftIn_reads_low_bits"));
}

// Run the Advanced I/O checks and print their pass, failure, and unavailable counts.
static void run_advanced_io_tests() {
    tests = 0;
    failures = 0;
    unavailable = 0;
    Serial.println(F("\n=== Advanced I/O API manual test ==="));
    Serial.flush();

    test_tone();
    test_no_tone();
    test_pulse_in();
    test_pulse_in_long();
    test_shift_in();

    Serial.print(tests);
    Serial.print(F(" Tests "));
    Serial.print(failures);
    Serial.print(F(" Failures "));
    Serial.print(unavailable);
    Serial.println(F(" Unavailable"));
    Serial.println(F("=== Advanced I/O API manual test done ==="));
    Serial.flush();
}

// Start Serial and configure the tone loopback pins before testing.
void setup() {
    Serial.begin(115200);
    pinMode(TONE_INPUT_PIN, INPUT);
    pinMode(TONE_OUTPUT_PIN, OUTPUT);
    digitalWrite(TONE_OUTPUT_PIN, LOW);
    delay(500);
}

// Repeat the test suite every three seconds.
void loop() {
    run_advanced_io_tests();
    delay(3000);
}