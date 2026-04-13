/*
  QSPIStorage - Basic File Operations Example

  This example demonstrates basic file and folder operations using the
  QSPIStorage library.

  IMPORTANT REQUIREMENTS:
  =======================
  This library requires LittleFS to be auto-mounted via devicetree FSTAB.
  Your board's devicetree must include:

  1. A storage partition on the QSPI flash
  2. An FSTAB entry that mounts LittleFS at "/storage"

  If you see "Filesystem not mounted" error, the board's devicetree
  needs to be configured for auto-mounting.

  For boards without FSTAB configuration, use the low-level QSPI library
  directly (see QSPI/examples/QSPIFilesystem.ino).
*/

#include <QSPIStorage.h>

QSPIStorage storage;

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        delay(10);
    }

    Serial.println("QSPIStorage - Basic File Operations");
    Serial.println("====================================\n");

    // Initialize storage
    StorageError error;

    Serial.println("Initializing QSPI storage...");
    if (!storage.begin(&error)) {
        Serial.print("Storage initialization failed: ");
        Serial.println(error.getMessage());
        Serial.println("\nNote: This library requires LittleFS auto-mount via devicetree FSTAB.");
        Serial.println("Check your board's devicetree configuration.");
        while (1) delay(1000);
    }

    Serial.println("Storage mounted successfully!\n");

    // Show storage info
    showStorageInfo();

    // Run file operation demos
    demoWriteFile();
    demoReadFile();
    demoListFiles();
    demoDeleteFile();

    Serial.println("\n=== All demos completed ===");
}

void loop() {
    delay(1000);
}

void showStorageInfo() {
    StorageError error;
    size_t total, used, available;

    if (storage.getStorageInfo(total, used, available, &error)) {
        Serial.println("Storage Information:");
        Serial.print("  Total:     ");
        Serial.print(total / 1024);
        Serial.println(" KB");
        Serial.print("  Used:      ");
        Serial.print(used / 1024);
        Serial.println(" KB");
        Serial.print("  Available: ");
        Serial.print(available / 1024);
        Serial.println(" KB");
        Serial.println();
    }
}

void demoWriteFile() {
    Serial.println("--- Demo: Writing a File ---");

    StorageError error;
    QSPIFile file("/storage/hello.txt");

    if (file.open(FileMode::WRITE, &error)) {
        String content = "Hello from QSPIStorage!\n";
        content += "Timestamp: ";
        content += String(millis());
        content += " ms";

        size_t written = file.write(content, &error);
        file.close(&error);

        if (!error) {
            Serial.print("Wrote ");
            Serial.print(written);
            Serial.println(" bytes to hello.txt");
        } else {
            Serial.print("Write error: ");
            Serial.println(error.getMessage());
        }
    } else {
        Serial.print("Failed to open file: ");
        Serial.println(error.getMessage());
    }

    Serial.println();
}

void demoReadFile() {
    Serial.println("--- Demo: Reading a File ---");

    StorageError error;
    QSPIFile file("/storage/hello.txt");

    if (file.open(FileMode::READ, &error)) {
        String content = file.readAsString(&error);
        file.close(&error);

        if (!error) {
            Serial.println("Content of hello.txt:");
            Serial.println("---");
            Serial.println(content);
            Serial.println("---");
        } else {
            Serial.print("Read error: ");
            Serial.println(error.getMessage());
        }
    } else {
        Serial.print("Failed to open file: ");
        Serial.println(error.getMessage());
    }

    Serial.println();
}

void demoListFiles() {
    Serial.println("--- Demo: Listing Files ---");

    StorageError error;
    QSPIFolder root = storage.getRootFolder(&error);

    if (error) {
        Serial.print("Error getting root folder: ");
        Serial.println(error.getMessage());
        return;
    }

    // List files in root
    Serial.println("Files in /storage:");
    std::vector<QSPIFile> files = root.getFiles(&error);

    if (files.empty()) {
        Serial.println("  (no files)");
    } else {
        for (auto& f : files) {
            Serial.print("  ");
            Serial.print(f.getFilename());
            Serial.print(" (");
            Serial.print(f.size(&error));
            Serial.println(" bytes)");
        }
    }

    // List folders
    Serial.println("\nFolders in /storage:");
    std::vector<QSPIFolder> folders = root.getFolders(&error);

    if (folders.empty()) {
        Serial.println("  (no folders)");
    } else {
        for (auto& folder : folders) {
            Serial.print("  [DIR] ");
            Serial.println(folder.getFolderName());
        }
    }

    Serial.println();
}

void demoDeleteFile() {
    Serial.println("--- Demo: Deleting a File ---");

    StorageError error;

    // Create a temp file
    QSPIFile tempFile("/storage/temp.txt");
    if (tempFile.open(FileMode::WRITE, &error)) {
        tempFile.write("Temporary file", &error);
        tempFile.close(&error);
        Serial.println("Created temp.txt");
    }

    // Check existence
    Serial.print("temp.txt exists: ");
    Serial.println(tempFile.exists(&error) ? "Yes" : "No");

    // Delete it
    if (tempFile.remove(&error)) {
        Serial.println("Deleted temp.txt");
    } else {
        Serial.print("Delete failed: ");
        Serial.println(error.getMessage());
    }

    // Verify deletion
    Serial.print("temp.txt exists: ");
    Serial.println(tempFile.exists(&error) ? "Yes" : "No");

    Serial.println();
}
