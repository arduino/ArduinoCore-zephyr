# QSPI Storage Implementation

## Overview

The QSPI Storage library provides a file system implementation for QSPI flash memory, conforming to the Base Storage API. It builds upon the low-level `QSPIClass` interface to provide file and folder abstractions with comprehensive error handling.

This library implements the Base Storage API on top of a file system (LittleFS recommended) mounted on QSPI flash.

---

## Architecture

```
┌─────────────────────────────────────┐
│   QSPIFile / QSPIFolder Classes     │  ← Base Storage API Implementation
├─────────────────────────────────────┤
│     File System Layer (LittleFS)    │  ← Zephyr FS Subsystem
├─────────────────────────────────────┤
│         QSPIClass Interface          │  ← Low-level flash operations
├─────────────────────────────────────┤
│    Zephyr Flash Driver (QSPI)       │  ← Hardware abstraction
└─────────────────────────────────────┘
```

---

## Dependencies

- **Base Storage API**: Conforms to `BaseStorageAPI.md` specification
- **QSPIClass**: Low-level flash interface (`QSPI.h`)
- **Zephyr FS**: File system subsystem (LittleFS or FAT)
- **StorageError**: Shared error handling from Base API

---

## QSPIStorage Class

Main storage manager that handles mounting, formatting, and partition management.

### Initialization

```cpp
class QSPIStorage {
public:
    QSPIStorage();

    // Initialize and mount file system
    bool begin(StorageError* error = nullptr);

    // Unmount and deinitialize
    void end(StorageError* error = nullptr);

    // Check if storage is mounted and ready
    bool isMounted() const;

    // Get mount point path (e.g., "/qspi")
    const char* getMountPoint() const;

    // Get storage statistics
    bool getStorageInfo(size_t& total, size_t& used, size_t& available, StorageError* error = nullptr);

private:
    const char* mount_point_;
    bool mounted_;
    struct fs_mount_t mount_config_;
};
```

### Usage Example

```cpp
#include <QSPIStorage.h>

QSPIStorage storage;
StorageError error;

void setup() {
    Serial.begin(115200);

    // Initialize QSPI storage
    if (!storage.begin(&error)) {
        Serial.print("Storage init failed: ");
        Serial.println(error.getMessage());
        return;
    }

    Serial.println("QSPI Storage ready!");

    // Get storage info
    size_t total, used, available;
    storage.getStorageInfo(total, used, available, &error);
    Serial.print("Total: "); Serial.print(total / 1024); Serial.println(" KB");
    Serial.print("Used: "); Serial.print(used / 1024); Serial.println(" KB");
    Serial.print("Available: "); Serial.print(available / 1024); Serial.println(" KB");
}
```

---

## QSPIFile Class

Implements the Base Storage API `StorageFile` interface for QSPI storage.

### Header Definition

```cpp
class QSPIFile {
public:
    // Constructors
    QSPIFile();
    QSPIFile(const char* path);
    QSPIFile(const String& path);

    // Opening and Closing
    bool open(const char* filename, FileMode mode, StorageError* error = nullptr);
    bool open(const String& filename, FileMode mode, StorageError* error = nullptr);
    bool open(FileMode mode = FileMode::READ, StorageError* error = nullptr);
    bool close(StorageError* error = nullptr);
    bool changeMode(FileMode mode, StorageError* error = nullptr);
    bool isOpen() const;

    // Reading Operations
    size_t read(uint8_t* buffer, size_t size, StorageError* error = nullptr);
    int read(StorageError* error = nullptr);
    String readAsString(StorageError* error = nullptr);
    uint32_t available(StorageError* error = nullptr);
    bool seek(size_t offset, StorageError* error = nullptr);
    size_t position(StorageError* error = nullptr);
    size_t size(StorageError* error = nullptr);

    // Writing Operations
    size_t write(const uint8_t* buffer, size_t size, StorageError* error = nullptr);
    size_t write(const String& data, StorageError* error = nullptr);
    size_t write(uint8_t value, StorageError* error = nullptr);
    bool flush(StorageError* error = nullptr);

    // File Management
    bool exists(StorageError* error = nullptr) const;
    bool remove(StorageError* error = nullptr);
    bool rename(const char* newFilename, StorageError* error = nullptr);
    bool rename(const String& newFilename, StorageError* error = nullptr);

    // Path Information
    const char* getPath() const;
    String getPathAsString() const;
    QSPIFolder getParentFolder(StorageError* error = nullptr) const;
    String getFilename() const;

private:
    char path_[256];
    struct fs_file_t file_;
    bool is_open_;
    FileMode mode_;

    // Internal helpers
    bool resolvePath(const char* path, char* resolved, StorageError* error);
    int fileModeToFlags(FileMode mode);
};
```

### Implementation Notes

1. **Path Resolution**: All paths are resolved relative to mount point (e.g., "/qspi/data.txt")
2. **File Handles**: Uses Zephyr `fs_file_t` structure
3. **Buffering**: Write operations are buffered; use `flush()` to ensure data is written
4. **Error Mapping**: Zephyr error codes are mapped to `StorageErrorCode`

