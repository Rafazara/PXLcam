#include "app_controller.h"

#include <cstdio>
#include <cstring>

#include <esp_heap_caps.h>
#include <esp32-hal-psram.h>

#include "logging.h"

namespace pxlcam {

namespace {
constexpr uint32_t kFeedbackDurationMs = 1500;
#ifdef PXLCAM_ENABLE_METRICS
constexpr bool kEnableMetrics = true;
#else
constexpr bool kEnableMetrics = false;
#endif
}  // namespace

void AppController::begin() {
    PXLCAM_LOGI("AppController begin");
    cameraPins_ = makeDefaultPins();
    cameraSettings_ = makeDefaultSettings();
    fallbackToJpeg_ = false;

    button_.begin();
    startupGuardExpiryMs_ = millis() + 1000;  // Mitigate GPIO12 boot strap risk.

    transitionTo(AppState::InitDisplay);
}

void AppController::tick() {
    const uint32_t now = millis();
    if (now >= startupGuardExpiryMs_) {
        button_.update(now);
    }

    switch (state_) {
        case AppState::Boot:
            transitionTo(AppState::InitDisplay);
            break;
        case AppState::InitDisplay:
            handleInitDisplay();
            break;
        case AppState::InitStorage:
            handleInitStorage();
            break;
        case AppState::InitCamera:
            handleInitCamera();
            break;
        case AppState::Idle:
            handleIdle();
            break;
        case AppState::Capture:
            handleCapture(now);
            break;
        case AppState::Filter:
            handleFilter();
            break;
        case AppState::Save:
            handleSave();
            break;
        case AppState::Feedback:
            handleFeedback(now);
            break;
        case AppState::Error:
        default:
            // Remain in error state until reset.
            break;
    }
}

void AppController::transitionTo(AppState nextState) {
    state_ = nextState;
}

void AppController::enterError(const char *message) {
    strncpy(lastMessage_, message, sizeof(lastMessage_) - 1);
    lastMessage_[sizeof(lastMessage_) - 1] = '\0';
    PXLCAM_LOGE("%s", lastMessage_);
    showStatus(lastMessage_);
    state_ = AppState::Error;
    initializationFailed_ = true;
}

void AppController::showStatus(const char *message, bool clear) {
    display::printDisplay(message, 1, 0, 0, clear);
}

void AppController::handleInitDisplay() {
    if (!display::initDisplay(displayConfig_)) {
        enterError("DISPLAY ERROR");
        return;
    }

    showStatus("PXLcam\nIniciando...", true);
    transitionTo(AppState::InitStorage);
}

void AppController::handleInitStorage() {
    if (!storage::initSD(storageConfig_)) {
        enterError("SD ERROR");
        return;
    }

    showStatus("SD READY", true);
    transitionTo(AppState::InitCamera);
}

void AppController::handleInitCamera() {
    if (!configureCamera()) {
        enterError(psramAvailable_ ? "CAM ERROR" : "NO CAMERA");
        return;
    }

    if (filterConfig_.enabled && cameraUsesRgb_) {
        filter::init(filterConfig_);
    } else {
        filter::reset();
    }

    if (cameraUsesRgb_) {
        showStatus("PXLcam pronta\nPressione botao", true);
    } else if (!psramAvailable_) {
        showStatus("NO PSRAM\nJPEG MODE", true);
    } else if (fallbackToJpeg_) {
        showStatus("RGB FAIL\nJPEG MODE", true);
    } else {
        showStatus("JPEG MODE\nPressione botao", true);
    }
    feedbackShown_ = false;
    transitionTo(AppState::Idle);
}

void AppController::handleIdle() {
    if (button_.consumePressed()) {
        captureDurationMs_ = filterDurationMs_ = saveDurationMs_ = 0;
        feedbackShown_ = false;
        transitionTo(AppState::Capture);
    }
}

void AppController::handleCapture(uint32_t nowMs) {
    const uint32_t start = nowMs;
    showStatus("Capturando...");

    activeFrame_ = captureFrame();
    captureDurationMs_ = millis() - start;

    if (!activeFrame_) {
        strncpy(lastMessage_, "CAM ERROR", sizeof(lastMessage_) - 1);
        lastMessage_[sizeof(lastMessage_) - 1] = '\0';
        showStatus(lastMessage_);
        feedbackExpiryMs_ = millis() + kFeedbackDurationMs;
        feedbackShown_ = false;
        transitionTo(AppState::Feedback);
        return;
    }

    if (!cameraUsesRgb_ || !filterConfig_.enabled) {
        transitionTo(AppState::Save);
    } else {
        transitionTo(AppState::Filter);
    }
}

void AppController::handleFilter() {
    if (activeFrame_ && cameraUsesRgb_ && filterConfig_.enabled) {
        const uint32_t start = millis();
        filter::apply(activeFrame_);
        filterDurationMs_ = millis() - start;
    }
    transitionTo(AppState::Save);
}

void AppController::handleSave() {
    if (!activeFrame_) {
        transitionTo(AppState::Feedback);
        return;
    }

    const uint32_t start = millis();

    const char *extension = cameraUsesRgb_ ? "rgb" : "jpg";
    char filePath[64];
    snprintf(filePath, sizeof(filePath), "/captures/frame_%lu.%s", static_cast<unsigned long>(millis()), extension);

    const bool saved = storage::saveFile(filePath, activeFrame_);
    saveDurationMs_ = millis() - start;

    if (saved) {
        snprintf(lastMessage_, sizeof(lastMessage_), "SAVE OK\n%s", filePath);
        PXLCAM_LOGI("Frame saved to %s", filePath);
    } else {
        strncpy(lastMessage_, "SAVE FAIL", sizeof(lastMessage_) - 1);
        lastMessage_[sizeof(lastMessage_) - 1] = '\0';
        PXLCAM_LOGE("Failed to save frame");
    }

    releaseActiveFrame();
    feedbackExpiryMs_ = millis() + kFeedbackDurationMs;
    feedbackShown_ = false;

    if (kEnableMetrics) {
        logMetrics();
    }

    transitionTo(AppState::Feedback);
}

void AppController::handleFeedback(uint32_t nowMs) {
    if (!feedbackShown_) {
        showStatus(lastMessage_);
        feedbackShown_ = true;
    }

    if (nowMs >= feedbackExpiryMs_) {
        if (cameraUsesRgb_) {
            showStatus("PXLcam pronta\nPressione botao", true);
        } else if (!psramAvailable_) {
            showStatus("NO PSRAM\nJPEG MODE", true);
        } else if (fallbackToJpeg_) {
            showStatus("RGB FAIL\nJPEG MODE", true);
        } else {
            showStatus("JPEG MODE\nPressione botao", true);
        }
        feedbackShown_ = false;
        transitionTo(AppState::Idle);
    }
}

bool AppController::configureCamera() {
    psramAvailable_ = psramFound();
    fallbackToJpeg_ = false;
    cameraSettings_ = makeDefaultSettings();
    cameraSettings_.frameSize = FRAMESIZE_QVGA;
    cameraSettings_.frameBufferCount = psramAvailable_ ? 2 : 1;
    cameraSettings_.enableLedFlash = false;

    if (!psramAvailable_) {
        strncpy(lastMessage_, "NO PSRAM", sizeof(lastMessage_) - 1);
        lastMessage_[sizeof(lastMessage_) - 1] = '\0';
        PXLCAM_LOGW("PSRAM unavailable; using JPEG fallback");
        cameraSettings_.pixelFormat = PIXFORMAT_JPEG;
        cameraUsesRgb_ = false;
        fallbackToJpeg_ = true;
        return initCamera(cameraPins_, cameraSettings_);
    }

    cameraSettings_.pixelFormat = PIXFORMAT_RGB888;
    cameraUsesRgb_ = true;
    if (initCamera(cameraPins_, cameraSettings_)) {
        return true;
    }

    PXLCAM_LOGW("RGB888 init failed, attempting JPEG fallback");
    shutdownCamera();
    cameraUsesRgb_ = false;
    cameraSettings_.pixelFormat = PIXFORMAT_JPEG;
    fallbackToJpeg_ = true;
    strncpy(lastMessage_, "RGB FAIL", sizeof(lastMessage_) - 1);
    lastMessage_[sizeof(lastMessage_) - 1] = '\0';
    return initCamera(cameraPins_, cameraSettings_);
}

void AppController::releaseActiveFrame() {
    if (activeFrame_) {
        pxlcam::releaseFrame(activeFrame_);
        activeFrame_ = nullptr;
    }
}

void AppController::logMetrics() const {
    const uint32_t freePsram = psramFound() ? ESP.getFreePsram() : 0;
    const uint32_t freeHeap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    PXLCAM_LOGI("Metrics - capture:%ums filter:%ums save:%ums free_psram:%u free_heap:%u", captureDurationMs_, filterDurationMs_, saveDurationMs_, freePsram, freeHeap);
}

}  // namespace pxlcam
