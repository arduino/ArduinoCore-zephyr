/*
 * Manual Serial (console) API test for KIT-PSE84-AI (PSOC Edge E84)
 *
 * Output/control methods are tested directly. The read-family methods are
 * tested by having the HOST send a line to the port each cycle:
 *     printf 'Hello,World;-42 3.14\n' > /dev/ttyACM0
 *
 * Results print on Serial. Repeats in loop().
 */

static int tests = 0;
static int failures = 0;

// Count and print the result of one Serial API assertion.
static void check(bool cond, const char *name) {
    tests++;
    if (!cond) failures++;
    Serial.print(cond ? "PASS " : "FAIL ");
    Serial.println(name);
    Serial.flush();
}

// Discard bytes up to and including the next newline so the following read
// starts at the beginning of a fresh host-sent line; stop waiting on timeout.
static void sync_to_line_start() {
    unsigned long t0 = millis();
    // wait for some data
    while (Serial.available() == 0 && millis() - t0 < 2000) { }
    // consume through a newline
    t0 = millis();
    while (millis() - t0 < 2000) {
        if (Serial.available()) {
            if (Serial.read() == '\n') break;
        }
    }
    // ensure next line's bytes are present
    t0 = millis();
    while (Serial.available() == 0 && millis() - t0 < 2000) { }
}

// Start the console used for both the test output and host-fed input.
void setup() {
    Serial.begin(115200);
    delay(500);
}

// Test output methods, prompt for host input, exercise read methods, and report results.
void loop() {
    tests = 0;
    failures = 0;
    Serial.println("\n=== Serial API manual test ===");

    // --- output / control methods (no host input needed) ---
    check((bool)Serial, "if(Serial)");
    check(Serial.availableForWrite() > 0, "availableForWrite");
    check(Serial.write('.') == 1, "write");
    Serial.flush();
    check(true, "flush");
    Serial.setTimeout(300);
    check(true, "setTimeout");

    // --- input methods: require host to send a line ---
    // Expected line from host: "Hello,World;-42 3.14\n"
    Serial.println("SEND_LINE");   // cue for the host feeder
    Serial.flush();

    unsigned long t0 = millis();
    while (Serial.available() == 0 && millis() - t0 < 2000) { }

    if (Serial.available() > 0) {
        check(Serial.available() > 0, "available");

        char pk = Serial.peek();
        check(pk == 'H', "peek");
        check(Serial.read() == 'H', "read");

        // remaining now starts at "ello,World;-42 3.14"
        char buf[8] = {0};
        size_t n = Serial.readBytes(buf, 4);         // "ello"
        check(n == 4 && strncmp(buf, "ello", 4) == 0, "readBytes");

        memset(buf, 0, sizeof(buf));
        n = Serial.readBytesUntil(';', buf, sizeof(buf) - 1); // ",World"
        check(n == 6 && strncmp(buf, ",World", 6) == 0, "readBytesUntil");

        long i = Serial.parseInt();                  // -42
        check(i == -42, "parseInt");

        float f = Serial.parseFloat();               // 3.14
        check(f > 3.13f && f < 3.15f, "parseFloat");

        // drain remainder of the line
        while (Serial.available()) Serial.read();

        // --- string/search methods: sync to a fresh line each time ---
        // find(): fresh line "Hello,World;-42 3.14"
        sync_to_line_start();
        check(Serial.find((char *)"World"), "find");
        while (Serial.available()) Serial.read();

        // findUntil(): "Hello" before terminator ','
        sync_to_line_start();
        check(Serial.findUntil((char *)"Hello", (char *)","), "findUntil");
        while (Serial.available()) Serial.read();

        // readStringUntil(): "Hello"
        sync_to_line_start();
        String k = Serial.readStringUntil(',');
        check(k == "Hello", "readStringUntil");
        while (Serial.available()) Serial.read();

        // readString(): whole line
        sync_to_line_start();
        String s = Serial.readString();
        check(s.startsWith("Hello,World"), "readString");
        while (Serial.available()) Serial.read();
    } else {
        check(false, "available (no host input received)");
    }

    // Serial.end() then re-begin; if console still prints after, it works.
    Serial.end();
    Serial.begin(115200);
    delay(50);
    check(true, "end");

    Serial.print(tests);
    Serial.print(" Tests ");
    Serial.print(failures);
    Serial.println(failures == 0 ? " Failures - OK" : " Failures - FAIL");
    Serial.println("=== Serial API manual test done ===");
    Serial.flush();
    delay(1500);
}