### Usage Example

```cpp
QSPIFile file("/qspi/config.txt");
StorageError error;

// Write configuration
if (file.open(FileMode::WRITE, &error)) {
    String config = "wifi_ssid=MyNetwork\n";
    config += "wifi_pass=MyPassword\n";

    file.write(config, &error);
    file.flush(&error);
    file.close(&error);
}

if (error) {
    Serial.print("Write error: ");
    Serial.println(error.getMessage());
}

// Read configuration
if (file.open(FileMode::READ, &error)) {
    String content = file.readAsString(&error);
    Serial.println(content);
    file.close(&error);
}
```

---

## QSPIFolder Class

Implements the Base Storage API `StorageFolder` interface for QSPI storage.

### Header Definition

```cpp
class QSPIFolder {
public:
    // Constructors
    QSPIFolder();
    QSPIFolder(const char* path);
    QSPIFolder(const String& path);

    // File Operations
    QSPIFile createFile(const char* filename, FileMode mode = FileMode::WRITE, StorageError* error = nullptr);
    QSPIFile createFile(const String& filename, FileMode mode = FileMode::WRITE, StorageError* error = nullptr);
    QSPIFile getFile(const char* filename, StorageError* error = nullptr);
    QSPIFile getFile(const String& filename, StorageError* error = nullptr);

    // Directory Management
    bool exists(StorageError* error = nullptr) const;
    bool create(StorageError* error = nullptr);
    bool remove(bool recursive = false, StorageError* error = nullptr);
    bool rename(const char* newName, StorageError* error = nullptr);
    bool rename(const String& newName, StorageError* error = nullptr);

    // Subfolder Operations
    QSPIFolder createSubfolder(const char* name, bool overwrite = false, StorageError* error = nullptr);
    QSPIFolder createSubfolder(const String& name, bool overwrite = false, StorageError* error = nullptr);
    QSPIFolder getSubfolder(const char* name, StorageError* error = nullptr);
    QSPIFolder getSubfolder(const String& name, StorageError* error = nullptr);

    // Content Enumeration
    std::vector<QSPIFile> getFiles(StorageError* error = nullptr);
    std::vector<QSPIFolder> getFolders(StorageError* error = nullptr);
    size_t getFileCount(StorageError* error = nullptr);
    size_t getFolderCount(StorageError* error = nullptr);

    // Path Information
    const char* getPath() const;
    String getPathAsString() const;
    QSPIFolder getParentFolder(StorageError* error = nullptr) const;
    String getFolderName() const;

private:
    char path_[256];

    // Internal helpers
    bool resolvePath(const char* path, char* resolved, StorageError* error);
    bool removeRecursive(const char* path, StorageError* error);
};
```

### Implementation Notes

1. **Directory Operations**: Uses Zephyr `fs_opendir()` / `fs_readdir()` / `fs_closedir()`
2. **Recursive Operations**: `remove(true)` handles nested structures
3. **Path Building**: Automatically handles path separators and mount points
4. **Enumeration**: Returns vectors of file/folder objects for easy iteration

### Usage Example

```cpp
QSPIFolder dataFolder("/qspi/data");
StorageError error;

// Create folder structure
if (!dataFolder.exists(&error)) {
    dataFolder.create(&error);
}

// Create subfolders
QSPIFolder logsFolder = dataFolder.createSubfolder("logs", false, &error);
QSPIFolder configFolder = dataFolder.createSubfolder("config", false, &error);

// Create files in subfolder
QSPIFile logFile = logsFolder.createFile("app.log", FileMode::WRITE, &error);
if (logFile.isOpen()) {
    logFile.write("Application started\n", &error);
    logFile.close(&error);
}

// List all files
std::vector<QSPIFile> files = dataFolder.getFiles(&error);
Serial.print("Found ");
Serial.print(files.size());
Serial.println(" files:");

for (auto& file : files) {
    Serial.print("  - ");
    Serial.println(file.getFilename());
}
```

---

## Error Code Mapping

Zephyr file system errors are mapped to `StorageErrorCode`:

| Zephyr Error | StorageErrorCode |
|--------------|------------------|
| `-ENOENT` | `FILE_NOT_FOUND` / `FOLDER_NOT_FOUND` |
| `-EEXIST` | `ALREADY_EXISTS` |
| `-EINVAL` | `INVALID_PATH` |
| `-EACCES` | `PERMISSION_DENIED` |
| `-EIO` | `READ_ERROR` / `WRITE_ERROR` |
| `-ENOSPC` | `STORAGE_FULL` |
| `-EROFS` | `PERMISSION_DENIED` |
| `-ENODEV` | `STORAGE_NOT_MOUNTED` |
| Other | `UNKNOWN_ERROR` |

---

## Performance Considerations

### Write Optimization

