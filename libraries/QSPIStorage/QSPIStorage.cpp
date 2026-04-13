/*
 * Copyright (c) 2024 Arduino SA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "QSPIStorage.h"

#include <zephyr/fs/fs.h>
#include <errno.h>
#include <cstring>

bool QSPIStorage::begin(StorageError* error) {
    if (mounted_) {
        return true;
    }

    // Check if the filesystem is already mounted (via devicetree FSTAB auto-mount)
    struct fs_statvfs stat;
    int ret = fs_statvfs("/storage", &stat);

    if (ret == 0) {
        mounted_ = true;
        return true;
    }

    // Filesystem not mounted - provide helpful error message
    if (error) {
        if (ret == -ENOENT) {
            error->setError(StorageErrorCode::STORAGE_NOT_MOUNTED,
                "Filesystem not mounted. Ensure LittleFS FSTAB is configured in devicetree.");
        } else {
            error->setError(StorageErrorCode::UNKNOWN_ERROR, "Failed to access filesystem");
        }
    }
    return false;
}

bool QSPIStorage::getStorageInfo(size_t& total, size_t& used, size_t& available, StorageError* error) {
    if (!mounted_) {
        if (error) {
            error->setError(StorageErrorCode::STORAGE_NOT_MOUNTED, "Storage not mounted");
        }
        return false;
    }

    struct fs_statvfs stat;
    int ret = fs_statvfs("/storage", &stat);
    if (ret != 0) {
        if (error) {
            error->setError(StorageErrorCode::READ_ERROR, "Failed to get storage info");
        }
        return false;
    }

    total = stat.f_frsize * stat.f_blocks;
    available = stat.f_frsize * stat.f_bfree;
    used = total - available;

    return true;
}

QSPIFolder QSPIStorage::getRootFolder(StorageError* error) {
    if (!mounted_) {
        if (error) {
            error->setError(StorageErrorCode::STORAGE_NOT_MOUNTED, "Storage not mounted");
        }
        return QSPIFolder();
    }
    return QSPIFolder("/storage");
}

// ========== Static Mount Info Methods ==========

int QSPIStorage::getMountCount() {
    int count = 0;
    int idx = 0;
    const char *mnt_point;

    while (fs_readmount(&idx, &mnt_point) >= 0) {
        count++;
    }

    return count;
}

bool QSPIStorage::getMountInfo(int index, QSPIMountInfo& info) {
    int idx = 0;
    int current = 0;
    const char *mnt_point;

    while (fs_readmount(&idx, &mnt_point) >= 0) {
        if (current == index) {
            info.mountPoint = mnt_point;
            // FAT mount points end with ':'
            size_t len = strlen(mnt_point);
            info.isFAT = (len > 0 && mnt_point[len - 1] == ':');
            return true;
        }
        current++;
    }

    return false;
}

void QSPIStorage::listMounts() {
    int idx = 0;
    const char *mnt_point;
    bool found = false;

    while (fs_readmount(&idx, &mnt_point) >= 0) {
        Serial.print("Mount point ");
        Serial.print(idx - 1);
        Serial.print(": ");
        Serial.print(mnt_point);

        // Detect filesystem type by mount point naming convention
        size_t len = strlen(mnt_point);
        if (len > 0 && mnt_point[len - 1] == ':') {
            Serial.print(" (FAT)");
        } else {
            Serial.print(" (LittleFS)");
        }
        Serial.println();

        found = true;
    }

    if (!found) {
        Serial.println("No mounted filesystems found!");
    }
}

void QSPIStorage::listDirectory(const char* path) {
    struct fs_dir_t dir;
    fs_dir_t_init(&dir);

    int ret = fs_opendir(&dir, path);
    if (ret < 0) {
        Serial.print("Error opening directory ");
        Serial.print(path);
        Serial.print(" [error: ");
        Serial.print(ret);
        Serial.println("]");
        return;
    }

    Serial.print("\nContents of ");
    Serial.print(path);
    Serial.println(":");

    struct fs_dirent entry;
    bool empty = true;

    while (true) {
        ret = fs_readdir(&dir, &entry);
        if (ret < 0 || entry.name[0] == '\0') {
            break;
        }

        empty = false;
        if (entry.type == FS_DIR_ENTRY_FILE) {
            Serial.print("  [FILE] ");
            Serial.print(entry.name);
            Serial.print(" (");
            Serial.print((size_t)entry.size);
            Serial.println(" bytes)");
        } else if (entry.type == FS_DIR_ENTRY_DIR) {
            Serial.print("  [DIR ] ");
            Serial.println(entry.name);
        }
    }

    if (empty) {
        Serial.println("  <empty>");
    }

    fs_closedir(&dir);
}

void QSPIStorage::listAllMounts() {
    int idx = 0;
    const char *mnt_point;

    while (fs_readmount(&idx, &mnt_point) >= 0) {
        listDirectory(mnt_point);
    }
}
