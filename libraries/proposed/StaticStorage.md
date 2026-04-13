# Static Storage Utilities Library

## Overview

The Static Storage library provides utility functions for managing storage devices across all storage implementations (QSPI, SD, Flash, etc.). It handles cross-storage operations like formatting, partitioning, and advanced copy/move operations that may span multiple storage backends.

This library provides **static methods** that work with any storage implementation conforming to the Base Storage API, enabling operations that are not tied to a specific storage instance.

---

## Design Principles

1. **Storage-Agnostic**: Works with any Base Storage API implementation
2. **Static Interface**: All methods are static - no instantiation required
3. **Cross-Storage Operations**: Support operations between different storage types
4. **Comprehensive Error Handling**: All methods use `StorageError*` parameter
5. **Utility Functions**: High-level operations built on Base Storage API

---

## Architecture

```
┌────────────────────────────────────────────┐
│        StaticStorage Utility Layer         │  ← Static helper methods
├────────────────────────────────────────────┤
│    Base Storage API (File/Folder/Error)   │  ← Common interface
├─────────────┬──────────────┬───────────────┤
│ QSPI Storage│  SD Storage  │ Flash Storage │  ← Specific implementations
└─────────────┴──────────────┴───────────────┘
```

---

## Dependencies

- **Base Storage API**: Uses `StorageFile`, `StorageFolder`, `StorageError`
- **Storage Implementations**: Works with any conforming implementation
- **Zephyr FS API**: For low-level formatting and partition operations

---

## StaticStorage Class

All methods are static. No instantiation required.

```cpp
#include <StaticStorage.h>

// Direct usage without creating an object
StaticStorage::format("/qspi", FilesystemType::LITTLEFS);
```

---

## File System Types

```cpp
enum class FilesystemType {
    LITTLEFS,   // LittleFS - recommended for flash storage
    FAT,        // FAT32 - better compatibility, larger overhead
    EXT2,       // Extended 2 - Linux-style filesystem
    AUTO        // Auto-detect or use default
};
```

---

## Formatting Operations

### Format Storage

Format a storage device with a specific file system.

```cpp
class StaticStorage {
public:
    /**
     * Format a storage device
     * @param mountPoint Mount point path (e.g., "/qspi", "/sd")
     * @param fsType File system type to format with
     * @param error Optional error output parameter
     * @return true if successful, false otherwise
     */
    static bool format(
        const char* mountPoint,
        FilesystemType fsType = FilesystemType::AUTO,
        StorageError* error = nullptr
    );

    static bool format(
        const String& mountPoint,
        FilesystemType fsType = FilesystemType::AUTO,
        StorageError* error = nullptr
    );

    /**
     * Quick format (faster but less thorough)
     * @param mountPoint Mount point path
     * @param fsType File system type
     * @param error Optional error output parameter
     * @return true if successful
     */
    static bool quickFormat(
        const char* mountPoint,
        FilesystemType fsType = FilesystemType::AUTO,
        StorageError* error = nullptr
    );

    /**
     * Check if a storage device needs formatting
     * @param mountPoint Mount point path
     * @param error Optional error output parameter
     * @return true if formatting is needed
     */
    static bool needsFormatting(
        const char* mountPoint,
        StorageError* error = nullptr
    );
};
```

### Usage Example

```cpp
#include <StaticStorage.h>

void setup() {
    Serial.begin(115200);

    StorageError error;

    // Check if QSPI needs formatting
    if (StaticStorage::needsFormatting("/qspi", &error)) {
        Serial.println("QSPI needs formatting...");

        // Format with LittleFS
        if (StaticStorage::format("/qspi", FilesystemType::LITTLEFS, &error)) {
            Serial.println("Format successful!");
        } else {
            Serial.print("Format failed: ");
            Serial.println(error.getMessage());
        }
    }
}
```

---

## Partitioning Operations

### Partition Management

Create and manage partitions on storage devices.

