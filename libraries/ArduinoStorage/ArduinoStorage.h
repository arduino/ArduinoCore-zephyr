/*
 * Copyright (c) 2024 Arduino SA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ARDUINO_STORAGE_H
#define ARDUINO_STORAGE_H

/**
 * @brief Arduino Storage Library
 *
 * This library provides a unified interface for all Arduino storage implementations.
 * It defines the base classes and error handling that all storage types (QSPI, SD, Flash, etc.)
 * conform to.
 *
 * Include this header to get access to:
 * - StorageError: Error handling class
 * - File: Abstract base class for file operations
 * - Folder: Abstract base class for folder operations
 * - FileMode: Enum for file opening modes
 * - FilesystemType: Enum for filesystem types
 *
 * For specific storage implementations, include their respective headers:
 * - QSPIStorage.h: QSPI flash storage
 * - SDStorage.h: SD card storage
 * - FlashStorage.h: Internal flash storage
 */

#include "StorageCommon.h"
#include "StorageError.h"
#include "StorageFile.h"
#include "StorageFolder.h"

#endif // ARDUINO_STORAGE_H