1. **Buffering**: Enable write buffering in LittleFS configuration
2. **Block Alignment**: Align writes to flash page size when possible
3. **Batch Operations**: Group multiple writes before calling `flush()`

### Read Optimization

1. **Read-Ahead**: Configure LittleFS cache size appropriately
2. **Sequential Access**: Sequential reads are faster than random access
3. **File Size**: Check file size before reading to allocate buffers efficiently

### Memory Usage

- **Stack**: Path buffers use 256 bytes per object
- **Heap**: File system cache configurable (default 512 bytes per cache)
- **Static**: Mount structures and device handles

---

## Configuration

### Zephyr Configuration (prj.conf)

```ini
# File system support
CONFIG_FILE_SYSTEM=y
CONFIG_FILE_SYSTEM_LITTLEFS=y

# QSPI Flash
CONFIG_FLASH=y
CONFIG_FLASH_MAP=y

# LittleFS settings
CONFIG_FS_LITTLEFS_CACHE_SIZE=512
CONFIG_FS_LITTLEFS_LOOKAHEAD_SIZE=32
CONFIG_FS_LITTLEFS_BLOCK_CYCLES=512
```

### Device Tree Partition (board.overlay)

```dts
&mx25r64 {
    partitions {
        compatible = "fixed-partitions";
        #address-cells = <1>;
        #size-cells = <1>;

        /* Storage partition: 7.5MB */
        storage_partition: partition@80000 {
            label = "storage";
            reg = <0x00080000 0x00780000>;
        };
    };
};
```

---

## Complete Example

```cpp
#include <Arduino.h>
#include <QSPIStorage.h>

QSPIStorage storage;

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);

    // Initialize storage
    StorageError error;
    if (!storage.begin(&error)) {
        Serial.print("Failed to initialize storage: ");
        Serial.println(error.getMessage());
        return;
    }

    // Create folder structure
    QSPIFolder root("/qspi");
    QSPIFolder dataFolder = root.createSubfolder("data", false, &error);

    // Create and write to file
    QSPIFile dataFile = dataFolder.createFile("sensor.csv", FileMode::WRITE, &error);
    if (dataFile.isOpen()) {
        dataFile.write("timestamp,temperature,humidity\n", &error);
        dataFile.write("1234567890,23.5,45.2\n", &error);
        dataFile.flush(&error);
        dataFile.close(&error);
    }

    // Read back file
    dataFile.open(FileMode::READ, &error);
    if (dataFile.isOpen()) {
        String content = dataFile.readAsString(&error);
        Serial.println("File content:");
        Serial.println(content);
        dataFile.close(&error);
    }

    // List all files in folder
    std::vector<QSPIFile> files = dataFolder.getFiles(&error);
    Serial.print("Files in /qspi/data: ");
    Serial.println(files.size());

    for (auto& file : files) {
        Serial.print("  - ");
        Serial.print(file.getFilename());
        Serial.print(" (");
        Serial.print(file.size(&error));
        Serial.println(" bytes)");
    }

    if (error) {
        Serial.print("Error occurred: ");
        Serial.println(error.getMessage());
    }
}

void loop() {
    // Nothing to do
}
```

---

## Testing Guidelines

### Unit Tests

1. **Initialization**: Test mount/unmount cycles
2. **File Operations**: Test create, read, write, delete, rename
3. **Folder Operations**: Test create, enumerate, remove (recursive)
4. **Error Handling**: Test error propagation and recovery
5. **Edge Cases**: Test full storage, long paths, special characters

### Integration Tests

1. **Power Loss**: Verify file system integrity after simulated power loss
2. **Stress Test**: Continuous read/write cycles
3. **Fragmentation**: Test performance with many small files
4. **Wear Leveling**: Monitor flash wear distribution

---

## Limitations

1. **Path Length**: Maximum path length is 255 characters
2. **Filename**: Maximum filename length depends on FS (typically 255 chars for LittleFS)
3. **Open Files**: Limited by `CONFIG_FS_LITTLEFS_CACHE_SIZE` and available memory
4. **Concurrent Access**: No file locking; avoid concurrent writes to same file
5. **Flash Wear**: QSPI flash has limited write/erase cycles (~100K typical)

---

## Migration from Raw QSPI

### Before (Raw QSPI)

```cpp
#include <QSPI.h>

QSPI.begin();
uint8_t data[256];
QSPI.read(0x1000, data, 256);
QSPI.write(0x2000, data, 256);
```

### After (QSPI Storage)

```cpp
#include <QSPIStorage.h>

QSPIStorage storage;
storage.begin();

QSPIFile file("/qspi/data.bin");
file.open(FileMode::READ_WRITE_CREATE);
file.write(data, 256);
file.seek(0);
file.read(buffer, 256);
file.close();
```

**Benefits**:
- File system structure and organization
- Automatic wear leveling
- Power-loss recovery
- Standard file operations

---

## Version

**Library Version**: 1.0.0
**Base API Version**: 1.0.0
**Status**: Draft Proposal
**Last Updated**: 2025-12-04