```cpp
struct PartitionInfo {
    const char* label;      // Partition name/label
    size_t offset;          // Start offset in bytes
    size_t size;            // Size in bytes
    FilesystemType fsType;  // File system type for this partition
};

class StaticStorage {
public:
    /**
     * Create partitions on a storage device
     * @param mountPoint Base mount point (e.g., "/qspi")
     * @param partitions Array of partition definitions
     * @param count Number of partitions
     * @param error Optional error output parameter
     * @return true if successful
     */
    static bool createPartitions(
        const char* mountPoint,
        const PartitionInfo* partitions,
        size_t count,
        StorageError* error = nullptr
    );

    /**
     * List existing partitions
     * @param mountPoint Mount point to query
     * @param partitions Output vector of partition info
     * @param error Optional error output parameter
     * @return true if successful
     */
    static bool listPartitions(
        const char* mountPoint,
        std::vector<PartitionInfo>& partitions,
        StorageError* error = nullptr
    );

    /**
     * Remove all partitions (restore to single partition)
     * @param mountPoint Mount point
     * @param error Optional error output parameter
     * @return true if successful
     */
    static bool removePartitions(
        const char* mountPoint,
        StorageError* error = nullptr
    );

    /**
     * Get partition by label
     * @param mountPoint Base mount point
     * @param label Partition label
     * @param info Output partition information
     * @param error Optional error output parameter
     * @return true if found
     */
    static bool getPartition(
        const char* mountPoint,
        const char* label,
        PartitionInfo& info,
        StorageError* error = nullptr
    );
};
```

### Usage Example

```cpp
#include <StaticStorage.h>

void setup() {
    Serial.begin(115200);
    StorageError error;

    // Define partitions for QSPI flash
    // Assuming 8MB QSPI flash
    PartitionInfo partitions[] = {
        {"config",  0x000000, 512 * 1024,  FilesystemType::LITTLEFS},  // 512KB for config
        {"data",    0x080000, 2 * 1024 * 1024, FilesystemType::LITTLEFS},  // 2MB for data
        {"logs",    0x280000, 1 * 1024 * 1024, FilesystemType::LITTLEFS},  // 1MB for logs
        {"storage", 0x380000, 4 * 1024 * 1024, FilesystemType::FAT}       // 4MB for storage
    };

    // Create partitions
    if (StaticStorage::createPartitions("/qspi", partitions, 4, &error)) {
        Serial.println("Partitions created successfully!");

        // List partitions
        std::vector<PartitionInfo> found;
        StaticStorage::listPartitions("/qspi", found, &error);

        Serial.print("Found ");
        Serial.print(found.size());
        Serial.println(" partitions:");

        for (auto& part : found) {
            Serial.print("  - ");
            Serial.print(part.label);
            Serial.print(": ");
            Serial.print(part.size / 1024);
            Serial.println(" KB");
        }
    } else {
        Serial.print("Partition creation failed: ");
        Serial.println(error.getMessage());
    }
}
```

---

## Cross-Storage Copy/Move Operations

Advanced copy and move operations that work across different storage backends.

```cpp
class StaticStorage {
public:
    /**
     * Copy file between different storage devices
     * @param srcFile Source file object (any storage type)
     * @param destPath Destination path (may be different storage)
     * @param overwrite Overwrite if exists
     * @param progress Optional progress callback (bytes_copied, total_bytes)
     * @param error Optional error output parameter
     * @return true if successful
     */
    static bool copyFile(
        const StorageFile& srcFile,
        const char* destPath,
        bool overwrite = false,
        void (*progress)(size_t, size_t) = nullptr,
        StorageError* error = nullptr
    );

    static bool copyFile(
        const char* srcPath,
        const char* destPath,
        bool overwrite = false,
        void (*progress)(size_t, size_t) = nullptr,
        StorageError* error = nullptr
    );

    /**
     * Move file between different storage devices
     * @param srcFile Source file object
     * @param destPath Destination path
     * @param overwrite Overwrite if exists
     * @param progress Optional progress callback
     * @param error Optional error output parameter
     * @return true if successful
     */
    static bool moveFile(
        const StorageFile& srcFile,
        const char* destPath,
        bool overwrite = false,
        void (*progress)(size_t, size_t) = nullptr,
        StorageError* error = nullptr
    );

    static bool moveFile(
        const char* srcPath,
        const char* destPath,
        bool overwrite = false,
        void (*progress)(size_t, size_t) = nullptr,
        StorageError* error = nullptr
    );

    /**
     * Copy folder between different storage devices (recursive)
     * @param srcFolder Source folder object
     * @param destPath Destination path
     * @param overwrite Overwrite if exists
     * @param progress Optional progress callback
     * @param error Optional error output parameter
     * @return true if successful
     */
    static bool copyFolder(
        const StorageFolder& srcFolder,
        const char* destPath,
        bool overwrite = false,
        void (*progress)(size_t, size_t) = nullptr,
        StorageError* error = nullptr
    );

    static bool copyFolder(
        const char* srcPath,
        const char* destPath,
        bool overwrite = false,
        void (*progress)(size_t, size_t) = nullptr,
        StorageError* error = nullptr
    );

    /**
     * Move folder between different storage devices (recursive)
     * @param srcFolder Source folder object
     * @param destPath Destination path
     * @param overwrite Overwrite if exists
     * @param progress Optional progress callback
     * @param error Optional error output parameter
     * @return true if successful
     */
    static bool moveFolder(
        const StorageFolder& srcFolder,
        const char* destPath,
        bool overwrite = false,
        void (*progress)(size_t, size_t) = nullptr,
        StorageError* error = nullptr
    );

    static bool moveFolder(
        const char* srcPath,
        const char* destPath,
        bool overwrite = false,
        void (*progress)(size_t, size_t) = nullptr,
        StorageError* error = nullptr
    );

    /**
     * Synchronize folders (copy only changed files)
     * @param srcPath Source folder path
     * @param destPath Destination folder path
     * @param bidirectional If true, sync both ways
     * @param progress Optional progress callback
     * @param error Optional error output parameter
     * @return true if successful
     */
    static bool syncFolders(
        const char* srcPath,
        const char* destPath,
        bool bidirectional = false,
        void (*progress)(size_t, size_t) = nullptr,
        StorageError* error = nullptr
    );
};
```

