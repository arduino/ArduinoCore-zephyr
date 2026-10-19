/*
 * Manual Math + Trigonometry API test for KIT-PSE84-AI (PSOC Edge E84)
 *
 * Verifies: abs, constrain, map, max, min, pow, sq, sqrt, cos, sin, tan
 * Checks known results, using a tolerance for floating-point functions.
 * No external hardware is required; results print on Serial every three seconds.
 */

#include <math.h>

static int tests = 0;
static int failures = 0;

// Count and print the result of one math assertion.
static void check(bool cond, const char *name) {
    tests++;
    if (!cond) failures++;
    Serial.print(cond ? "PASS " : "FAIL ");
    Serial.println(name);
    Serial.flush();
}

// Compare floating-point values within an absolute error tolerance.
static bool near(double a, double b, double tol) {
    return fabs(a - b) <= tol;
}

// Run the integer, floating-point, and trigonometric checks.
static void run_math_tests() {
    tests = 0;
    failures = 0;
    Serial.println("\n=== Math API manual test ===");

        // Return the magnitude of a negative number.
    check(abs(-5) == 5, "abs");
        // Clamp values below, above, and inside the requested range.
    check(constrain(15, 0, 10) == 10 &&
          constrain(-3, 0, 10) == 0 &&
          constrain(5, 0, 10) == 5, "constrain");
        // Scale a value from one integer range into another.
    check(map(512, 0, 1023, 0, 255) == 127, "map");
        // Select the larger of two values.
    check(max(3, 7) == 7, "max");
        // Select the smaller of two values.
    check(min(3, 7) == 3, "min");
        // Raise a base to an exponent.
    check(near(pow(2, 10), 1024.0, 0.001), "pow");
        // Multiply a value by itself.
    check(sq(5) == 25, "sq");
        // Calculate the non-negative square root.
    check(near(sqrt(144.0), 12.0, 0.001), "sqrt");

    // Verify trigonometric functions using angles in radians.
        // Compare cosine at zero and pi radians.
    check(near(cos(0.0), 1.0, 0.0001) &&
          near(cos(PI), -1.0, 0.0001), "cos");
        // Compare sine at zero and pi/2 radians.
    check(near(sin(0.0), 0.0, 0.0001) &&
          near(sin(PI / 2.0), 1.0, 0.0001), "sin");
        // Compare tangent at zero and pi/4 radians.
    check(near(tan(0.0), 0.0, 0.0001) &&
          near(tan(PI / 4.0), 1.0, 0.0001), "tan");

    Serial.print(tests);
    Serial.print(" Tests ");
    Serial.print(failures);
    Serial.println(failures == 0 ? " Failures - OK" : " Failures - FAIL");
    Serial.println("=== Math API manual test done ===");
    Serial.flush();
}

// Start the serial console for test results.
void setup() {
    Serial.begin(115200);
    delay(500);
}

// Repeat all math checks every three seconds.
void loop() {
    run_math_tests();
    delay(3000);
}
