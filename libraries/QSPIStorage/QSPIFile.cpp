/*
 * Copyright (c) 2024 Arduino SA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "QSPIFile.h"
#include "QSPIFolder.h"

#include <zephyr/fs/fs.h>
#include <errno.h>
#include <cstring>

QSPIFile::QSPIFile() : File(), file_(nullptr), is_open_(false), mode_(FileMode::READ) {
}

QSPIFile::QSPIFile(const char* path) : File(path), file_(nullptr), is_open_(false), mode_(FileMode::READ) {
}

QSPIFile::QSPIFile(const String& path) : File(path), file_(nullptr), is_open_(false), mode_(FileMode::READ) {
}

QSPIFile::~QSPIFile() {
    if (is_open_) {
        close(nullptr);
    }
    freeFileHandle();
}

bool QSPIFile::ensureFileHandle() {
    if (file_ == nullptr) {
        file_ = new struct fs_file_t;
        if (file_ == nullptr) {
            return false;
        }
        fs_file_t_init(file_);
    }
    return true;
}

void QSPIFile::freeFileHandle() {
    if (file_ != nullptr) {
        delete file_;
        file_ = nullptr;
    }
}

int QSPIFile::fileModeToFlags(FileMode mode) {
    switch (mode) {
        case FileMode::READ:
            return FS_O_READ;
        case FileMode::WRITE:
            return FS_O_WRITE | FS_O_CREATE;
        case FileMode::APPEND:
            return FS_O_WRITE | FS_O_CREATE | FS_O_APPEND;
        case FileMode::READ_WRITE:
            return FS_O_READ | FS_O_WRITE;
        case FileMode::READ_WRITE_CREATE:
            return FS_O_READ | FS_O_WRITE | FS_O_CREATE;
        default:
            return FS_O_READ;
    }
}

StorageErrorCode QSPIFile::mapZephyrError(int err) {
    if (err >= 0) {
        return StorageErrorCode::NONE;
    }

    switch (-err) {
        case ENOENT:
            return StorageErrorCode::FILE_NOT_FOUND;
        case EEXIST:
            return StorageErrorCode::ALREADY_EXISTS;
        case EACCES:
        case EPERM:
            return StorageErrorCode::PERMISSION_DENIED;
        case ENOSPC:
            return StorageErrorCode::STORAGE_FULL;
        case EINVAL:
            return StorageErrorCode::INVALID_PATH;
        case EIO:
            return StorageErrorCode::HARDWARE_ERROR;
        case ENOMEM:
            return StorageErrorCode::OUT_OF_MEMORY;
        default:
            return StorageErrorCode::UNKNOWN_ERROR;
    }
}

bool QSPIFile::open(const char* filename, FileMode mode, StorageError* error) {
    // Update path
    if (filename != nullptr) {
        strncpy(path_, filename, sizeof(path_) - 1);
        path_[sizeof(path_) - 1] = '\0';
    }
    return open(mode, error);
}

bool QSPIFile::open(const String& filename, FileMode mode, StorageError* error) {
    return open(filename.c_str(), mode, error);
}

bool QSPIFile::open(FileMode mode, StorageError* error) {
    if (is_open_) {
        close(nullptr);
    }

    if (path_[0] == '\0') {
        if (error) {
            error->setError(StorageErrorCode::INVALID_PATH, "No file path specified");
        }
        return false;
    }

    if (!ensureFileHandle()) {
        if (error) {
            error->setError(StorageErrorCode::OUT_OF_MEMORY, "Failed to allocate file handle");
        }
        return false;
    }

    fs_file_t_init(file_);

    int flags = fileModeToFlags(mode);
    int ret = fs_open(file_, path_, flags);

    if (ret < 0) {
        if (error) {
            error->setError(mapZephyrError(ret), "Failed to open file");
        }
        return false;
    }

    is_open_ = true;
    mode_ = mode;
    return true;
}

bool QSPIFile::close(StorageError* error) {
    if (!is_open_ || file_ == nullptr) {
        return true;  // Already closed
    }

    int ret = fs_close(file_);
    is_open_ = false;

    if (ret < 0) {
        if (error) {
            error->setError(mapZephyrError(ret), "Failed to close file");
        }
        return false;
    }

    return true;
}

bool QSPIFile::changeMode(FileMode mode, StorageError* error) {
    if (!is_open_) {
        if (error) {
            error->setError(StorageErrorCode::INVALID_OPERATION, "File not open");
        }
        return false;
    }

    // Close and reopen with new mode
    close(nullptr);
    return open(mode, error);
}

bool QSPIFile::isOpen() const {
    return is_open_;
}

size_t QSPIFile::read(uint8_t* buffer, size_t size, StorageError* error) {
    if (!is_open_ || file_ == nullptr) {
        if (error) {
            error->setError(StorageErrorCode::INVALID_OPERATION, "File not open");
        }
        return 0;
    }

    ssize_t ret = fs_read(file_, buffer, size);

    if (ret < 0) {
        if (error) {
            error->setError(mapZephyrError(ret), "Read failed");
        }
        return 0;
    }

    return static_cast<size_t>(ret);
}

int QSPIFile::read(StorageError* error) {
    uint8_t byte;
    size_t ret = read(&byte, 1, error);
    if (ret == 1) {
        return byte;
    }
    return -1;
}

String QSPIFile::readAsString(StorageError* error) {
    if (!is_open_) {
        if (error) {
            error->setError(StorageErrorCode::INVALID_OPERATION, "File not open");
        }
        return String();
    }

    // Get file size
    size_t fileSize = this->size(error);
    if (fileSize == 0) {
        return String();
    }

    // Seek to beginning
    seek(0, error);

    // Read entire file
    char* buffer = new char[fileSize + 1];
    if (buffer == nullptr) {
        if (error) {
            error->setError(StorageErrorCode::OUT_OF_MEMORY, "Failed to allocate buffer");
        }
        return String();
    }

    size_t bytesRead = read(reinterpret_cast<uint8_t*>(buffer), fileSize, error);
    buffer[bytesRead] = '\0';

    String result(buffer);
    delete[] buffer;

    return result;
}

uint32_t QSPIFile::available(StorageError* error) {
    if (!is_open_) {
        return 0;
    }

    size_t fileSize = this->size(error);
    size_t currentPos = this->position(error);

    if (fileSize > currentPos) {
        return fileSize - currentPos;
    }
    return 0;
}

bool QSPIFile::seek(size_t offset, StorageError* error) {
    if (!is_open_ || file_ == nullptr) {
        if (error) {
            error->setError(StorageErrorCode::INVALID_OPERATION, "File not open");
        }
        return false;
    }

    int ret = fs_seek(file_, offset, FS_SEEK_SET);

    if (ret < 0) {
        if (error) {
            error->setError(mapZephyrError(ret), "Seek failed");
        }
        return false;
    }

    return true;
}

size_t QSPIFile::position(StorageError* error) {
    if (!is_open_ || file_ == nullptr) {
        if (error) {
            error->setError(StorageErrorCode::INVALID_OPERATION, "File not open");
        }
        return 0;
    }

    off_t pos = fs_tell(file_);

    if (pos < 0) {
        if (error) {
            error->setError(mapZephyrError(pos), "Failed to get position");
        }
        return 0;
    }

    return static_cast<size_t>(pos);
}

size_t QSPIFile::size(StorageError* error) {
    struct fs_dirent entry;
    int ret = fs_stat(path_, &entry);

    if (ret < 0) {
        if (error) {
            error->setError(mapZephyrError(ret), "Failed to get file size");
        }
        return 0;
    }

    return entry.size;
}

size_t QSPIFile::write(const uint8_t* buffer, size_t size, StorageError* error) {
    if (!is_open_ || file_ == nullptr) {
        if (error) {
            error->setError(StorageErrorCode::INVALID_OPERATION, "File not open");
        }
        return 0;
    }

    ssize_t ret = fs_write(file_, buffer, size);

    if (ret < 0) {
        if (error) {
            error->setError(mapZephyrError(ret), "Write failed");
        }
        return 0;
    }

    return static_cast<size_t>(ret);
}

size_t QSPIFile::write(const String& data, StorageError* error) {
    return write(reinterpret_cast<const uint8_t*>(data.c_str()), data.length(), error);
}

size_t QSPIFile::write(uint8_t value, StorageError* error) {
    return write(&value, 1, error);
}

bool QSPIFile::flush(StorageError* error) {
    if (!is_open_ || file_ == nullptr) {
        if (error) {
            error->setError(StorageErrorCode::INVALID_OPERATION, "File not open");
        }
        return false;
    }

    int ret = fs_sync(file_);

    if (ret < 0) {
        if (error) {
            error->setError(mapZephyrError(ret), "Flush failed");
        }
        return false;
    }

    return true;
}

bool QSPIFile::exists(StorageError* error) const {
    if (path_[0] == '\0') {
        return false;
    }

    struct fs_dirent entry;
    int ret = fs_stat(path_, &entry);

    return (ret == 0 && entry.type == FS_DIR_ENTRY_FILE);
}

bool QSPIFile::remove(StorageError* error) {
    if (is_open_) {
        close(nullptr);
    }

    if (path_[0] == '\0') {
        if (error) {
            error->setError(StorageErrorCode::INVALID_PATH, "No file path specified");
        }
        return false;
    }

    int ret = fs_unlink(path_);

    if (ret < 0) {
        if (error) {
            error->setError(mapZephyrError(ret), "Failed to remove file");
        }
        return false;
    }

    return true;
}

bool QSPIFile::rename(const char* newFilename, StorageError* error) {
    if (is_open_) {
        close(nullptr);
    }

    if (path_[0] == '\0' || newFilename == nullptr) {
        if (error) {
            error->setError(StorageErrorCode::INVALID_PATH, "Invalid path");
        }
        return false;
    }

    int ret = fs_rename(path_, newFilename);

    if (ret < 0) {
        if (error) {
            error->setError(mapZephyrError(ret), "Failed to rename file");
        }
        return false;
    }

    // Update internal path
    strncpy(path_, newFilename, sizeof(path_) - 1);
    path_[sizeof(path_) - 1] = '\0';

    return true;
}

bool QSPIFile::rename(const String& newFilename, StorageError* error) {
    return rename(newFilename.c_str(), error);
}

QSPIFolder QSPIFile::getParentFolder(StorageError* error) const {
    if (path_[0] == '\0') {
        if (error) {
            error->setError(StorageErrorCode::INVALID_PATH, "No file path");
        }
        return QSPIFolder();
    }

    char parentPath[STORAGE_MAX_PATH_LENGTH];
    strncpy(parentPath, path_, sizeof(parentPath) - 1);
    parentPath[sizeof(parentPath) - 1] = '\0';

    // Find last separator
    char* lastSep = strrchr(parentPath, '/');
    if (lastSep != nullptr && lastSep != parentPath) {
        *lastSep = '\0';
    } else if (lastSep == parentPath) {
        // Root folder
        parentPath[1] = '\0';
    }

    return QSPIFolder(parentPath);
}
