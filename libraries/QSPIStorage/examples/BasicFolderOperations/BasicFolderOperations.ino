/*
  QSPIStorage - Basic Folder Operations Example

  Simple example showing how to create, list, and manage folders.

  This example code is in the public domain.
*/

#include <QSPIStorage.h>

QSPIStorage storage;

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        delay(10);
    }

    Serial.println("QSPIStorage - Basic Folder Operations\n");

    if (!storage.begin()) {
        Serial.println("Failed to initialize storage!");
        while (1) delay(1000);
    }
    Serial.println("Storage ready.\n");

    // ======================
    // CREATE a folder
    // ======================
    Serial.println("Creating folder...");

    QSPIFolder root = storage.getRootFolder();
    QSPIFolder myFolder = root.createSubfolder("myfolder");

    Serial.print("Created: ");
    Serial.println(myFolder.getPath());

    // ======================
    // CREATE nested folders
    // ======================
    Serial.println("\nCreating nested folders...");

    QSPIFolder level1 = root.createSubfolder("level1");
    QSPIFolder level2 = level1.createSubfolder("level2");
    QSPIFolder level3 = level2.createSubfolder("level3");

    Serial.println("Created: /storage/level1/level2/level3");

    // ======================
    // CREATE files in folder
    // ======================
    Serial.println("\nCreating files in folder...");

    QSPIFile file1 = myFolder.createFile("file1.txt", FileMode::WRITE);
    file1.write("Content of file 1");
    file1.close();

    QSPIFile file2 = myFolder.createFile("file2.txt", FileMode::WRITE);
    file2.write("Content of file 2");
    file2.close();

    Serial.println("Created 2 files in myfolder");

    // ======================
    // LIST folder contents
    // ======================
    Serial.println("\nListing folder contents...");

    Serial.print("Files in ");
    Serial.print(myFolder.getPath());
    Serial.println(":");

    std::vector<QSPIFile> files = myFolder.getFiles();
    for (auto& f : files) {
        Serial.print("  ");
        Serial.print(f.getFilename());
        Serial.print(" (");
        Serial.print(f.size());
        Serial.println(" bytes)");
    }

    // ======================
    // COUNT items
    // ======================
    Serial.println("\nCounting items...");

    Serial.print("Files in myfolder: ");
    Serial.println(myFolder.getFileCount());

    Serial.print("Folders in root: ");
    Serial.println(root.getFolderCount());

    // ======================
    // LIST subfolders
    // ======================
    Serial.println("\nListing subfolders in root:");

    std::vector<QSPIFolder> folders = root.getFolders();
    for (auto& folder : folders) {
        Serial.print("  [DIR] ");
        Serial.println(folder.getFolderName());
    }

    // ======================
    // GET subfolder
    // ======================
    Serial.println("\nGetting existing subfolder...");

    QSPIFolder existing = root.getSubfolder("myfolder");
    Serial.print("Got folder: ");
    Serial.println(existing.getPath());
    Serial.print("It has ");
    Serial.print(existing.getFileCount());
    Serial.println(" files");

    // ======================
    // CHECK if folder exists
    // ======================
    Serial.println("\nChecking folder existence...");

    Serial.print("myfolder exists: ");
    Serial.println(myFolder.exists() ? "Yes" : "No");

    QSPIFolder fake("/storage/doesnotexist");
    Serial.print("doesnotexist exists: ");
    Serial.println(fake.exists() ? "Yes" : "No");

    // ======================
    // GET parent folder
    // ======================
    Serial.println("\nGetting parent folder...");

    QSPIFolder parent = level3.getParentFolder();
    Serial.print("Parent of level3: ");
    Serial.println(parent.getPath());

    // ======================
    // RENAME folder
    // ======================
    Serial.println("\nRenaming folder...");

    QSPIFolder toRename = root.createSubfolder("oldname");
    Serial.print("Created: ");
    Serial.println(toRename.getPath());

    toRename.rename("newname");
    Serial.println("Renamed to: newname");

    // ======================
    // DELETE empty folder
    // ======================
    Serial.println("\nDeleting empty folder...");

    QSPIFolder emptyFolder = root.createSubfolder("todelete");
    Serial.println("Created: todelete");

    emptyFolder.remove();
    Serial.println("Deleted: todelete");

    // ======================
    // DELETE folder with contents (recursive)
    // ======================
    Serial.println("\nDeleting folder with contents...");

    QSPIFolder withFiles = root.createSubfolder("withfiles");
    QSPIFile tempFile = withFiles.createFile("temp.txt", FileMode::WRITE);
    tempFile.write("temp");
    tempFile.close();
    Serial.println("Created: withfiles/temp.txt");

    withFiles.remove(true);  // true = recursive
    Serial.println("Deleted: withfiles (recursive)");

    // ======================
    // FINAL listing
    // ======================
    Serial.println("\nFinal folder listing:");
    QSPIStorage::listDirectory("/storage");

    Serial.println("\nDone!");
}

void loop() {
    delay(10000);
}