### Usage Example

```cpp
#include <StaticStorage.h>
#include <QSPIStorage.h>
#include <SDStorage.h>

void progressCallback(size_t copied, size_t total) {
    int percent = (copied * 100) / total;
    Serial.print("\rProgress: ");
    Serial.print(percent);
    Serial.print("%");
}

void setup() {
    Serial.begin(115200);

    // Initialize both storage devices
    QSPIStorage qspi;
    SDStorage sd;

    StorageError error;

    qspi.begin(&error);
    sd.begin(&error);

    // Copy file from QSPI to SD with progress
    Serial.println("Copying large file from QSPI to SD...");
    if (StaticStorage::copyFile(
        "/qspi/data/large_file.bin",
        "/sd/backup/large_file.bin",
        true,  // overwrite
        progressCallback,
        &error
    )) {
        Serial.println("\nCopy successful!");
    } else {
        Serial.print("\nCopy failed: ");
        Serial.println(error.getMessage());
    }

    // Backup entire QSPI config folder to SD
    Serial.println("Backing up config folder...");
    if (StaticStorage::copyFolder(
        "/qspi/config",
        "/sd/backup/config",
        true,
        progressCallback,
        &error
    )) {
        Serial.println("\nBackup successful!");
    }

    // Synchronize data folders between QSPI and SD
    Serial.println("Syncing data folders...");
    if (StaticStorage::syncFolders(
        "/qspi/data",
        "/sd/data",
        true,  // bidirectional
        progressCallback,
        &error
    )) {
        Serial.println("\nSync successful!");
    }
}
```

---

## Storage Information and Analysis

```cpp
class StaticStorage {
public:
    /**
     * Get detailed storage information
     * @param mountPoint Mount point to query
     * @param info Output storage information structure
     * @param error Optional error output parameter
     * @return true if successful
     */
    static bool getStorageInfo(
        const char* mountPoint,
        StorageInfo& info,
        StorageError* error = nullptr
    );

    /**
     * Check storage health
     * @param mountPoint Mount point to check
     * @param health Output health information
     * @param error Optional error output parameter
     * @return true if successful
     */
    static bool checkHealth(
        const char* mountPoint,
        StorageHealth& health,
        StorageError* error = nullptr
    );

    /**
     * Estimate available write cycles (for flash storage)
     * @param mountPoint Mount point
     * @param error Optional error output parameter
     * @return Estimated remaining write cycles (0 = unknown)
     */
    static uint32_t estimateRemainingCycles(
        const char* mountPoint,
        StorageError* error = nullptr
    );

    /**
     * Optimize storage (defragment, garbage collect, etc.)
     * @param mountPoint Mount point
     * @param error Optional error output parameter
     * @return true if successful
     */
    static bool optimize(
        const char* mountPoint,
        StorageError* error = nullptr
    );
};

struct StorageInfo {
    char mountPoint[64];
    FilesystemType fsType;
    size_t totalBytes;
    size_t usedBytes;
    size_t availableBytes;
    size_t blockSize;
    size_t totalBlocks;
    size_t usedBlocks;
    bool readOnly;
    bool mounted;
};

struct StorageHealth {
    bool healthy;                    // Overall health status
    uint32_t errorCount;             // Number of errors encountered
    uint32_t badBlocks;              // Number of bad blocks (flash)
    uint32_t writeCount;             // Total write operations
    uint32_t eraseCount;             // Total erase operations
    float fragmentationPercent;      // File system fragmentation
    char statusMessage[128];         // Human-readable status
};
```

