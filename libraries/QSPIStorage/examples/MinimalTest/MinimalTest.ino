/*
  QSPIStorage Minimal Test
*/

#include <QSPIStorage.h>

QSPIStorage storage;

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);

    Serial.println("QSPIStorage Test");
    Serial.println("================");
    Serial.print("Mount point: ");
    Serial.println(storage.getMountPoint());

    Serial.println("Calling begin()...");
    StorageError error;
    if (storage.begin(&error)) {
        Serial.println("Storage mounted!");

        size_t total, used, available;
        if (storage.getStorageInfo(total, used, available)) {
            Serial.print("Total: ");
            Serial.print(total / 1024);
            Serial.println(" KB");
            Serial.print("Used: ");
            Serial.print(used / 1024);
            Serial.println(" KB");
            Serial.print("Available: ");
            Serial.print(available / 1024);
            Serial.println(" KB");
        }
    } else {
        Serial.print("Mount failed: ");
        Serial.println(error.getMessage());
    }

    Serial.println("Done!");
}

void loop() {
    delay(1000);
}
