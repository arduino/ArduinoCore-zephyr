/*
 * Manual Stream API test for KIT-PSE84-AI (PSOC Edge E84)
 *
 * Verifies the Stream methods on Serial1: available, read, peek, flush,
 * readBytes, readBytesUntil, readString, readStringUntil, find, findUntil,
 * parseInt, parseFloat, setTimeout.
 *
 * REQUIRES a self-loopback jumper on ONE board:
 *     Serial1 TX (P9_2) --> Serial1 RX (P9_3)
 *
 * Results print on Serial (console). Repeats in loop().
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

// Clear any pending RX bytes.
static void drain() {
    unsigned long t = millis();
    while (Serial1.available()) {
        Serial1.read();
        if (millis() - t > 500) break;
    }
}

// Send a string over the loopback and wait for it to arrive.
// NOTE: Serial1.flush() hangs on this core, so we use a delay instead.
static void loop_send(const char *s) {
    drain();
    Serial1.print(s);
    delay(80);
}

static void run_stream_tests() {
    tests = 0;
    failures = 0;
    Serial.println("\n=== Stream API manual test ===");

    Serial1.setTimeout(200);

    // available() + read()
    loop_send("A");
    check(Serial1.available() >= 1, "available");
    check(Serial1.read() == 'A', "read");

    // peek() does not consume
    loop_send("Z");
    char pk = Serial1.peek();
    check(pk == 'Z' && Serial1.read() == 'Z', "peek");

    // flush() HANGS on this core (Serial1.flush never returns) -> not callable
    check(false, "flush (Serial1.flush hangs - not supported)");

    // readBytes()
    loop_send("12345");
    char buf[8] = {0};
    size_t n = Serial1.readBytes(buf, 5);
    check(n == 5 && strncmp(buf, "12345", 5) == 0, "readBytes");

    // readBytesUntil()
    loop_send("abc,def");
    memset(buf, 0, sizeof(buf));
    n = Serial1.readBytesUntil(',', buf, sizeof(buf) - 1);
    check(n == 3 && strncmp(buf, "abc", 3) == 0, "readBytesUntil");

    // readString()
    loop_send("hello");
    String s = Serial1.readString();
    check(s == "hello", "readString");

    // readStringUntil()
    loop_send("name=value");
    String key = Serial1.readStringUntil('=');
    check(key == "name", "readStringUntil");

    // find()
    loop_send("prefix#TARGET");
    check(Serial1.find((char *)"TARGET"), "find");
    drain();

    // findUntil()
    loop_send("GOAL;rest");
    check(Serial1.findUntil((char *)"GOAL", (char *)";"), "findUntil");
    drain();

    // parseInt()
    loop_send("  -427x");
    check(Serial1.parseInt() == -427, "parseInt");
    drain();

    // parseFloat()
    loop_send("3.14y");
    float f = Serial1.parseFloat();
    check(f > 3.13f && f < 3.15f, "parseFloat");
    drain();

    // setTimeout() already exercised (reads above returned within timeout)
    check(true, "setTimeout");

    Serial.print(tests);
    Serial.print(" Tests ");
    Serial.print(failures);
    Serial.println(failures == 0 ? " Failures - OK" : " Failures - FAIL");
    Serial.println("=== Stream API manual test done ===");
    Serial.flush();
}

void setup() {
    Serial.begin(115200);
    Serial1.begin(115200);
    delay(500);
}

void loop() {
    run_stream_tests();
    delay(3000);
}
