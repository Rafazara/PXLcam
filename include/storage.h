#pragma once

#include <cstddef>

#include <esp_camera.h>

namespace pxlcam::storage {

// Persistent storage layer for captured frames and telemetry.
struct StorageConfig {
    const char *mountPoint;          // e.g. "/sdcard"
    size_t maxFileSizeBytes;         // Optional sanity guard, 0 disables the check.
    bool enableTimestampedFolders;   // Reserved for higher-level session management.
};

bool initSD(const StorageConfig &config);
bool saveFile(const char *relativePath, const uint8_t *data, size_t length);
bool saveFile(const char *relativePath, const camera_fb_t *frame);
void shutdownSD();

}  // namespace pxlcam::storage
