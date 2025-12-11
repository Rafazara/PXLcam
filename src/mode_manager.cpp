/**
 * @file mode_manager.cpp
 * @brief Centralized mode management implementation for PXLcam v1.2.0
 */

#include "mode_manager.h"
#include "nvs_store.h"
#include "logging.h"

namespace pxlcam::mode {

namespace {

constexpr const char* kLogTag = "mode";

CaptureMode g_currentMode = CaptureMode::Normal;
ModeChangeCallback g_callback = nullptr;
bool g_initialized = false;

}  // anonymous namespace

bool init() {
    if (g_initialized) {
        return true;
    }
    
    // Initialize NVS first using new API
    pxlcam::nvs::nvsStoreInit();
    
    // Load saved mode from NVS using simplified API
    uint8_t savedMode = pxlcam::nvs::loadModeOrDefault(
        static_cast<uint8_t>(CaptureMode::Normal)
    );
    
    // Validate saved mode
    if (savedMode >= static_cast<uint8_t>(CaptureMode::COUNT)) {
        PXLCAM_LOGW_TAG(kLogTag, "Modo salvo invalido %d, resetando para Normal", savedMode);
        savedMode = static_cast<uint8_t>(CaptureMode::Normal);
    }
    
    g_currentMode = static_cast<CaptureMode>(savedMode);
    g_initialized = true;
    
    PXLCAM_LOGI_TAG(kLogTag, "ModeManager inicializado, modo: %s (%d)", 
                    getModeName(g_currentMode), static_cast<int>(g_currentMode));
    return true;
}

CaptureMode getCurrentMode() {
    return g_currentMode;
}

void setMode(CaptureMode mode, bool persist) {
    if (mode >= CaptureMode::COUNT) {
        PXLCAM_LOGW_TAG(kLogTag, "Modo invalido %d, ignorando", static_cast<int>(mode));
        return;
    }
    
    CaptureMode oldMode = g_currentMode;
    g_currentMode = mode;
    
    // Persist to NVS using new simplified API
    if (persist) {
        pxlcam::nvs::saveMode(static_cast<uint8_t>(mode));
    }
    
    PXLCAM_LOGI_TAG(kLogTag, "Modo alterado: %s -> %s", getModeName(oldMode), getModeName(mode));
    
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
    
    // Erase saved mode from NVS
    pxlcam::nvs::erase(pxlcam::nvs::keys::kCaptureMode);
    
    PXLCAM_LOGI_TAG(kLogTag, "Modo resetado para Normal");
    
    if (g_callback) {
        g_callback(CaptureMode::Normal);
    }
}

}  // namespace pxlcam::mode
