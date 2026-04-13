/*
 * Copyright (c) 2024 Arduino SA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "QSPIFolder.h"

#include <zephyr/fs/fs.h>
#include <errno.h>
#include <cstring>

QSPIFolder::QSPIFolder() : Folder() {
}

QSPIFolder::QSPIFolder(const char* path) : Folder(path) {
}

QSPIFolder::QSPIFolder(const String& path) : Folder(path) {
}

QSPIFolder::~QSPIFolder() {
}

StorageErrorCode QSPIFolder::mapZephyrError(int err) {
    if (err >= 0) {
        return StorageErrorCode::NONE;
    }

    switch (-err) {
        case ENOENT:
            return StorageErrorCode::FOLDER_NOT_FOUND;
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
        case ENOTEMPTY:
            return StorageErrorCode::INVALID_OPERATION;
        default:
            return StorageErrorCode::UNKNOWN_ERROR;
    }
}

bool QSPIFolder::exists(StorageError* error) const {
    if (path_[0] == '\0') {
        return false;
    }

    struct fs_dirent entry;
    int ret = fs_stat(path_, &entry);

    return (ret == 0 && entry.type == FS_DIR_ENTRY_DIR);
}

bool QSPIFolder::create(StorageError* error) {
    if (path_[0] == '\0') {
        if (error) {
            error->setError(StorageErrorCode::INVALID_PATH, "No folder path specified");
        }
        return false;
    }

    int ret = fs_mkdir(path_);

    if (ret < 0 && ret != -EEXIST) {
        if (error) {
            error->setError(mapZephyrError(ret), "Failed to create folder");
        }
        return false;
    }

    return true;
}

bool QSPIFolder::remove(bool recursive, StorageError* error) {
    if (path_[0] == '\0') {
        if (error) {
            error->setError(StorageErrorCode::INVALID_PATH, "No folder path specified");
        }
        return false;
    }

    if (recursive) {
        return removeRecursive(path_, error);
    }

    int ret = fs_unlink(path_);

    if (ret < 0) {
        if (error) {
            error->setError(mapZephyrError(ret), "Failed to remove folder");
        }
        return false;
    }

    return true;
}

bool QSPIFolder::removeRecursive(const char* path, StorageError* error) {
    struct fs_dir_t dir;
    fs_dir_t_init(&dir);

    int ret = fs_opendir(&dir, path);
    if (ret < 0) {
        if (error) {
            error->setError(mapZephyrError(ret), "Failed to open directory");
        }
        return false;
    }

    struct fs_dirent entry;
    char childPath[STORAGE_MAX_PATH_LENGTH];

    while (true) {
        ret = fs_readdir(&dir, &entry);
        if (ret < 0 || entry.name[0] == '\0') {
            break;
        }

        // Build full path
        snprintf(childPath, sizeof(childPath), "%s/%s", path, entry.name);

        if (entry.type == FS_DIR_ENTRY_DIR) {
            // Recursively remove subdirectory
            if (!removeRecursive(childPath, error)) {
                fs_closedir(&dir);
                return false;
            }
        } else {
            // Remove file
            ret = fs_unlink(childPath);
            if (ret < 0) {
                fs_closedir(&dir);
                if (error) {
                    error->setError(mapZephyrError(ret), "Failed to remove file");
                }
                return false;
            }
        }
    }

    fs_closedir(&dir);

    // Now remove the empty directory
    ret = fs_unlink(path);
    if (ret < 0) {
        if (error) {
            error->setError(mapZephyrError(ret), "Failed to remove directory");
        }
        return false;
    }

    return true;
}

bool QSPIFolder::rename(const char* newName, StorageError* error) {
    if (path_[0] == '\0' || newName == nullptr) {
        if (error) {
            error->setError(StorageErrorCode::INVALID_PATH, "Invalid path");
        }
        return false;
    }

    int ret = fs_rename(path_, newName);

    if (ret < 0) {
        if (error) {
            error->setError(mapZephyrError(ret), "Failed to rename folder");
        }
        return false;
    }

    // Update internal path
    strncpy(path_, newName, sizeof(path_) - 1);
    path_[sizeof(path_) - 1] = '\0';

    return true;
}

bool QSPIFolder::rename(const String& newName, StorageError* error) {
    return rename(newName.c_str(), error);
}

QSPIFile QSPIFolder::createFile(const char* filename, FileMode mode, StorageError* error) {
    char fullPath[STORAGE_MAX_PATH_LENGTH];
    snprintf(fullPath, sizeof(fullPath), "%s/%s", path_, filename);

    QSPIFile file(fullPath);
    if (!file.open(mode, error)) {
        return QSPIFile();
    }

    return file;
}

QSPIFile QSPIFolder::createFile(const String& filename, FileMode mode, StorageError* error) {
    return createFile(filename.c_str(), mode, error);
}

QSPIFile QSPIFolder::getFile(const char* filename, StorageError* error) {
    char fullPath[STORAGE_MAX_PATH_LENGTH];
    snprintf(fullPath, sizeof(fullPath), "%s/%s", path_, filename);

    QSPIFile file(fullPath);

    if (!file.exists(error)) {
        if (error) {
            error->setError(StorageErrorCode::FILE_NOT_FOUND, "File not found");
        }
        return QSPIFile();
    }

    return file;
}

QSPIFile QSPIFolder::getFile(const String& filename, StorageError* error) {
    return getFile(filename.c_str(), error);
}

QSPIFolder QSPIFolder::createSubfolder(const char* name, bool overwrite, StorageError* error) {
    char fullPath[STORAGE_MAX_PATH_LENGTH];
    snprintf(fullPath, sizeof(fullPath), "%s/%s", path_, name);

    QSPIFolder folder(fullPath);

    if (folder.exists(nullptr)) {
        if (overwrite) {
            if (!folder.remove(true, error)) {
                return QSPIFolder();
            }
        } else {
            if (error) {
                error->setError(StorageErrorCode::ALREADY_EXISTS, "Folder already exists");
            }
            return folder;  // Return existing folder
        }
    }

    if (!folder.create(error)) {
        return QSPIFolder();
    }

    return folder;
}

QSPIFolder QSPIFolder::createSubfolder(const String& name, bool overwrite, StorageError* error) {
    return createSubfolder(name.c_str(), overwrite, error);
}

QSPIFolder QSPIFolder::getSubfolder(const char* name, StorageError* error) {
    char fullPath[STORAGE_MAX_PATH_LENGTH];
    snprintf(fullPath, sizeof(fullPath), "%s/%s", path_, name);

    QSPIFolder folder(fullPath);

    if (!folder.exists(error)) {
        if (error) {
            error->setError(StorageErrorCode::FOLDER_NOT_FOUND, "Folder not found");
        }
        return QSPIFolder();
    }

    return folder;
}

QSPIFolder QSPIFolder::getSubfolder(const String& name, StorageError* error) {
    return getSubfolder(name.c_str(), error);
}

std::vector<QSPIFile> QSPIFolder::getFiles(StorageError* error) {
    std::vector<QSPIFile> files;

    if (path_[0] == '\0') {
        if (error) {
            error->setError(StorageErrorCode::INVALID_PATH, "No folder path");
        }
        return files;
    }

    struct fs_dir_t dir;
    fs_dir_t_init(&dir);

    int ret = fs_opendir(&dir, path_);
    if (ret < 0) {
        if (error) {
            error->setError(mapZephyrError(ret), "Failed to open directory");
        }
        return files;
    }

    struct fs_dirent entry;
    char fullPath[STORAGE_MAX_PATH_LENGTH];

    while (true) {
        ret = fs_readdir(&dir, &entry);
        if (ret < 0 || entry.name[0] == '\0') {
            break;
        }

        if (entry.type == FS_DIR_ENTRY_FILE) {
            snprintf(fullPath, sizeof(fullPath), "%s/%s", path_, entry.name);
            files.push_back(QSPIFile(fullPath));
        }
    }

    fs_closedir(&dir);
    return files;
}

std::vector<QSPIFolder> QSPIFolder::getFolders(StorageError* error) {
    std::vector<QSPIFolder> folders;

    if (path_[0] == '\0') {
        if (error) {
            error->setError(StorageErrorCode::INVALID_PATH, "No folder path");
        }
        return folders;
    }

    struct fs_dir_t dir;
    fs_dir_t_init(&dir);

    int ret = fs_opendir(&dir, path_);
    if (ret < 0) {
        if (error) {
            error->setError(mapZephyrError(ret), "Failed to open directory");
        }
        return folders;
    }

    struct fs_dirent entry;
    char fullPath[STORAGE_MAX_PATH_LENGTH];

    while (true) {
        ret = fs_readdir(&dir, &entry);
        if (ret < 0 || entry.name[0] == '\0') {
            break;
        }

        if (entry.type == FS_DIR_ENTRY_DIR) {
            snprintf(fullPath, sizeof(fullPath), "%s/%s", path_, entry.name);
            folders.push_back(QSPIFolder(fullPath));
        }
    }

    fs_closedir(&dir);
    return folders;
}

size_t QSPIFolder::getFileCount(StorageError* error) {
    size_t count = 0;

    if (path_[0] == '\0') {
        return 0;
    }

    struct fs_dir_t dir;
    fs_dir_t_init(&dir);

    int ret = fs_opendir(&dir, path_);
    if (ret < 0) {
        if (error) {
            error->setError(mapZephyrError(ret), "Failed to open directory");
        }
        return 0;
    }

    struct fs_dirent entry;

    while (true) {
        ret = fs_readdir(&dir, &entry);
        if (ret < 0 || entry.name[0] == '\0') {
            break;
        }

        if (entry.type == FS_DIR_ENTRY_FILE) {
            count++;
        }
    }

    fs_closedir(&dir);
    return count;
}

size_t QSPIFolder::getFolderCount(StorageError* error) {
    size_t count = 0;

    if (path_[0] == '\0') {
        return 0;
    }

    struct fs_dir_t dir;
    fs_dir_t_init(&dir);

    int ret = fs_opendir(&dir, path_);
    if (ret < 0) {
        if (error) {
            error->setError(mapZephyrError(ret), "Failed to open directory");
        }
        return 0;
    }

    struct fs_dirent entry;

    while (true) {
        ret = fs_readdir(&dir, &entry);
        if (ret < 0 || entry.name[0] == '\0') {
            break;
        }

        if (entry.type == FS_DIR_ENTRY_DIR) {
            count++;
        }
    }

    fs_closedir(&dir);
    return count;
}

QSPIFolder QSPIFolder::getParentFolder(StorageError* error) const {
    if (path_[0] == '\0') {
        if (error) {
            error->setError(StorageErrorCode::INVALID_PATH, "No folder path");
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

bool QSPIFolder::resolvePath(const char* path, char* resolved, StorageError* error) {
    if (path == nullptr || resolved == nullptr) {
        if (error) {
            error->setError(StorageErrorCode::INVALID_PATH, "Invalid path pointer");
        }
        return false;
    }

    // If path is absolute, use it directly
    if (path[0] == '/') {
        strncpy(resolved, path, STORAGE_MAX_PATH_LENGTH - 1);
        resolved[STORAGE_MAX_PATH_LENGTH - 1] = '\0';
    } else {
        // Relative path - combine with current folder path
        snprintf(resolved, STORAGE_MAX_PATH_LENGTH, "%s/%s", path_, path);
    }

    return true;
}
