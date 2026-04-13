/*
  QSPIStorage - File Copy and Move Example

  This example demonstrates how to copy and move files and folders
  using the QSPIStorage library.

  Operations demonstrated:
  - Creating files with content
  - Copying files (read + write)
  - Moving/renaming files
  - Creating folders
  - Copying folders recursively
  - Moving/renaming folders

  This example code is in the public domain.
*/

#include <QSPIStorage.h>

QSPIStorage storage;

// Helper function to copy a file
bool copyFile(const char* srcPath, const char* dstPath) {
    QSPIFile srcFile(srcPath);
    StorageError error;

    if (!srcFile.open(FileMode::READ, &error)) {
        Serial.print("Failed to open source file: ");
        Serial.println(error.getMessage());
        return false;
    }

    QSPIFile dstFile(dstPath);
    if (!dstFile.open(FileMode::WRITE, &error)) {
        Serial.print("Failed to create destination file: ");
        Serial.println(error.getMessage());
        srcFile.close();
        return false;
    }

    // Copy in chunks
    uint8_t buffer[256];
    size_t totalCopied = 0;

    while (srcFile.available() > 0) {
        size_t bytesRead = srcFile.read(buffer, sizeof(buffer), &error);
        if (bytesRead == 0) break;

        size_t bytesWritten = dstFile.write(buffer, bytesRead, &error);
        if (bytesWritten != bytesRead) {
            Serial.println("Write error during copy");
            srcFile.close();
            dstFile.close();
            return false;
        }
        totalCopied += bytesWritten;
    }

    srcFile.close();
    dstFile.close();

    Serial.print("Copied ");
    Serial.print(totalCopied);
    Serial.println(" bytes");
    return true;
}

