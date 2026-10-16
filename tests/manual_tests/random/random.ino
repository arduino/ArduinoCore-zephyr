/*
 * Manual Random Numbers API test for KIT-PSE84-AI (PSOC Edge E84)
 *
 * Verifies: random(), randomSeed()
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

static void run_random_tests() {
    tests = 0;
    failures = 0;
    Serial.println("\n=== Random Numbers API manual test ===");

    // random(max): result in [0, max)
    bool in_range = true;
    for (int i = 0; i < 100; i++) {
        long r = random(10);
        if (r < 0 || r >= 10) in_range = false;
    }
    check(in_range, "random_max_range");

    // random(min, max): result in [min, max)
    in_range = true;
    for (int i = 0; i < 100; i++) {
        long r = random(50, 100);
        if (r < 50 || r >= 100) in_range = false;
    }
    check(in_range, "random_min_max_range");

    // random() produces varied values (not stuck)
    long first = random(1000000);
    bool varied = false;
    for (int i = 0; i < 20; i++) {
        if (random(1000000) != first) { varied = true; break; }
    }
    check(varied, "random_varies");

    // randomSeed() makes the sequence reproducible
    randomSeed(42);
    long seqA[5];
    for (int i = 0; i < 5; i++) seqA[i] = random(100000);
    randomSeed(42);
    bool reproducible = true;
    for (int i = 0; i < 5; i++) {
        if (random(100000) != seqA[i]) reproducible = false;
    }
    check(reproducible, "randomSeed_reproducible");

    Serial.print(tests);
    Serial.print(" Tests ");
    Serial.print(failures);
    Serial.println(failures == 0 ? " Failures - OK" : " Failures - FAIL");
    Serial.println("=== Random Numbers API manual test done ===");
    Serial.flush();
}

void setup() {
    Serial.begin(115200);
    delay(500);
}

void loop() {
    run_random_tests();
    delay(3000);
}
