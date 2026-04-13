/*
  QSPIStorage - Binary Storage Example

  Simple example showing how to store and retrieve binary data (structs).

  This example code is in the public domain.
*/

#include <QSPIStorage.h>

QSPIStorage storage;

// Example struct to store
struct SensorData {
    uint32_t timestamp;
    float temperature;
    float humidity;
    uint16_t lightLevel;
};

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        delay(10);
    }

    Serial.println("QSPIStorage - Binary Storage Example\n");

    if (!storage.begin()) {
        Serial.println("Failed to initialize storage!");
        while (1) delay(1000);
    }
    Serial.println("Storage ready.\n");

    // ======================
    // WRITE a single struct
    // ======================
    Serial.println("Writing single struct...");

    SensorData data;
    data.timestamp = millis();
    data.temperature = 23.5;
    data.humidity = 65.2;
    data.lightLevel = 512;

    QSPIFile file("/storage/sensor.bin");
    file.open(FileMode::WRITE);
    file.write((uint8_t*)&data, sizeof(data));
    file.close();

    Serial.print("Wrote ");
    Serial.print(sizeof(data));
    Serial.println(" bytes\n");

    // ======================
    // READ a single struct
    // ======================
    Serial.println("Reading single struct...");

    SensorData readData;
    file.open(FileMode::READ);
    file.read((uint8_t*)&readData, sizeof(readData));
    file.close();

    Serial.print("  Timestamp: ");
    Serial.println(readData.timestamp);
    Serial.print("  Temperature: ");
    Serial.println(readData.temperature);
    Serial.print("  Humidity: ");
    Serial.println(readData.humidity);
    Serial.print("  Light: ");
    Serial.println(readData.lightLevel);

    // ======================
    // WRITE array of structs
    // ======================
    Serial.println("\nWriting array of structs...");

    const int COUNT = 5;
    SensorData readings[COUNT];

    for (int i = 0; i < COUNT; i++) {
        readings[i].timestamp = millis() + (i * 1000);
        readings[i].temperature = 20.0 + i;
        readings[i].humidity = 50.0 + (i * 2);
        readings[i].lightLevel = 100 * (i + 1);
    }

    QSPIFile arrayFile("/storage/readings.bin");
    arrayFile.open(FileMode::WRITE);

    // Write count first
    uint32_t count = COUNT;
    arrayFile.write((uint8_t*)&count, sizeof(count));

    // Write all readings
    arrayFile.write((uint8_t*)readings, sizeof(readings));
    arrayFile.close();

    Serial.print("Wrote ");
    Serial.print(COUNT);
    Serial.println(" readings\n");

    // ======================
    // READ array of structs
    // ======================
    Serial.println("Reading array of structs...");

    arrayFile.open(FileMode::READ);

    // Read count
    uint32_t readCount;
    arrayFile.read((uint8_t*)&readCount, sizeof(readCount));
    Serial.print("Found ");
    Serial.print(readCount);
    Serial.println(" readings:");

    // Read each one
    for (uint32_t i = 0; i < readCount; i++) {
        SensorData r;
        arrayFile.read((uint8_t*)&r, sizeof(r));
        Serial.print("  [");
        Serial.print(i);
        Serial.print("] T=");
        Serial.print(r.temperature);
        Serial.print(", H=");
        Serial.print(r.humidity);
        Serial.print(", L=");
        Serial.println(r.lightLevel);
    }
    arrayFile.close();

    // ======================
    // APPEND binary data
    // ======================
    Serial.println("\nAppending more data...");

    arrayFile.open(FileMode::READ);
    arrayFile.read((uint8_t*)&count, sizeof(count));
    arrayFile.close();

    // Update count and append new reading
    SensorData newReading = {millis(), 30.0, 70.0, 999};

    arrayFile.open(FileMode::READ_WRITE);

    // Update count at beginning
    count++;
    arrayFile.write((uint8_t*)&count, sizeof(count));

    // Seek to end and append
    arrayFile.seek(sizeof(count) + (count - 1) * sizeof(SensorData));
    arrayFile.write((uint8_t*)&newReading, sizeof(newReading));
    arrayFile.close();

    Serial.print("Total readings now: ");
    Serial.println(count);

    Serial.println("\nDone!");
}

void loop() {
    delay(10000);
}