// Helper function to copy a folder recursively
bool copyFolder(QSPIFolder& srcFolder, QSPIFolder& dstFolder) {
    StorageError error;

    // Create destination folder if it doesn't exist
    if (!dstFolder.exists()) {
        if (!dstFolder.create(&error)) {
            Serial.print("Failed to create destination folder: ");
            Serial.println(error.getMessage());
            return false;
        }
    }

    // Copy all files
    std::vector<QSPIFile> files = srcFolder.getFiles(&error);
    for (auto& file : files) {
        String srcPath = file.getPath();
        String filename = file.getFilename();
        String dstPath = String(dstFolder.getPath()) + "/" + filename;

        Serial.print("  Copying file: ");
        Serial.print(filename);
        Serial.print(" -> ");
        Serial.println(dstPath);

        if (!copyFile(srcPath.c_str(), dstPath.c_str())) {
            return false;
        }
    }

    // Recursively copy subfolders
    std::vector<QSPIFolder> subfolders = srcFolder.getFolders(&error);
    for (auto& subfolder : subfolders) {
        String subName = subfolder.getFolderName();
        String dstSubPath = String(dstFolder.getPath()) + "/" + subName;

        Serial.print("  Copying subfolder: ");
        Serial.println(subName);

        QSPIFolder dstSubfolder(dstSubPath);
        if (!copyFolder(subfolder, dstSubfolder)) {
            return false;
        }
    }

    return true;
}

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        delay(10);
    }

    Serial.println("\n========================================");
    Serial.println("QSPIStorage - File Copy & Move Example");
    Serial.println("========================================\n");

    // Initialize storage
    StorageError error;
    if (!storage.begin(&error)) {
        Serial.print("Storage initialization failed: ");
        Serial.println(error.getMessage());
        return;
    }
    Serial.println("Storage initialized successfully!\n");

    // Get root folder
    QSPIFolder root = storage.getRootFolder();

    // ========================================
    // Part 1: Create test files and folders
    // ========================================
    Serial.println("--- Creating Test Files ---\n");

    // Create a test folder
    QSPIFolder testFolder = root.createSubfolder("test_copy", true, &error);
    if (error) {
        Serial.print("Failed to create test folder: ");
        Serial.println(error.getMessage());
        return;
    }
    Serial.println("Created /storage/test_copy/");

    // Create a test file with content
    QSPIFile file1 = testFolder.createFile("original.txt", FileMode::WRITE, &error);
    if (!error) {
        file1.write("Hello, this is the original file content!\n");
        file1.write("Line 2 of the file.\n");
        file1.write("Line 3 - the end.\n");
        file1.close();
        Serial.println("Created /storage/test_copy/original.txt");
    }

    // Create a subfolder with files
    QSPIFolder subFolder = testFolder.createSubfolder("subfolder", false, &error);
    if (!error) {
        Serial.println("Created /storage/test_copy/subfolder/");

        QSPIFile subFile = subFolder.createFile("data.txt", FileMode::WRITE, &error);
        if (!error) {
            subFile.write("Data in subfolder\n");
            subFile.close();
            Serial.println("Created /storage/test_copy/subfolder/data.txt");
        }
    }

    // ========================================
    // Part 2: Copy a file
    // ========================================
    Serial.println("\n--- Copying Files ---\n");

    Serial.println("Copying original.txt to copy.txt...");
    if (copyFile("/storage/test_copy/original.txt", "/storage/test_copy/copy.txt")) {
        Serial.println("File copied successfully!");
    }

    // ========================================
    // Part 3: Move/Rename a file
    // ========================================
    Serial.println("\n--- Moving/Renaming Files ---\n");

    // Create a file to move
    QSPIFile moveFile = testFolder.createFile("to_move.txt", FileMode::WRITE, &error);
    if (!error) {
        moveFile.write("This file will be moved.\n");
        moveFile.close();
        Serial.println("Created /storage/test_copy/to_move.txt");
    }

    // Rename/move the file
    QSPIFile fileToMove("/storage/test_copy/to_move.txt");
    if (fileToMove.rename("/storage/test_copy/moved.txt", &error)) {
        Serial.println("Renamed to_move.txt -> moved.txt");
    } else {
        Serial.print("Failed to rename file: ");
        Serial.println(error.getMessage());
    }

    // ========================================
    // Part 4: Copy a folder
    // ========================================
    Serial.println("\n--- Copying Folders ---\n");

    Serial.println("Copying test_copy/ to test_backup/...");
    QSPIFolder backupFolder = root.createSubfolder("test_backup", true, &error);
    if (copyFolder(testFolder, backupFolder)) {
        Serial.println("Folder copied successfully!");
    }

    // ========================================
    // Part 5: Rename a folder
    // ========================================
    Serial.println("\n--- Renaming Folders ---\n");

    // Create a folder to rename
    QSPIFolder renameFolder = root.createSubfolder("old_name", true, &error);
    if (!error) {
        // Add a file to it
        QSPIFile rf = renameFolder.createFile("info.txt", FileMode::WRITE, &error);
        if (!error) {
            rf.write("Folder rename test\n");
            rf.close();
        }
        Serial.println("Created /storage/old_name/");

        // Rename the folder
        if (renameFolder.rename("new_name", &error)) {
            Serial.println("Renamed old_name/ -> new_name/");
        } else {
            Serial.print("Failed to rename folder: ");
            Serial.println(error.getMessage());
        }
    }

    // ========================================
    // Part 6: Show results
    // ========================================
    Serial.println("\n--- Final Directory Contents ---\n");
    QSPIStorage::listDirectory("/storage");
    QSPIStorage::listDirectory("/storage/test_copy");
    QSPIStorage::listDirectory("/storage/test_backup");

    // ========================================
    // Part 7: Cleanup (optional)
    // ========================================
    Serial.println("\n--- Cleanup ---\n");
    Serial.println("To clean up test files, uncomment the cleanup code.");

    // Uncomment to clean up:
    /*
    QSPIFolder("/storage/test_copy").remove(true, &error);
    QSPIFolder("/storage/test_backup").remove(true, &error);
    QSPIFolder("/storage/new_name").remove(true, &error);
    Serial.println("Test folders removed.");
    */

    Serial.println("\n========================================");
    Serial.println("Done!");
    Serial.println("========================================");
}

void loop() {
    delay(10000);
}
