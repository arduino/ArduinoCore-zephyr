/*
 * Manual init hooks test for KIT-PSE84-AI (PSOC Edge E84)
 *
 * Overrides initVariant() and defines a global init(); each sets a flag.
 * Because both hooks (if called) run before setup(), the flags reveal which
 * hooks this core actually invokes. No external hardware is required.
 */

#include <Arduino.h>

static volatile bool initVariantRan = false;
static volatile bool initRan = false;

// Weak initVariant() hook; this core's main() calls it before setup().
void initVariant() {
    initVariantRan = true;
}

// AVR-style global init() hook; set a flag if the core ever calls it.
void init() {
    initRan = true;
}

static int tests = 0;
static int failures = 0;

// Count and print the result of one hook assertion.
static void check(bool cond, const char *name) {
    tests++;
    if (!cond) failures++;
    Serial.print(cond ? "PASS " : "FAIL ");
    Serial.println(name);
    Serial.flush();
}

void setup() {
    Serial.begin(115200);
    delay(500);
}

void loop() {
    tests = 0;
    failures = 0;
    Serial.println("\n=== init hooks manual test ===");

    check(initVariantRan, "initVariant_ran_before_setup");
    check(initRan, "init_ran_before_setup");

    Serial.print(tests);
    Serial.print(" Tests ");
    Serial.print(failures);
    Serial.println(failures == 0 ? " Failures - OK" : " Failures - FAIL");
    Serial.println("=== init hooks manual test done ===");
    Serial.flush();
    delay(3000);
}
