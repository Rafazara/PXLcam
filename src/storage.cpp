#include "storage.h"

#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>
#include <esp_err.h>
#include <esp_log.h>

#include <string>

#include "logging.h"

namespace pxlcam::storage {

namespace {

constexpr const char *kLogTag = "pxlcam-storage";

StorageConfig g_config{};
bool g_initialized = false;

const char *resolveMountPoint(const StorageConfig &config) {
    return (config.mountPoint != nullptr && config.mountPoint[0] != '\0') ? config.mountPoint : "/sdcard";
}

std::string sanitizePath(const char *path) {
    if (path == nullptr || path[0] == '\0') {
        return {};
    }

    std::string sanitized(path);
    if (sanitized.front() != '/') {
        sanitized.insert(sanitized.begin(), '/');
    }
    return sanitized;
}

bool ensureParentDirectories(const std::string &fullPath) {
    std::size_t searchPos = 1;  // skip root slash
    while (true) {
        std::size_t slash = fullPath.find('/', searchPos);
        if (slash == std::string::npos) {
            return true;
        }

        std::string directory = fullPath.substr(0, slash);
        if (!directory.empty() && directory != "/" && !SD_MMC.exists(directory.c_str())) {
            if (!SD_MMC.mkdir(directory.c_str())) {
                PXLCAM_LOGE_TAG(kLogTag, "Failed to create directory: %s", directory.c_str());
                return false;
            }
        }

        searchPos = slash + 1;
    }
}

bool writeBufferToFile(const std::string &path, const uint8_t *data, size_t length) {
    if (!ensureParentDirectories(path)) {
        return false;
    }

    File file = SD_MMC.open(path.c_str(), FILE_WRITE);
    if (!file) {
        PXLCAM_LOGE_TAG(kLogTag, "Failed to open file for writing: %s", path.c_str());
        return false;
    }

    const size_t written = file.write(data, length);
    file.close();

    if (written != length) {
        PXLCAM_LOGE_TAG(kLogTag, "Incomplete write (%u/%u bytes) to %s", static_cast<unsigned>(written), static_cast<unsigned>(length), path.c_str());
        return false;
    }

    return true;
}

}  // namespace

bool initSD(const StorageConfig &config) {
    if (g_initialized) {
        shutdownSD();
    }

    const char *mountPoint = resolveMountPoint(config);
    constexpr bool kMode1Bit = true;               // ESP32-CAM shares pins with camera, prefer 1-bit mode.
    constexpr bool kFormatOnFail = false;          // Avoid accidental formatting; surface errors instead.
    constexpr int kDefaultFrequency = SDMMC_FREQ_DEFAULT;

    if (!SD_MMC.begin(mountPoint, kMode1Bit, kFormatOnFail, kDefaultFrequency)) {
        PXLCAM_LOGE_TAG(kLogTag, "Failed to mount SD_MMC at %s", mountPoint);
        return false;
    }

    if (SD_MMC.cardType() == CARD_NONE) {
        PXLCAM_LOGE_TAG(kLogTag, "No SD card detected on SD_MMC bus");
        SD_MMC.end();
        return false;
    }

    if (!SD_MMC.exists("/captures") && !SD_MMC.mkdir("/captures")) {
        PXLCAM_LOGE_TAG(kLogTag, "Failed to create /captures directory");
        SD_MMC.end();
        return false;
    }

    g_config = config;
    g_initialized = true;

    PXLCAM_LOGI_TAG(kLogTag, "SD_MMC mounted at %s (size=%lluMB)", mountPoint, static_cast<unsigned long long>(SD_MMC.cardSize() / (1024ULL * 1024ULL)));
    return true;
}

bool saveFile(const char *relativePath, const uint8_t *data, size_t length) {
    if (!g_initialized) {
        PXLCAM_LOGE_TAG(kLogTag, "saveFile called before initSD");
        return false;
    }

    if (relativePath == nullptr || data == nullptr || length == 0) {
        PXLCAM_LOGE_TAG(kLogTag, "Invalid arguments supplied to saveFile");
        return false;
    }

    if (g_config.maxFileSizeBytes != 0 && length > g_config.maxFileSizeBytes) {
        PXLCAM_LOGE_TAG(kLogTag, "Buffer exceeds maxFileSizeBytes (%u > %u)", static_cast<unsigned>(length), static_cast<unsigned>(g_config.maxFileSizeBytes));
        return false;
    }

    const std::string path = sanitizePath(relativePath);
    if (path.empty()) {
        PXLCAM_LOGE_TAG(kLogTag, "Could not sanitise path for saveFile");
        return false;
    }

    return writeBufferToFile(path, data, length);
}

bool saveFile(const char *relativePath, const camera_fb_t *frame) {
    if (frame == nullptr) {
        PXLCAM_LOGE_TAG(kLogTag, "Null frame passed to saveFile overload");
        return false;
    }
    return saveFile(relativePath, frame->buf, frame->len);
}

void shutdownSD() {
    if (!g_initialized) {
        return;
    }

    SD_MMC.end();
    g_initialized = false;
    g_config = StorageConfig{};

    PXLCAM_LOGI_TAG(kLogTag, "SD_MMC unmounted");
}

}  // namespace pxlcam::storage
