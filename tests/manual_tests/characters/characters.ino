/*
 * Manual Characters API test for KIT-PSE84-AI (PSOC Edge E84)
 *
 * Verifies: isAlpha, isAlphaNumeric, isAscii, isControl, isDigit, isGraph,
 *           isHexadecimalDigit, isLowerCase, isPrintable, isPunct, isSpace,
 *           isUpperCase, isWhitespace
 * Each predicate receives representative accepted and rejected characters;
 * no external hardware is required. Results print on Serial every three seconds.
 */

static int tests = 0;
static int failures = 0;

// Count and print the result of one character-predicate assertion.
static void check(bool cond, const char *name) {
    tests++;
    if (!cond) failures++;
    Serial.print(cond ? "PASS " : "FAIL ");
    Serial.println(name);
    Serial.flush();
}

// Test each character predicate with inputs that should pass and fail.
static void run_char_tests() {
    tests = 0;
    failures = 0;
    Serial.println("\n=== Characters API manual test ===");

    // Letters pass; a digit does not.
    check(isAlpha('A') && !isAlpha('1'), "isAlpha");
    // Letters and digits pass; punctuation does not.
    check(isAlphaNumeric('A') && isAlphaNumeric('9') && !isAlphaNumeric('#'), "isAlphaNumeric");
    // ASCII-range characters pass; a value above 127 does not.
    check(isAscii('A') && !isAscii((char)200), "isAscii");
    // Newline is a control character; a letter is not.
    check(isControl('\n') && !isControl('A'), "isControl");
    // Digits pass; letters do not.
    check(isDigit('7') && !isDigit('A'), "isDigit");
    // Visible characters pass; whitespace does not.
    check(isGraph('A') && !isGraph(' '), "isGraph");
    // Hex digits pass; a non-hex letter does not.
    check(isHexadecimalDigit('F') && isHexadecimalDigit('9') && !isHexadecimalDigit('G'), "isHexadecimalDigit");
    // Lowercase letters pass; uppercase letters do not.
    check(isLowerCase('a') && !isLowerCase('A'), "isLowerCase");
    // Printable characters pass; newline does not.
    check(isPrintable('A') && !isPrintable('\n'), "isPrintable");
    // Punctuation passes; an alphabetic character does not.
    check(isPunct('!') && !isPunct('A'), "isPunct");
    // Space and tab pass; a letter does not.
    check(isSpace(' ') && isSpace('\t') && !isSpace('A'), "isSpace");
    // Uppercase letters pass; lowercase letters do not.
    check(isUpperCase('A') && !isUpperCase('a'), "isUpperCase");
    // Whitespace characters pass; a letter does not.
    check(isWhitespace(' ') && isWhitespace('\t') && !isWhitespace('A'), "isWhitespace");

    Serial.print(tests);
    Serial.print(" Tests ");
    Serial.print(failures);
    Serial.println(failures == 0 ? " Failures - OK" : " Failures - FAIL");
    Serial.println("=== Characters API manual test done ===");
    Serial.flush();
}

// Start the serial console for test results.
void setup() {
    Serial.begin(115200);
    delay(500);
}

// Repeat all character-predicate checks every three seconds.
void loop() {
    run_char_tests();
    delay(3000);
}
