/*
 * Copyright (c) 2024 Arduino SA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef QSPI_FOLDER_H
#define QSPI_FOLDER_H

#include <Arduino.h>
#include <ArduinoStorage.h>
#include <vector>

// Note: zephyr/fs/fs.h is only included in the .cpp file
// to avoid static initialization issues

#include "QSPIFile.h"

/**
 * @class QSPIFolder
 * @brief Folder/directory operations for QSPI flash storage.
 *
 * QSPIFolder provides directory management operations including creating,
 * listing, and removing folders on QSPI flash memory. It implements the
 * Folder interface from ArduinoStorage.
 *
 * @note Folders are created on the LittleFS partition mounted at /storage.
 *
 * @example BasicFolderOperations.ino
 * @code
 * QSPIFolder root = storage.getRootFolder();
 * QSPIFolder myFolder = root.createSubfolder("mydata");
 *
 * QSPIFile file = myFolder.createFile("log.txt", FileMode::WRITE);
 * file.write("Hello!");
 * file.close();
 *
 * std::vector<QSPIFile> files = myFolder.getFiles();
 * @endcode
 */
class QSPIFolder : public Folder {
public:
    /**
     * @brief Default constructor.
     *
     * Creates an empty folder object.
     */
    QSPIFolder();

    /**
     * @brief Construct a folder object with the specified path.
     * @param path Absolute path to the folder (e.g., "/storage/myfolder")
     */
    QSPIFolder(const char* path);

    /**
     * @brief Construct a folder object with the specified path.
     * @param path Absolute path to the folder as a String
     */
    QSPIFolder(const String& path);

    /**
     * @brief Destructor.
     */
    ~QSPIFolder() override;

    // ==================== File Operations ====================

    /**
     * @brief Create a new file in this folder.
     * @param filename Name of the file to create
     * @param mode File mode for the new file (default: WRITE)
     * @param error Optional pointer to receive error details
     * @return QSPIFile object for the created file (already open)
     */
    QSPIFile createFile(const char* filename, FileMode mode = FileMode::WRITE, StorageError* error = nullptr);

    /**
     * @brief Create a new file in this folder.
     * @param filename Name of the file as a String
     * @param mode File mode for the new file (default: WRITE)
     * @param error Optional pointer to receive error details
     * @return QSPIFile object for the created file (already open)
     */
    QSPIFile createFile(const String& filename, FileMode mode = FileMode::WRITE, StorageError* error = nullptr);

    /**
     * @brief Get a file object for an existing file in this folder.
     * @param filename Name of the file
     * @param error Optional pointer to receive error details
     * @return QSPIFile object (not opened, use open() to access)
     */
    QSPIFile getFile(const char* filename, StorageError* error = nullptr);

    /**
     * @brief Get a file object for an existing file in this folder.
     * @param filename Name of the file as a String
     * @param error Optional pointer to receive error details
     * @return QSPIFile object (not opened, use open() to access)
     */
    QSPIFile getFile(const String& filename, StorageError* error = nullptr);

    // ==================== Directory Management ====================

    /**
     * @brief Check if the folder exists on storage.
     * @param error Optional pointer to receive error details
     * @return true if the folder exists, false otherwise
     */
    bool exists(StorageError* error = nullptr) const override;

    /**
     * @brief Create this folder on storage.
     * @param error Optional pointer to receive error details
     * @return true if successful, false otherwise
     */
    bool create(StorageError* error = nullptr) override;

    /**
     * @brief Remove this folder from storage.
     * @param recursive If true, delete all contents first; if false, folder must be empty
     * @param error Optional pointer to receive error details
     * @return true if successful, false otherwise
     */
    bool remove(bool recursive = false, StorageError* error = nullptr) override;

    /**
     * @brief Rename or move the folder.
     * @param newName New name or path for the folder
     * @param error Optional pointer to receive error details
     * @return true if successful, false otherwise
     */
    bool rename(const char* newName, StorageError* error = nullptr) override;

    /**
     * @brief Rename or move the folder.
     * @param newName New name or path as a String
     * @param error Optional pointer to receive error details
     * @return true if successful, false otherwise
     */
    bool rename(const String& newName, StorageError* error = nullptr) override;

    // ==================== Subfolder Operations ====================

    /**
     * @brief Create a subfolder within this folder.
     * @param name Name of the subfolder to create
     * @param overwrite If true, remove existing folder first
     * @param error Optional pointer to receive error details
     * @return QSPIFolder object for the created subfolder
     */
    QSPIFolder createSubfolder(const char* name, bool overwrite = false, StorageError* error = nullptr);

    /**
     * @brief Create a subfolder within this folder.
     * @param name Name of the subfolder as a String
     * @param overwrite If true, remove existing folder first
     * @param error Optional pointer to receive error details
     * @return QSPIFolder object for the created subfolder
     */
    QSPIFolder createSubfolder(const String& name, bool overwrite = false, StorageError* error = nullptr);

    /**
     * @brief Get a subfolder object for an existing subfolder.
     * @param name Name of the subfolder
     * @param error Optional pointer to receive error details
     * @return QSPIFolder object for the subfolder
     */
    QSPIFolder getSubfolder(const char* name, StorageError* error = nullptr);

    /**
     * @brief Get a subfolder object for an existing subfolder.
     * @param name Name of the subfolder as a String
     * @param error Optional pointer to receive error details
     * @return QSPIFolder object for the subfolder
     */
    QSPIFolder getSubfolder(const String& name, StorageError* error = nullptr);

    // ==================== Content Enumeration ====================

    /**
     * @brief Get a list of all files in this folder.
     * @param error Optional pointer to receive error details
     * @return Vector of QSPIFile objects
     */
    std::vector<QSPIFile> getFiles(StorageError* error = nullptr);

    /**
     * @brief Get a list of all subfolders in this folder.
     * @param error Optional pointer to receive error details
     * @return Vector of QSPIFolder objects
     */
    std::vector<QSPIFolder> getFolders(StorageError* error = nullptr);

    /**
     * @brief Get the number of files in this folder.
     * @param error Optional pointer to receive error details
     * @return Number of files (not including subfolders)
     */
    size_t getFileCount(StorageError* error = nullptr) override;

    /**
     * @brief Get the number of subfolders in this folder.
     * @param error Optional pointer to receive error details
     * @return Number of subfolders
     */
    size_t getFolderCount(StorageError* error = nullptr) override;

    // ==================== Path Information ====================

    /**
     * @brief Get the parent folder of this folder.
     * @param error Optional pointer to receive error details
     * @return QSPIFolder representing the parent directory
     */
    QSPIFolder getParentFolder(StorageError* error = nullptr) const;

private:
    bool resolvePath(const char* path, char* resolved, StorageError* error);
    bool removeRecursive(const char* path, StorageError* error);
    static StorageErrorCode mapZephyrError(int err);
};

#endif // QSPI_FOLDER_H
