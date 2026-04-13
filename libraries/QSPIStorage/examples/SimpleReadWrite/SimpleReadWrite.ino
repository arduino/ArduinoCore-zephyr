/*
  QSPIStorage - Simple Read/Write Example

  The simplest example showing how to write, read, and seek in a file.

  This example code is in the public domain.
*/

#include <QSPIStorage.h>

QSPIStorage storage;

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        delay(10);
    }

    Serial.println("QSPIStorage - Simple Read/Write Example\n");

    // Initialize storage
    if (!storage.begin()) {
        Serial.println("Failed to initialize storage!");
        while (1) delay(1000);
    }
    Serial.println("Storage ready.\n");

    // ======================
    // WRITE to a file
    // ======================
    Serial.println("Writing to file...");

    QSPIFile file("/storage/test.txt");
    file.open(FileMode::WRITE);
    file.write("Line 1: Hello\n");
    file.write("Line 2: World\n");
    file.write("Line 3: Done!\n");
    file.close();

    Serial.println("Write complete.\n");

    // ======================
    // READ entire file
    // ======================
    Serial.println("Reading entire file:");

    file.open(FileMode::READ);
    String content = file.readAsString();
    file.close();

    Serial.println("---");
    Serial.print(content);
    Serial.println("---\n");

    // ======================
    // READ byte by byte
    // ======================
    Serial.println("Reading byte by byte (first 20 chars):");

    file.open(FileMode::READ);
    for (int i = 0; i < 20 && file.available() > 0; i++) {
        int c = file.read();
        if (c >= 0) {
            Serial.print((char)c);
        }
    }
    Serial.println("...\n");

    // ======================
    // SEEK and read
    // ======================
    Serial.println("Seeking to position 14 (start of Line 2):");

    file.seek(14);  // Skip "Line 1: Hello\n"
    String line2 = "";
    int c;
    while ((c = file.read()) >= 0 && c != '\n') {
        line2 += (char)c;
    }
    file.close();

    Serial.print("Read: ");
    Serial.println(line2);

    // ======================
    // READ into buffer
    // ======================
    Serial.println("\nReading into buffer:");

    file.open(FileMode::READ);
    uint8_t buffer[32];
    size_t bytesRead = file.read(buffer, sizeof(buffer) - 1);
    buffer[bytesRead] = '\0';  // Null terminate
    file.close();

    Serial.print("Buffer (");
    Serial.print(bytesRead);
    Serial.print(" bytes): ");
    Serial.println((char*)buffer);

    // ======================
    // APPEND to file
    // ======================
    Serial.println("\nAppending to file...");

    file.open(FileMode::APPEND);
    file.write("Line 4: Appended!\n");
    file.close();

    // Read back to verify
    file.open(FileMode::READ);
    Serial.println("File after append:");
    Serial.println("---");
    Serial.print(file.readAsString());
    Serial.println("---");
    file.close();

    // ======================
    // File info
    // ======================
    Serial.print("\nFile size: ");
    Serial.print(file.size());
    Serial.println(" bytes");

    Serial.print("File exists: ");
    Serial.println(file.exists() ? "Yes" : "No");

    Serial.println("\nDone!");
}

void loop() {
    delay(10000);
}
