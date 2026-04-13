/*
  QSPIStorage - List Files Example

  This example demonstrates how to list all mounted filesystems
  and their contents using the QSPIStorage library.

  It shows:
  - All mounted filesystems (FAT and LittleFS)
  - Directory contents with file sizes
  - Storage statistics

  This example code is in the public domain.
*/

#include <QSPIStorage.h>

QSPIStorage storage;

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        delay(10);
    }

    Serial.println("\n========================================");
    Serial.println("QSPIStorage - List Files Example");
    Serial.println("========================================\n");

    // Initialize storage
    StorageError error;
    if (!storage.begin(&error)) {
        Serial.print("Storage initialization failed: ");
        Serial.println(error.getMessage());
        Serial.println("\nNote: The filesystem must be formatted first.");
        Serial.println("Run the FormatStorage example if needed.");
    } else {
        Serial.println("Storage initialized successfully!\n");
    }

    // List all mounted filesystems
    Serial.println("=== Mounted Filesystems ===");
    QSPIStorage::listMounts();

    // List contents of all mounted filesystems
    Serial.println("\n=== Filesystem Contents ===");
    QSPIStorage::listAllMounts();

    // Show storage statistics for /storage partition
    Serial.println("\n=== Storage Statistics ===");
    showStorageStats();

    // Demo: Using QSPIFolder to list files with more detail
    Serial.println("\n=== Using QSPIFolder API ===");
    listWithFolderAPI();

    Serial.println("\n========================================");
    Serial.println("Done!");
}

void loop() {
    delay(10000);
}

void showStorageStats() {
    StorageError error;
    size_t total, used, available;

    if (storage.getStorageInfo(total, used, available, &error)) {
        Serial.println("User Storage (/storage):");
        Serial.print("  Total:     ");
        printSize(total);
        Serial.print("  Used:      ");
        printSize(used);
        Serial.print("  Available: ");
        printSize(available);
        Serial.print("  Usage:     ");
        if (total > 0) {
            Serial.print((used * 100) / total);
            Serial.println("%");
        } else {
            Serial.println("N/A");
        }
    } else {
        Serial.print("Error getting storage info: ");
        Serial.println(error.getMessage());
    }
}

void printSize(size_t bytes) {
    if (bytes >= 1024 * 1024) {
        Serial.print(bytes / (1024 * 1024));
        Serial.println(" MB");
    } else if (bytes >= 1024) {
        Serial.print(bytes / 1024);
        Serial.println(" KB");
    } else {
        Serial.print(bytes);
        Serial.println(" bytes");
    }
}

void listWithFolderAPI() {
    StorageError error;

    if (!storage.isMounted()) {
        Serial.println("Storage not mounted, skipping folder API demo.");
        return;
    }

    QSPIFolder root = storage.getRootFolder(&error);
    if (error) {
        Serial.print("Error getting root folder: ");
        Serial.println(error.getMessage());
        return;
    }

    Serial.print("Root folder: ");
    Serial.println(root.getPath());

    // Count items
    size_t fileCount = root.getFileCount(&error);
    size_t folderCount = root.getFolderCount(&error);

    Serial.print("  Files: ");
    Serial.println(fileCount);
    Serial.print("  Folders: ");
    Serial.println(folderCount);

    // List files with details
    if (fileCount > 0) {
        Serial.println("\n  Files:");
        std::vector<QSPIFile> files = root.getFiles(&error);
        for (auto& file : files) {
            Serial.print("    ");
            Serial.print(file.getFilename());
            Serial.print(" (");
            Serial.print(file.size(&error));
            Serial.println(" bytes)");
        }
    }

    // List folders
    if (folderCount > 0) {
        Serial.println("\n  Folders:");
        std::vector<QSPIFolder> folders = root.getFolders(&error);
        for (auto& folder : folders) {
            Serial.print("    [DIR] ");
            Serial.println(folder.getFolderName());

            // Show contents of each subfolder
            size_t subFiles = folder.getFileCount(&error);
            size_t subFolders = folder.getFolderCount(&error);
            Serial.print("          (");
            Serial.print(subFiles);
            Serial.print(" files, ");
            Serial.print(subFolders);
            Serial.println(" folders)");
        }
    }
}
