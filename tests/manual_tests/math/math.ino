/*
 * Manual Math + Trigonometry API test for KIT-PSE84-AI (PSOC Edge E84)
 *
 * Verifies: abs, constrain, map, max, min, pow, sq, sqrt, cos, sin, tan
 * Results print on Serial as PASS/FAIL lines, repeating in loop().
 */

#include <math.h>

static int tests = 0;
static int failures = 0;

static void check(bool cond, const char *name) {
    tests++;
    if (!cond) failures++;
    Serial.print(cond ? "PASS " : "FAIL ");
    Serial.println(name);
    Serial.flush();
}

static bool near(double a, double b, double tol) {
    return fabs(a - b) <= tol;
}

static void run_math_tests() {
    tests = 0;
    failures = 0;
    Serial.println("\n=== Math API manual test ===");

    // Math
    check(abs(-5) == 5, "abs");
    check(constrain(15, 0, 10) == 10 &&
          constrain(-3, 0, 10) == 0 &&
          constrain(5, 0, 10) == 5, "constrain");
    check(map(512, 0, 1023, 0, 255) == 127, "map");
    check(max(3, 7) == 7, "max");
    check(min(3, 7) == 3, "min");
    check(near(pow(2, 10), 1024.0, 0.001), "pow");
    check(sq(5) == 25, "sq");
    check(near(sqrt(144.0), 12.0, 0.001), "sqrt");

    // Trigonometry (radians)
    check(near(cos(0.0), 1.0, 0.0001) &&
          near(cos(PI), -1.0, 0.0001), "cos");
    check(near(sin(0.0), 0.0, 0.0001) &&
          near(sin(PI / 2.0), 1.0, 0.0001), "sin");
    check(near(tan(0.0), 0.0, 0.0001) &&
          near(tan(PI / 4.0), 1.0, 0.0001), "tan");

    Serial.print(tests);
    Serial.print(" Tests ");
    Serial.print(failures);
    Serial.println(failures == 0 ? " Failures - OK" : " Failures - FAIL");
    Serial.println("=== Math API manual test done ===");
    Serial.flush();
}

void setup() {
    Serial.begin(115200);
    delay(500);
}

void loop() {
    run_math_tests();
    delay(3000);
}