### Usage Example

```cpp
#include <StaticStorage.h>

void printStorageInfo(const char* mountPoint) {
    StorageInfo info;
    StorageHealth health;
    StorageError error;

    // Get storage info
    if (StaticStorage::getStorageInfo(mountPoint, info, &error)) {
        Serial.println("=== Storage Information ===");
        Serial.print("Mount Point: "); Serial.println(info.mountPoint);
        Serial.print("Total: "); Serial.print(info.totalBytes / 1024); Serial.println(" KB");
        Serial.print("Used: "); Serial.print(info.usedBytes / 1024); Serial.println(" KB");
        Serial.print("Available: "); Serial.print(info.availableBytes / 1024); Serial.println(" KB");
        Serial.print("Usage: ");
        Serial.print((info.usedBytes * 100) / info.totalBytes);
        Serial.println("%");
    }

    // Check health
    if (StaticStorage::checkHealth(mountPoint, health, &error)) {
        Serial.println("\n=== Storage Health ===");
        Serial.print("Status: ");
        Serial.println(health.healthy ? "HEALTHY" : "WARNING");
        Serial.print("Errors: "); Serial.println(health.errorCount);
        Serial.print("Bad Blocks: "); Serial.println(health.badBlocks);
        Serial.print("Fragmentation: ");
        Serial.print(health.fragmentationPercent);
        Serial.println("%");
        Serial.print("Message: "); Serial.println(health.statusMessage);

        // Optimize if fragmented
        if (health.fragmentationPercent > 50.0) {
            Serial.println("High fragmentation detected. Optimizing...");
            if (StaticStorage::optimize(mountPoint, &error)) {
                Serial.println("Optimization complete!");
            }
        }
    }

    // Estimate remaining cycles (for flash)
    uint32_t cycles = StaticStorage::estimateRemainingCycles(mountPoint, &error);
    if (cycles > 0) {
        Serial.print("Estimated remaining write cycles: ");
        Serial.println(cycles);
    }
}

void setup() {
    Serial.begin(115200);

    QSPIStorage qspi;
    qspi.begin();

    printStorageInfo("/qspi");
}
```

---

## Utility Functions

```cpp
class StaticStorage {
public:
    /**
     * Compare two files (byte-by-byte)
     * @param file1Path First file path
     * @param file2Path Second file path
     * @param error Optional error output parameter
     * @return true if files are identical
     */
    static bool compareFiles(
        const char* file1Path,
        const char* file2Path,
        StorageError* error = nullptr
    );

    /**
     * Calculate file checksum (CRC32)
     * @param filePath File path
     * @param checksum Output checksum value
     * @param error Optional error output parameter
     * @return true if successful
     */
    static bool calculateChecksum(
        const char* filePath,
        uint32_t& checksum,
        StorageError* error = nullptr
    );

    /**
     * Verify file integrity using checksum
     * @param filePath File path
     * @param expectedChecksum Expected checksum value
     * @param error Optional error output parameter
     * @return true if checksum matches
     */
    static bool verifyChecksum(
        const char* filePath,
        uint32_t expectedChecksum,
        StorageError* error = nullptr
    );

    /**
     * Create backup of file/folder
     * @param srcPath Source path
     * @param backupSuffix Backup suffix (e.g., ".bak", ".backup")
     * @param error Optional error output parameter
     * @return true if successful
     */
    static bool createBackup(
        const char* srcPath,
        const char* backupSuffix = ".bak",
        StorageError* error = nullptr
    );

    /**
     * Restore from backup
     * @param backupPath Backup file path
     * @param restorePath Where to restore (nullptr = original location)
     * @param error Optional error output parameter
     * @return true if successful
     */
    static bool restoreBackup(
        const char* backupPath,
        const char* restorePath = nullptr,
        StorageError* error = nullptr
    );

    /**
     * Wipe storage securely (overwrite with zeros/random)
     * @param mountPoint Mount point to wipe
     * @param passes Number of overwrite passes (1-3 recommended)
     * @param progress Optional progress callback
     * @param error Optional error output parameter
     * @return true if successful
     */
    static bool secureWipe(
        const char* mountPoint,
        uint8_t passes = 1,
        void (*progress)(size_t, size_t) = nullptr,
        StorageError* error = nullptr
    );

    /**
     * Mount storage device
     * @param mountPoint Mount point path
     * @param devicePath Device path (implementation-specific)
     * @param fsType File system type
     * @param readOnly Mount as read-only
     * @param error Optional error output parameter
     * @return true if successful
     */
    static bool mount(
        const char* mountPoint,
        const char* devicePath,
        FilesystemType fsType = FilesystemType::AUTO,
        bool readOnly = false,
        StorageError* error = nullptr
    );

    /**
     * Unmount storage device
     * @param mountPoint Mount point to unmount
     * @param error Optional error output parameter
     * @return true if successful
     */
    static bool unmount(
        const char* mountPoint,
        StorageError* error = nullptr
    );
};
```

