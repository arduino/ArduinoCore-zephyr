# Base Storage API Reference

## Overview

This document defines the base API for all Arduino storage libraries. It provides a unified interface inspired by Arduino_UnifiedStorage, with comprehensive error handling through an error object system. This API serves as a reference specification and should not be implemented directly - instead, storage implementations (QSPI, SD, Flash, etc.) should conform to these interfaces.

## Core Design Principles

1. **Unified Interface**: All storage types expose the same API surface
2. **Error Object Pattern**: Every method accepts an optional `StorageError*` parameter
3. **Path-based Operations**: Files and folders are referenced by paths
4. **Minimal Dependencies**: Standard Arduino types (String, uint8_t, etc.)
5. **Resource Safety**: Explicit open/close semantics with automatic cleanup

---

## Error Handling

### StorageError Class

The `StorageError` class provides detailed error information across all storage operations.

```cpp
enum class StorageErrorCode {
    NONE = 0,
    // File/Folder errors
    FILE_NOT_FOUND,
    FOLDER_NOT_FOUND,
    ALREADY_EXISTS,
    INVALID_PATH,
    PERMISSION_DENIED,

    // I/O errors
    READ_ERROR,
    WRITE_ERROR,
    SEEK_ERROR,
    OPEN_ERROR,
    CLOSE_ERROR,

    // Storage errors
    STORAGE_FULL,
    STORAGE_NOT_MOUNTED,
    STORAGE_CORRUPTED,
    STORAGE_NOT_FORMATTED,

    // Operation errors
    INVALID_OPERATION,
    INVALID_MODE,
    BUFFER_OVERFLOW,
    OUT_OF_MEMORY,
    TIMEOUT,

    // Hardware errors
    HARDWARE_ERROR,
    NOT_INITIALIZED,

    // Generic
    UNKNOWN_ERROR
};

class StorageError {
public:
    StorageError();

    // Error state
    StorageErrorCode getCode() const;
    const char* getMessage() const;
    bool hasError() const;

    // Error setting (for implementations)
    void setError(StorageErrorCode code, const char* message = nullptr);
    void clear();

    // Convenience operators
    operator bool() const;  // Returns true if error exists

private:
    StorageErrorCode code_;
    char message_[128];
};
```

### Error Usage Pattern

```cpp
// Example 1: Check error after operation
StorageError error;
file.open("data.txt", FileMode::READ, &error);
if (error) {
    Serial.print("Error: ");
    Serial.println(error.getMessage());
}

// Example 2: Ignore errors (backwards compatible)
file.open("data.txt", FileMode::READ);

// Example 3: Error accumulation
StorageError error;
file.open("data.txt", FileMode::WRITE, &error);
file.write(buffer, size, &error);
file.close(&error);
if (error) {
    Serial.println("Operation failed");
}
```

---

## File Modes

```cpp
enum class FileMode {
    READ,           // Open for reading, file must exist
    WRITE,          // Open for writing, creates if doesn't exist, truncates if exists
    APPEND,         // Open for writing at end, creates if doesn't exist
    READ_WRITE,     // Open for reading and writing, file must exist
    READ_WRITE_CREATE  // Open for reading and writing, creates if doesn't exist
};
```

---

## File Class

Represents a file in the storage system. Files can be read, written, and manipulated.

### Constructors

```cpp
File();
File(const char* path);
File(const String& path);
```

### Opening and Closing

```cpp
// Open file with specific mode
bool open(const char* filename, FileMode mode, StorageError* error = nullptr);
bool open(const String& filename, FileMode mode, StorageError* error = nullptr);
bool open(FileMode mode = FileMode::READ, StorageError* error = nullptr);  // Uses constructor path

// Close file and release resources
bool close(StorageError* error = nullptr);

// Change mode without closing/reopening
bool changeMode(FileMode mode, StorageError* error = nullptr);

// Check if file is currently open
bool isOpen() const;
```

### Reading Operations

```cpp
// Read data into buffer, returns bytes actually read
size_t read(uint8_t* buffer, size_t size, StorageError* error = nullptr);

// Read single byte, returns -1 on error or EOF
int read(StorageError* error = nullptr);

// Read entire file as string
String readAsString(StorageError* error = nullptr);

// Check bytes available for reading
uint32_t available(StorageError* error = nullptr);

// Position file pointer
bool seek(size_t offset, StorageError* error = nullptr);

// Get current position
size_t position(StorageError* error = nullptr);

// Get file size
size_t size(StorageError* error = nullptr);
```

### Writing Operations

```cpp
// Write buffer to file, returns bytes actually written
size_t write(const uint8_t* buffer, size_t size, StorageError* error = nullptr);

// Write string to file
size_t write(const String& data, StorageError* error = nullptr);

// Write single byte
size_t write(uint8_t value, StorageError* error = nullptr);

// Flush write buffer to storage
bool flush(StorageError* error = nullptr);
```

### File Management

```cpp
// Check if file exists
bool exists(StorageError* error = nullptr) const;

// Delete file
bool remove(StorageError* error = nullptr);

// Rename file
bool rename(const char* newFilename, StorageError* error = nullptr);
bool rename(const String& newFilename, StorageError* error = nullptr);
```

### Path Information

```cpp
// Get file path as C-string
const char* getPath() const;

// Get file path as Arduino String
String getPathAsString() const;

// Get parent folder
Folder getParentFolder(StorageError* error = nullptr) const;

// Get filename without path
String getFilename() const;
```

---

## Folder Class

Represents a folder/directory in the storage system.

### Constructors

```cpp
Folder();
Folder(const char* path);
Folder(const String& path);
```

### File Operationsf

```cpp
// Create file in this folder
File createFile(const char* filename, FileMode mode = FileMode::WRITE, StorageError* error = nullptr);
File createFile(const String& filename, FileMode mode = FileMode::WRITE, StorageError* error = nullptr);

// Get file from this folder (doesn't create)
File getFile(const char* filename, StorageError* error = nullptr);
File getFile(const String& filename, StorageError* error = nullptr);
```

### Directory Management

```cpp
// Check if folder exists
bool exists(StorageError* error = nullptr) const;

// Create this folder if it doesn't exist
bool create(StorageError* error = nullptr);

// Delete folder and all contents
bool remove(bool recursive = false, StorageError* error = nullptr);

// Rename folder
bool rename(const char* newName, StorageError* error = nullptr);
bool rename(const String& newName, StorageError* error = nullptr);
```

### Subfolder Operations

```cpp
// Create subfolder
Folder createSubfolder(const char* name, bool overwrite = false, StorageError* error = nullptr);
Folder createSubfolder(const String& name, bool overwrite = false, StorageError* error = nullptr);

// Get existing subfolder
Folder getSubfolder(const char* name, StorageError* error = nullptr);
Folder getSubfolder(const String& name, StorageError* error = nullptr);
```

### Content Enumeration

```cpp
// Get all files in this folder (non-recursive)
std::vector<File> getFiles(StorageError* error = nullptr);

// Get all subfolders (non-recursive)
std::vector<Folder> getFolders(StorageError* error = nullptr);

// Get number of files
size_t getFileCount(StorageError* error = nullptr);

// Get number of subfolders
size_t getFolderCount(StorageError* error = nullptr);
```

### Path Information

```cpp
// Get folder path as C-string
const char* getPath() const;

// Get folder path as Arduino String
String getPathAsString() const;

// Get parent folder
Folder getParentFolder(StorageError* error = nullptr) const;

// Get folder name without path
String getFolderName() const;
```

---
