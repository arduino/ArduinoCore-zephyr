/*
 * Copyright (c) 2024 Arduino SA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ARDUINO_STORAGE_FILE_H
#define ARDUINO_STORAGE_FILE_H

#include <Arduino.h>
#include <cstring>
#include "StorageCommon.h"
#include "StorageError.h"

// Forward declaration
class Folder;

/**
 * @brief Abstract base class for file operations in Arduino storage implementations.
 *
 * This class defines the interface for all file operations. Concrete implementations
 * (QSPIFile, SDFile, etc.) should inherit from this class and implement all
 * pure virtual methods.
 */
class File {
public:
    File() {
        path_[0] = '\0';
    }

    File(const char* path) {
        if (path != nullptr) {
            strncpy(path_, path, sizeof(path_) - 1);
            path_[sizeof(path_) - 1] = '\0';
        } else {
            path_[0] = '\0';
        }
    }

    File(const String& path) {
        strncpy(path_, path.c_str(), sizeof(path_) - 1);
        path_[sizeof(path_) - 1] = '\0';
    }

    virtual ~File() {}

    // Opening and Closing
    virtual bool open(const char* filename, FileMode mode, StorageError* error = nullptr) = 0;
    virtual bool open(const String& filename, FileMode mode, StorageError* error = nullptr) = 0;
    virtual bool open(FileMode mode = FileMode::READ, StorageError* error = nullptr) = 0;
    virtual bool close(StorageError* error = nullptr) = 0;
    virtual bool changeMode(FileMode mode, StorageError* error = nullptr) = 0;
    virtual bool isOpen() const = 0;

    // Reading Operations
    virtual size_t read(uint8_t* buffer, size_t size, StorageError* error = nullptr) = 0;
    virtual int read(StorageError* error = nullptr) = 0;
    virtual String readAsString(StorageError* error = nullptr) = 0;
    virtual uint32_t available(StorageError* error = nullptr) = 0;
    virtual bool seek(size_t offset, StorageError* error = nullptr) = 0;
    virtual size_t position(StorageError* error = nullptr) = 0;
    virtual size_t size(StorageError* error = nullptr) = 0;

    // Writing Operations
    virtual size_t write(const uint8_t* buffer, size_t size, StorageError* error = nullptr) = 0;
    virtual size_t write(const String& data, StorageError* error = nullptr) = 0;
    virtual size_t write(uint8_t value, StorageError* error = nullptr) = 0;
    virtual bool flush(StorageError* error = nullptr) = 0;

    // File Management
    virtual bool exists(StorageError* error = nullptr) const = 0;
    virtual bool remove(StorageError* error = nullptr) = 0;
    virtual bool rename(const char* newFilename, StorageError* error = nullptr) = 0;
    virtual bool rename(const String& newFilename, StorageError* error = nullptr) = 0;

    // Path Information
    virtual const char* getPath() const {
        return path_;
    }

    virtual String getPathAsString() const {
        return String(path_);
    }

    virtual String getFilename() const {
        const char* lastSep = strrchr(path_, '/');
        if (lastSep != nullptr) {
            return String(lastSep + 1);
        }
        return String(path_);
    }

protected:
    char path_[STORAGE_MAX_PATH_LENGTH];

    // Helper to set error if pointer is not null
    static void setErrorIfNotNull(StorageError* error, StorageErrorCode code, const char* message = nullptr) {
        if (error != nullptr) {
            error->setError(code, message);
        }
    }
};

#endif // ARDUINO_STORAGE_FILE_H
