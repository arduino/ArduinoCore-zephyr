/*
 * Manual Characters API test for KIT-PSE84-AI (PSOC Edge E84)
 *
 * Verifies: isAlpha, isAlphaNumeric, isAscii, isControl, isDigit, isGraph,
 *           isHexadecimalDigit, isLowerCase, isPrintable, isPunct, isSpace,
 *           isUpperCase, isWhitespace
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

static void run_char_tests() {
    tests = 0;
    failures = 0;
    Serial.println("\n=== Characters API manual test ===");

    check(isAlpha('A') && !isAlpha('1'), "isAlpha");
    check(isAlphaNumeric('A') && isAlphaNumeric('9') && !isAlphaNumeric('#'), "isAlphaNumeric");
    check(isAscii('A') && !isAscii((char)200), "isAscii");
    check(isControl('\n') && !isControl('A'), "isControl");
    check(isDigit('7') && !isDigit('A'), "isDigit");
    check(isGraph('A') && !isGraph(' '), "isGraph");
    check(isHexadecimalDigit('F') && isHexadecimalDigit('9') && !isHexadecimalDigit('G'), "isHexadecimalDigit");
    check(isLowerCase('a') && !isLowerCase('A'), "isLowerCase");
    check(isPrintable('A') && !isPrintable('\n'), "isPrintable");
    check(isPunct('!') && !isPunct('A'), "isPunct");
    check(isSpace(' ') && isSpace('\t') && !isSpace('A'), "isSpace");
    check(isUpperCase('A') && !isUpperCase('a'), "isUpperCase");
    check(isWhitespace(' ') && isWhitespace('\t') && !isWhitespace('A'), "isWhitespace");

    Serial.print(tests);
    Serial.print(" Tests ");
    Serial.print(failures);
    Serial.println(failures == 0 ? " Failures - OK" : " Failures - FAIL");
    Serial.println("=== Characters API manual test done ===");
    Serial.flush();
}

void setup() {
    Serial.begin(115200);
    delay(500);
}

void loop() {
    run_char_tests();
    delay(3000);
}