### Usage Example

```cpp
#include <StaticStorage.h>

void setup() {
    Serial.begin(115200);
    StorageError error;

    // Calculate and verify file checksum
    uint32_t checksum;
    if (StaticStorage::calculateChecksum("/qspi/config.txt", checksum, &error)) {
        Serial.print("File checksum: 0x");
        Serial.println(checksum, HEX);

        // Later, verify integrity
        if (StaticStorage::verifyChecksum("/qspi/config.txt", checksum, &error)) {
            Serial.println("File integrity verified!");
        } else {
            Serial.println("File may be corrupted!");
        }
    }

    // Create backup before modifying
    if (StaticStorage::createBackup("/qspi/important.dat", ".bak", &error)) {
        Serial.println("Backup created successfully");

        // Modify file...
        // If something goes wrong, restore:
        StaticStorage::restoreBackup("/qspi/important.dat.bak", nullptr, &error);
    }

    // Compare files
    if (StaticStorage::compareFiles(
        "/qspi/file1.txt",
        "/sd/file1.txt",
        &error
    )) {
        Serial.println("Files are identical");
    } else {
        Serial.println("Files differ");
    }
}
```

---

## Complete Example: Storage Manager

```cpp
#include <Arduino.h>
#include <StaticStorage.h>
#include <QSPIStorage.h>

QSPIStorage qspi;

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);

    StorageError error;

    // Initialize QSPI
    if (!qspi.begin(&error)) {
        Serial.println("QSPI init failed");
        return;
    }

    // Check if formatting needed
    if (StaticStorage::needsFormatting("/qspi", &error)) {
        Serial.println("Formatting QSPI with LittleFS...");
        StaticStorage::format("/qspi", FilesystemType::LITTLEFS, &error);
    }

    // Create partitions
    PartitionInfo partitions[] = {
        {"system", 0x000000, 1024 * 1024, FilesystemType::LITTLEFS},
        {"data",   0x100000, 6 * 1024 * 1024, FilesystemType::LITTLEFS}
    };

    StaticStorage::createPartitions("/qspi", partitions, 2, &error);

    // Get storage info
    StorageInfo info;
    if (StaticStorage::getStorageInfo("/qspi", info, &error)) {
        Serial.print("Storage: ");
        Serial.print(info.usedBytes / 1024);
        Serial.print(" / ");
        Serial.print(info.totalBytes / 1024);
        Serial.println(" KB used");
    }

    // Check health
    StorageHealth health;
    if (StaticStorage::checkHealth("/qspi", health, &error)) {
        Serial.print("Health: ");
        Serial.println(health.statusMessage);

        if (health.fragmentationPercent > 30.0) {
            Serial.println("Optimizing storage...");
            StaticStorage::optimize("/qspi", &error);
        }
    }

    Serial.println("Storage manager ready!");
}

void loop() {
    // Monitor storage health periodically
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 60000) {  // Check every minute
        lastCheck = millis();

        StorageHealth health;
        StorageError error;

        if (StaticStorage::checkHealth("/qspi", health, &error)) {
            if (!health.healthy) {
                Serial.print("WARNING: Storage health issue: ");
                Serial.println(health.statusMessage);
            }
        }
    }
}
```