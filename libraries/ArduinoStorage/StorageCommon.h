/*
 * Copyright (c) 2024 Arduino SA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ARDUINO_STORAGE_COMMON_H
#define ARDUINO_STORAGE_COMMON_H

#include <Arduino.h>

// File opening modes
enum class FileMode {
    READ,              // Open for reading, file must exist
    WRITE,             // Open for writing, creates if doesn't exist, truncates if exists
    APPEND,            // Open for writing at end, creates if doesn't exist
    READ_WRITE,        // Open for reading and writing, file must exist
    READ_WRITE_CREATE  // Open for reading and writing, creates if doesn't exist
};

// Supported filesystem types
enum class FilesystemType {
    LITTLEFS,   // LittleFS - recommended for flash storage
    FAT,        // FAT32 - better compatibility, larger overhead
    EXT2,       // Extended 2 - Linux-style filesystem
    AUTO        // Auto-detect or use default
};

// Storage information structure
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

// Storage health structure
struct StorageHealth {
    bool healthy;                    // Overall health status
    uint32_t errorCount;             // Number of errors encountered
    uint32_t badBlocks;              // Number of bad blocks (flash)
    uint32_t writeCount;             // Total write operations
    uint32_t eraseCount;             // Total erase operations
    float fragmentationPercent;      // File system fragmentation
    char statusMessage[128];         // Human-readable status
};

// Partition information
struct PartitionInfo {
    const char* label;      // Partition name/label
    size_t offset;          // Start offset in bytes
    size_t size;            // Size in bytes
    FilesystemType fsType;  // File system type for this partition
};

// Maximum path length
constexpr size_t STORAGE_MAX_PATH_LENGTH = 256;

#endif // ARDUINO_STORAGE_COMMON_H
