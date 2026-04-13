/*
 * Copyright (c) 2024 Arduino SA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef QSPI_FILE_H
#define QSPI_FILE_H

#include <Arduino.h>
#include <ArduinoStorage.h>

// Forward declarations - avoid including zephyr headers in public header
// to prevent static initialization issues
struct fs_file_t;
class QSPIFolder;

/**
 * @class QSPIFile
 * @brief File operations for QSPI flash storage.
 *
 * QSPIFile provides read, write, and seek operations for files stored on
 * QSPI flash memory. It implements the File interface from ArduinoStorage.
 *
 * @note Files are stored on the LittleFS partition mounted at /storage.
 *
 * @example SimpleReadWrite.ino
 * @code
 * QSPIFile file("/storage/data.txt");
 * file.open(FileMode::WRITE);
 * file.write("Hello World!");
 * file.close();
 *
 * file.open(FileMode::READ);
 * String content = file.readAsString();
 * file.close();
 * @endcode
 */
class QSPIFile : public File {
public:
    /**
     * @brief Default constructor.
     *
     * Creates an empty file object. Use open() with a path to access a file.
     */
    QSPIFile();

    /**
     * @brief Construct a file object with the specified path.
     * @param path Absolute path to the file (e.g., "/storage/myfile.txt")
     */
    QSPIFile(const char* path);

    /**
     * @brief Construct a file object with the specified path.
     * @param path Absolute path to the file as a String
     */
    QSPIFile(const String& path);

    /**
     * @brief Destructor. Closes the file if open.
     */
    ~QSPIFile() override;

    // ==================== Opening and Closing ====================

    /**
     * @brief Open a file at the specified path.
     * @param filename Path to the file to open
     * @param mode File mode (READ, WRITE, APPEND, READ_WRITE)
     * @param error Optional pointer to receive error details
     * @return true if successful, false otherwise
     */
    bool open(const char* filename, FileMode mode, StorageError* error = nullptr) override;

    /**
     * @brief Open a file at the specified path.
     * @param filename Path to the file as a String
     * @param mode File mode (READ, WRITE, APPEND, READ_WRITE)
     * @param error Optional pointer to receive error details
     * @return true if successful, false otherwise
     */
    bool open(const String& filename, FileMode mode, StorageError* error = nullptr) override;

    /**
     * @brief Open the file at the path set in the constructor.
     * @param mode File mode (READ, WRITE, APPEND, READ_WRITE)
     * @param error Optional pointer to receive error details
     * @return true if successful, false otherwise
     */
    bool open(FileMode mode = FileMode::READ, StorageError* error = nullptr) override;

    /**
     * @brief Close the file.
     * @param error Optional pointer to receive error details
     * @return true if successful, false otherwise
     */
    bool close(StorageError* error = nullptr) override;

    /**
     * @brief Change the file mode without closing.
     * @param mode New file mode
     * @param error Optional pointer to receive error details
     * @return true if successful, false otherwise
     */
    bool changeMode(FileMode mode, StorageError* error = nullptr) override;

    /**
     * @brief Check if the file is currently open.
     * @return true if open, false otherwise
     */
    bool isOpen() const override;

    // ==================== Reading Operations ====================

    /**
     * @brief Read bytes from the file into a buffer.
     * @param buffer Destination buffer for the data
     * @param size Maximum number of bytes to read
     * @param error Optional pointer to receive error details
     * @return Number of bytes actually read
     */
    size_t read(uint8_t* buffer, size_t size, StorageError* error = nullptr) override;

    /**
     * @brief Read a single byte from the file.
     * @param error Optional pointer to receive error details
     * @return The byte read, or -1 if end of file or error
     */
    int read(StorageError* error = nullptr) override;

    /**
     * @brief Read the entire file contents as a String.
     * @param error Optional pointer to receive error details
     * @return File contents as a String
     */
    String readAsString(StorageError* error = nullptr) override;

    /**
     * @brief Get the number of bytes available to read.
     * @param error Optional pointer to receive error details
     * @return Number of bytes from current position to end of file
     */
    uint32_t available(StorageError* error = nullptr) override;

    /**
     * @brief Seek to a position in the file.
     * @param offset Byte offset from the beginning of the file
     * @param error Optional pointer to receive error details
     * @return true if successful, false otherwise
     */
    bool seek(size_t offset, StorageError* error = nullptr) override;

    /**
     * @brief Get the current read/write position.
     * @param error Optional pointer to receive error details
     * @return Current byte offset from the beginning of the file
     */
    size_t position(StorageError* error = nullptr) override;

    /**
     * @brief Get the file size in bytes.
     * @param error Optional pointer to receive error details
     * @return File size in bytes
     */
    size_t size(StorageError* error = nullptr) override;

    // ==================== Writing Operations ====================

    /**
     * @brief Write bytes to the file.
     * @param buffer Source buffer containing data to write
     * @param size Number of bytes to write
     * @param error Optional pointer to receive error details
     * @return Number of bytes actually written
     */
    size_t write(const uint8_t* buffer, size_t size, StorageError* error = nullptr) override;

    /**
     * @brief Write a String to the file.
     * @param data String to write
     * @param error Optional pointer to receive error details
     * @return Number of bytes written
     */
    size_t write(const String& data, StorageError* error = nullptr) override;

    /**
     * @brief Write a single byte to the file.
     * @param value Byte to write
     * @param error Optional pointer to receive error details
     * @return 1 if successful, 0 otherwise
     */
    size_t write(uint8_t value, StorageError* error = nullptr) override;

    /**
     * @brief Flush any buffered data to storage.
     * @param error Optional pointer to receive error details
     * @return true if successful, false otherwise
     */
    bool flush(StorageError* error = nullptr) override;

    // ==================== File Management ====================

    /**
     * @brief Check if the file exists on storage.
     * @param error Optional pointer to receive error details
     * @return true if the file exists, false otherwise
     */
    bool exists(StorageError* error = nullptr) const override;

    /**
     * @brief Delete the file from storage.
     * @param error Optional pointer to receive error details
     * @return true if successful, false otherwise
     */
    bool remove(StorageError* error = nullptr) override;

    /**
     * @brief Rename or move the file.
     * @param newFilename New path for the file
     * @param error Optional pointer to receive error details
     * @return true if successful, false otherwise
     */
    bool rename(const char* newFilename, StorageError* error = nullptr) override;

    /**
     * @brief Rename or move the file.
     * @param newFilename New path for the file as a String
     * @param error Optional pointer to receive error details
     * @return true if successful, false otherwise
     */
    bool rename(const String& newFilename, StorageError* error = nullptr) override;

    // ==================== Path Information ====================

    /**
     * @brief Get the parent folder of this file.
     * @param error Optional pointer to receive error details
     * @return QSPIFolder representing the parent directory
     */
    QSPIFolder getParentFolder(StorageError* error = nullptr) const;

private:
    struct fs_file_t* file_;
    bool is_open_;
    FileMode mode_;

    bool resolvePath(const char* path, char* resolved, StorageError* error);
    int fileModeToFlags(FileMode mode);
    bool ensureFileHandle();
    void freeFileHandle();
    static StorageErrorCode mapZephyrError(int err);
};

#endif // QSPI_FILE_H
