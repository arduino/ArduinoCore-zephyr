# QSPIStorage Library

The **QSPIStorage** library provides a simple interface for accessing QSPI flash storage on Arduino boards running Zephyr RTOS. It offers file and folder operations using the LittleFS filesystem.

## Features

- Read and write files to QSPI flash memory
- Create, rename, and delete files and folders
- Binary data storage for structs and arrays
- Directory listing and enumeration
- Storage statistics (total, used, available space)
- Seek operations for random file access
- Copy and move files between locations

## Compatibility

This library is designed for Arduino boards with QSPI flash storage running the Zephyr-based Arduino core:

| Board | QSPI Flash | Status |
|-------|------------|--------|
| Arduino GIGA R1 WiFi | 16 MB | Supported |
| Arduino Portenta H7 | 16 MB | Supported |
| Arduino Portenta C33 | 16 MB | Supported |

## Examples

### SimpleReadWrite

The simplest example - write a string to a file and read it back:

```cpp
#include <QSPIStorage.h>

QSPIStorage storage;

void setup() {
    storage.begin();

    QSPIFile file("/storage/test.txt");
    file.open(FileMode::WRITE);
    file.write("Hello World!");
    file.close();

    file.open(FileMode::READ);
    String content = file.readAsString();
    Serial.println(content);
    file.close();
}
```

### BasicFileOperations

Demonstrates common file operations including creating, reading, writing, and deleting files with proper error handling.

### BasicFolderOperations

Shows how to create folders, list directory contents, create nested folder structures, and manage subfolders.

### BinaryStorage

Store and retrieve binary data such as structs and arrays - useful for sensor data logging and configuration storage.

### FileCopyMove

Copy and move files and folders between locations on the storage.

### ListFiles

List all mounted filesystems and display their contents with file sizes.

## Quick Start

### 1. Include the library

```cpp
#include <QSPIStorage.h>
```

### 2. Create a storage instance and initialize

```cpp
QSPIStorage storage;

void setup() {
    Serial.begin(115200);

    if (!storage.begin()) {
        Serial.println("Failed to initialize storage!");
        return;
    }
}
```

### 3. Work with files

```cpp
// Write a file
QSPIFile file("/storage/data.txt");
file.open(FileMode::WRITE);
file.write("Hello!");
file.close();

// Read a file
file.open(FileMode::READ);
String content = file.readAsString();
file.close();

// Append to a file
file.open(FileMode::APPEND);
file.write("\nMore data");
file.close();
```

### 4. Work with folders

```cpp
QSPIFolder root = storage.getRootFolder();

// Create a subfolder
QSPIFolder data = root.createSubfolder("data");

// Create a file in the folder
QSPIFile log = data.createFile("log.txt", FileMode::WRITE);
log.write("Log entry");
log.close();

// List files
std::vector<QSPIFile> files = data.getFiles();
for (auto& f : files) {
    Serial.println(f.getFilename());
}
```

## File Modes

| Mode | Description |
|------|-------------|
| `FileMode::READ` | Open for reading (file must exist) |
| `FileMode::WRITE` | Create or truncate file for writing |
| `FileMode::APPEND` | Open for writing at end of file |
| `FileMode::READ_WRITE` | Open for both reading and writing |

## Error Handling

All methods accept an optional `StorageError*` parameter:

```cpp
StorageError error;
if (!file.open(FileMode::READ, &error)) {
    Serial.print("Error: ");
    Serial.println(error.getMessage());
}
```

## Storage Layout

The QSPI flash is partitioned via devicetree:

| Partition | Filesystem | Mount Point | Purpose |
|-----------|------------|-------------|---------|
| wlan | FAT | `/wlan:` | WiFi certificates |
| ota | FAT | `/ota:` | OTA updates |
| storage | LittleFS | `/storage` | User data |

This library accesses the `/storage` partition.

## API Reference

### QSPIStorage

| Method | Description |
|--------|-------------|
| `begin()` | Initialize and verify storage is mounted |
| `end()` | Mark storage as not in use |
| `isMounted()` | Check if storage is ready |
| `getStorageInfo()` | Get total, used, and available space |
| `getRootFolder()` | Get the root folder object |
| `listMounts()` | Print mounted filesystems to Serial |
| `listDirectory()` | Print directory contents to Serial |

### QSPIFile

| Method | Description |
|--------|-------------|
| `open()` | Open the file with specified mode |
| `close()` | Close the file |
| `read()` | Read bytes into a buffer |
| `readAsString()` | Read entire file as String |
| `write()` | Write bytes or String |
| `seek()` | Move to position in file |
| `position()` | Get current position |
| `size()` | Get file size |
| `available()` | Get bytes remaining to read |
| `exists()` | Check if file exists |
| `remove()` | Delete the file |
| `rename()` | Rename or move the file |

### QSPIFolder

| Method | Description |
|--------|-------------|
| `exists()` | Check if folder exists |
| `create()` | Create the folder |
| `remove()` | Delete the folder |
| `rename()` | Rename or move the folder |
| `createFile()` | Create a file in this folder |
| `getFile()` | Get a file object |
| `createSubfolder()` | Create a subfolder |
| `getSubfolder()` | Get a subfolder object |
| `getFiles()` | List all files |
| `getFolders()` | List all subfolders |
| `getFileCount()` | Count files |
| `getFolderCount()` | Count subfolders |

## License

Copyright (c) 2024 Arduino SA. Licensed under the Apache License, Version 2.0.
