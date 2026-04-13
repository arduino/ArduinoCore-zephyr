/*
 * Copyright (c) 2024 Arduino SA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef QSPI_STORAGE_H
#define QSPI_STORAGE_H

#include <Arduino.h>
#include <ArduinoStorage.h>
#include "QSPIFolder.h"

/**
 * @struct QSPIMountInfo
 * @brief Information about a mounted filesystem.
 */
struct QSPIMountInfo {
    const char* mountPoint;  ///< Mount point path (e.g., "/storage")
    bool isFAT;              ///< true if FAT filesystem, false if LittleFS
};

/**
 * @class QSPIStorage
 * @brief Main interface for QSPI flash storage access.
 *
 * QSPIStorage provides high-level access to QSPI flash storage with LittleFS
 * filesystem. Use this class to initialize storage, get storage statistics,
 * and access the root folder for file operations.
 *
 * The filesystem is automatically mounted at boot via devicetree FSTAB
 * configuration. This library verifies the mount and provides access.
 *
 * @note Requires LittleFS auto-mount via devicetree FSTAB at "/storage".
 *
 * @example SimpleReadWrite.ino
 * @code
 * #include <QSPIStorage.h>
 *
 * QSPIStorage storage;
 *
 * void setup() {
 *     if (!storage.begin()) {
 *         Serial.println("Storage init failed!");
 *         return;
 *     }
 *
 *     QSPIFile file("/storage/test.txt");
 *     file.open(FileMode::WRITE);
 *     file.write("Hello QSPI!");
 *     file.close();
 * }
 * @endcode
 */
class QSPIStorage {
public:
    /**
     * @brief Default constructor.
     */
    QSPIStorage() : mounted_(false) {}

    /**
     * @brief Destructor.
     */
    ~QSPIStorage() {}

    // ==================== Initialization ====================

    /**
     * @brief Initialize and verify the QSPI filesystem is mounted.
     *
     * Checks that LittleFS is mounted at /storage. The filesystem
     * is auto-mounted by Zephyr via devicetree FSTAB configuration.
     *
     * @param error Optional pointer to receive error details
     * @return true if storage is ready, false otherwise
     */
    bool begin(StorageError* error = nullptr);

    /**
     * @brief Mark storage as not in use.
     *
     * Does not unmount the filesystem (it remains mounted by the OS).
     *
     * @param error Optional pointer to receive error details
     */
    void end(StorageError* error = nullptr) { mounted_ = false; }

    /**
     * @brief Check if storage is initialized and ready.
     * @return true if mounted and ready for use
     */
    bool isMounted() const { return mounted_; }

    /**
     * @brief Get the mount point path.
     * @return Mount point string "/storage"
     */
    const char* getMountPoint() const { return "/storage"; }

    // ==================== Storage Information ====================

    /**
     * @brief Get storage space statistics.
     *
     * @param total Output: total storage capacity in bytes
     * @param used Output: used storage space in bytes
     * @param available Output: available storage space in bytes
     * @param error Optional pointer to receive error details
     * @return true if successful, false otherwise
     *
     * @code
     * size_t total, used, available;
     * if (storage.getStorageInfo(total, used, available)) {
     *     Serial.print("Used: ");
     *     Serial.print(used / 1024);
     *     Serial.println(" KB");
     * }
     * @endcode
     */
    bool getStorageInfo(size_t& total, size_t& used, size_t& available, StorageError* error = nullptr);

    /**
     * @brief Get the root folder of the storage.
     *
     * Returns a QSPIFolder object representing /storage, which can be
     * used to create files, subfolders, and enumerate contents.
     *
     * @param error Optional pointer to receive error details
     * @return QSPIFolder representing the root directory
     *
     * @code
     * QSPIFolder root = storage.getRootFolder();
     * QSPIFolder data = root.createSubfolder("data");
     * @endcode
     */
    QSPIFolder getRootFolder(StorageError* error = nullptr);

    // ==================== Static Mount Utilities ====================

    /**
     * @brief Get the number of mounted filesystems.
     *
     * Includes all filesystems mounted by the OS (LittleFS, FAT, etc.).
     *
     * @return Number of mounted filesystems
     */
    static int getMountCount();

    /**
     * @brief Get mount point information by index.
     * @param index Mount index (0-based)
     * @param info Output structure to receive mount information
     * @return true if mount exists at index, false otherwise
     */
    static bool getMountInfo(int index, QSPIMountInfo& info);

    /**
     * @brief Print all mounted filesystems to Serial.
     *
     * Useful for debugging mount configuration.
     */
    static void listMounts();

    /**
     * @brief Print directory contents to Serial.
     *
     * Lists all files and folders at the specified path with sizes.
     *
     * @param path Directory path to list (e.g., "/storage")
     */
    static void listDirectory(const char* path);

    /**
     * @brief Print contents of all mounted filesystems to Serial.
     *
     * Calls listDirectory() for each mounted filesystem.
     */
    static void listAllMounts();

private:
    bool mounted_;
};

#endif // QSPI_STORAGE_H
