#pragma once

#include <Arduino.h>

// Mount LittleFS and create required directories
bool fsInit();

// Save uploaded data to UPLOAD_PATH; returns true on success
bool fsSaveUpload(const uint8_t *data, size_t len);

// Delete a file; returns true on success
bool fsDelete(const char *path);

// Returns true if the file exists
bool fsExists(const char *path);

// Returns file size in bytes, or -1 if not found
int32_t fsFileSize(const char *path);
