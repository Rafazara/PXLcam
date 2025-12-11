/**
 * @file mode_manager.cpp
 * @brief Centralized mode management implementation for PXLcam v1.2.0
 */

#include "mode_manager.h"
#include "nvs_store.h"
#include "logging.h"

#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

namespace pxlcam::mode {

namespace {

constexpr const char* kLogTag = "mode";

// Thread-safe state with spinlock for ISR-safe access
static portMUX_TYPE s_modeMux = portMUX_INITIALIZER_UNLOCKED;
static volatile CaptureMode s_currentMode = CaptureMode::Normal;
static ModeChangeCallback s_callback = nullptr;
static volatile bool s_initialized = false;

}  // anonymous namespace

bool init() {
    portENTER_CRITICAL(&s_modeMux);
    if (s_initialized) {
        portEXIT_CRITICAL(&s_modeMux);
        return true;
    }
    portEXIT_CRITICAL(&s_modeMux);
    
    // Initialize NVS first (outside critical section - may block)
    pxlcam::nvs::nvsStoreInit();
    
    // Load saved mode from NVS
    uint8_t savedMode = pxlcam::nvs::loadModeOrDefault(
        static_cast<uint8_t>(CaptureMode::Normal)
    );
    
    // Validate saved mode
    if (savedMode >= static_cast<uint8_t>(CaptureMode::COUNT)) {
        PXLCAM_LOGW_TAG(kLogTag, "Invalid mode %d, reset to Normal", savedMode);
        savedMode = static_cast<uint8_t>(CaptureMode::Normal);
    }
    
    portENTER_CRITICAL(&s_modeMux);
    s_currentMode = static_cast<CaptureMode>(savedMode);
    s_initialized = true;
    portEXIT_CRITICAL(&s_modeMux);
    
    PXLCAM_LOGI_TAG(kLogTag, "Init OK, mode=%s", getModeName(s_currentMode));
    return true;
}

CaptureMode getCurrentMode() {
    portENTER_CRITICAL(&s_modeMux);
    CaptureMode mode = s_currentMode;
    portEXIT_CRITICAL(&s_modeMux);
    return mode;
}

void setMode(CaptureMode mode, bool persist) {
    if (mode >= CaptureMode::COUNT) {
        PXLCAM_LOGW_TAG(kLogTag, "Invalid mode %d", static_cast<int>(mode));
        return;
    }
    
    CaptureMode oldMode;
    ModeChangeCallback cb = nullptr;
    
    portENTER_CRITICAL(&s_modeMux);
    oldMode = s_currentMode;
    s_currentMode = mode;
    cb = s_callback;
    portEXIT_CRITICAL(&s_modeMux);
    
    // Persist outside critical section (NVS may block)
    if (persist) {
        pxlcam::nvs::saveMode(static_cast<uint8_t>(mode));
    }
    
    // Notify callback outside critical section
    if (cb && oldMode != mode) {
        PXLCAM_LOGI_TAG(kLogTag, "%s->%s", getModeName(oldMode), getModeName(mode));
        cb(mode);
    }
}

CaptureMode cycleMode(bool persist) {
    portENTER_CRITICAL(&s_modeMux);
    uint8_t nextMode = (static_cast<uint8_t>(s_currentMode) + 1) % static_cast<uint8_t>(CaptureMode::COUNT);
    portEXIT_CRITICAL(&s_modeMux);
    
    setMode(static_cast<CaptureMode>(nextMode), persist);
    return getCurrentMode();
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
    portENTER_CRITICAL(&s_modeMux);
    s_callback = callback;
    portEXIT_CRITICAL(&s_modeMux);
}

bool requiresPostProcess() {
    return getCurrentMode() != CaptureMode::Normal;
}

bool isNightMode() {
    return getCurrentMode() == CaptureMode::Night;
}

bool usesDithering() {
    CaptureMode m = getCurrentMode();
    return m == CaptureMode::GameBoy || m == CaptureMode::Night;
}

void resetToDefault() {
    ModeChangeCallback cb = nullptr;
    
    portENTER_CRITICAL(&s_modeMux);
    s_currentMode = CaptureMode::Normal;
    cb = s_callback;
    portEXIT_CRITICAL(&s_modeMux);
    
    // Erase outside critical section
    pxlcam::nvs::erase(pxlcam::nvs::keys::kCaptureMode);
    
    PXLCAM_LOGI_TAG(kLogTag, "Reset to Normal");
    
    if (cb) {
        cb(CaptureMode::Normal);
    }
}

}  // namespace pxlcam::mode
