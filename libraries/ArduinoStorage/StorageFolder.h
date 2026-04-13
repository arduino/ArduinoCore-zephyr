/*
 * Copyright (c) 2024 Arduino SA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ARDUINO_STORAGE_FOLDER_H
#define ARDUINO_STORAGE_FOLDER_H

#include <Arduino.h>
#include <cstring>
#include <vector>
#include "StorageCommon.h"
#include "StorageError.h"
#include "StorageFile.h"

/**
 * @brief Abstract base class for folder/directory operations in Arduino storage implementations.
 *
 * This class defines the interface for all folder operations. Concrete implementations
 * (QSPIFolder, SDFolder, etc.) should inherit from this class and implement all
 * pure virtual methods.
 */
class Folder {
public:
    Folder() {
        path_[0] = '\0';
    }

    Folder(const char* path) {
        if (path != nullptr) {
            strncpy(path_, path, sizeof(path_) - 1);
            path_[sizeof(path_) - 1] = '\0';
        } else {
            path_[0] = '\0';
        }
    }

    Folder(const String& path) {
        strncpy(path_, path.c_str(), sizeof(path_) - 1);
        path_[sizeof(path_) - 1] = '\0';
    }

    virtual ~Folder() {}

    // Directory Management
    virtual bool exists(StorageError* error = nullptr) const = 0;
    virtual bool create(StorageError* error = nullptr) = 0;
    virtual bool remove(bool recursive = false, StorageError* error = nullptr) = 0;
    virtual bool rename(const char* newName, StorageError* error = nullptr) = 0;
    virtual bool rename(const String& newName, StorageError* error = nullptr) = 0;

    // Content Enumeration
    virtual size_t getFileCount(StorageError* error = nullptr) = 0;
    virtual size_t getFolderCount(StorageError* error = nullptr) = 0;

    // Path Information
    virtual const char* getPath() const {
        return path_;
    }

    virtual String getPathAsString() const {
        return String(path_);
    }

    virtual String getFolderName() const {
        const char* lastSep = strrchr(path_, '/');
        if (lastSep != nullptr && *(lastSep + 1) != '\0') {
            return String(lastSep + 1);
        }
        // Handle root path or trailing slash
        if (path_[0] == '/' && path_[1] == '\0') {
            return String("/");
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

#endif // ARDUINO_STORAGE_FOLDER_H
