/**
 * @file mode_manager.cpp
 * @brief Centralized mode management implementation for PXLcam v1.2.0
 */

#include "mode_manager.h"
#include "nvs_store.h"
#include "logging.h"

namespace pxlcam::mode {

namespace {

constexpr const char* kLogTag = "pxlcam-mode";

CaptureMode g_currentMode = CaptureMode::Normal;
ModeChangeCallback g_callback = nullptr;
bool g_initialized = false;

}  // anonymous namespace

bool init() {
    if (g_initialized) {
        return true;
    }
    
    // Initialize NVS first
    if (!pxlcam::nvs::init()) {
        PXLCAM_LOGW_TAG(kLogTag, "NVS init failed, using default mode");
        g_currentMode = CaptureMode::Normal;
        g_initialized = true;
        return true;
    }
    
    // Load saved mode from NVS
    uint8_t savedMode = pxlcam::nvs::readU8(pxlcam::nvs::keys::kCaptureMode, 
                                            static_cast<uint8_t>(CaptureMode::Normal));
    
    // Validate saved mode
    if (savedMode >= static_cast<uint8_t>(CaptureMode::COUNT)) {
        PXLCAM_LOGW_TAG(kLogTag, "Invalid saved mode %d, resetting to Normal", savedMode);
        savedMode = static_cast<uint8_t>(CaptureMode::Normal);
    }
    
    g_currentMode = static_cast<CaptureMode>(savedMode);
    g_initialized = true;
    
    PXLCAM_LOGI_TAG(kLogTag, "Mode manager initialized, mode: %s", getModeName(g_currentMode));
    return true;
}

CaptureMode getCurrentMode() {
    return g_currentMode;
}

void setMode(CaptureMode mode, bool persist) {
    if (mode >= CaptureMode::COUNT) {
        PXLCAM_LOGW_TAG(kLogTag, "Invalid mode %d, ignoring", static_cast<int>(mode));
        return;
    }
    
    CaptureMode oldMode = g_currentMode;
    g_currentMode = mode;
    
    if (persist && pxlcam::nvs::isInitialized()) {
        pxlcam::nvs::writeU8(pxlcam::nvs::keys::kCaptureMode, static_cast<uint8_t>(mode));
    }
    
    PXLCAM_LOGI_TAG(kLogTag, "Mode changed: %s -> %s", getModeName(oldMode), getModeName(mode));
    
    // Notify callback if registered and mode actually changed
    if (g_callback && oldMode != mode) {
        g_callback(mode);
    }
}

CaptureMode cycleMode(bool persist) {
    uint8_t nextMode = (static_cast<uint8_t>(g_currentMode) + 1) % static_cast<uint8_t>(CaptureMode::COUNT);
    setMode(static_cast<CaptureMode>(nextMode), persist);
    return g_currentMode;
}

const char* getModeName(CaptureMode mode) {
    switch (mode) {
        case CaptureMode::Normal:  return "Normal";
        case CaptureMode::GameBoy: return "GameBoy";
        case CaptureMode::Night:   return "Night";
        default:                   return "Unknown";
    }
}

char getModeChar(CaptureMode mode) {
    switch (mode) {
        case CaptureMode::Normal:  return 'N';
        case CaptureMode::GameBoy: return 'G';
        case CaptureMode::Night:   return 'X';
        default:                   return '?';
    }
}

void setModeChangeCallback(ModeChangeCallback callback) {
    g_callback = callback;
}

bool requiresPostProcess() {
    return g_currentMode != CaptureMode::Normal;
}

bool isNightMode() {
    return g_currentMode == CaptureMode::Night;
}

bool usesDithering() {
    return g_currentMode == CaptureMode::GameBoy || g_currentMode == CaptureMode::Night;
}

void resetToDefault() {
    g_currentMode = CaptureMode::Normal;
    
    if (pxlcam::nvs::isInitialized()) {
        pxlcam::nvs::erase(pxlcam::nvs::keys::kCaptureMode);
    }
    
    PXLCAM_LOGI_TAG(kLogTag, "Mode reset to default (Normal)");
    
    if (g_callback) {
        g_callback(CaptureMode::Normal);
    }
}

}  // namespace pxlcam::mode
