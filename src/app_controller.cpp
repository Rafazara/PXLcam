#include "app_controller.h"

#include <cstdio>
#include <cstring>

#include <esp_heap_caps.h>
#include <esp32-hal-psram.h>

#include "logging.h"
#include "preview.h"
#include "pxlcam_config.h"

#if PXLCAM_AUTO_EXPOSURE
#include "exposure_ctrl.h"
#endif

// v1.2.0 modules
#include "mode_manager.h"
#include "capture_pipeline.h"
#include "ui_menu.h"
#include "nvs_store.h"

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
    PXLCAM_LOGI("AppController begin (v1.2.0)");
    cameraPins_ = makeDefaultPins();
    cameraSettings_ = makeDefaultSettings();
    fallbackToJpeg_ = false;

    button_.begin();
    startupGuardExpiryMs_ = millis() + 1000;  // Mitigate GPIO12 boot strap risk.

    // Initialize v1.2.0 subsystems
    pxlcam::nvs::init();
    pxlcam::mode::init();
    pxlcam::ui::init();

    transitionTo(AppState::InitDisplay);
}

void AppController::tick() {
    const uint32_t now = millis();
    if (now >= startupGuardExpiryMs_) {
        button_.update(now);
    }

    // Handle menu if visible (v1.2.0)
#if PXLCAM_ENABLE_MENU
    if (pxlcam::ui::isMenuVisible()) {
        handleMenuInput();
        pxlcam::ui::updateDisplay();
        return;
    }
#endif

    // Check for button events in Idle state
    if (state_ == AppState::Idle) {
        ButtonEvent event = button_.consumeEvent();
        
        switch (event) {
            case ButtonEvent::ShortPress:
                // Short press: capture with current mode
                captureDurationMs_ = filterDurationMs_ = saveDurationMs_ = 0;
                feedbackShown_ = false;
                transitionTo(AppState::Capture);
                return;
                
            case ButtonEvent::LongPress:
                // Long press (500ms-2s): enter preview mode
                pxlcam::preview::runPreviewLoop();
                showIdleScreen();
                return;
                
            case ButtonEvent::VeryLongPress:
                // Very long press (2s+): open mode menu
#if PXLCAM_ENABLE_MENU
                pxlcam::ui::showModeMenu();
#endif
                return;
                
            default:
                break;
        }
        
        // Legacy: also support held() for preview
        if (button_.held(1000) && event == ButtonEvent::None) {
            pxlcam::preview::runPreviewLoop();
            showIdleScreen();
            return;
        }
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
            handleError();
            break;
        default:
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
#if PXLCAM_ENABLE_MENU
    pxlcam::ui::drawErrorScreen("ERRO", lastMessage_, true);
#else
    showStatus(lastMessage_);
#endif
    state_ = AppState::Error;
    initializationFailed_ = true;
}

void AppController::showStatus(const char *message, bool clear) {
    display::printDisplay(message, 1, 0, 0, clear);
}

void AppController::showIdleScreen() {
    char buf[64];
    snprintf(buf, sizeof(buf), "PXLcam v1.2.0\nModo: %s\n\nTap:foto Hold:prev\nHold2s:menu", 
             pxlcam::mode::getModeName(pxlcam::mode::getCurrentMode()));
    showStatus(buf, true);
}

void AppController::handleMenuInput() {
#if PXLCAM_ENABLE_MENU
    ButtonEvent event = button_.consumeEvent();
    
    switch (event) {
        case ButtonEvent::ShortPress:
            pxlcam::ui::handleTap();
            break;
        case ButtonEvent::LongPress:
        case ButtonEvent::VeryLongPress:
            if (pxlcam::ui::handleHold() == pxlcam::ui::MenuAction::ModeChanged) {
                // Mode was changed, will show success screen
            }
            if (!pxlcam::ui::isMenuVisible()) {
                showIdleScreen();
            }
            break;
        default:
            break;
    }
#endif
}

void AppController::handleInitDisplay() {
    if (!display::initDisplay(displayConfig_)) {
        enterError("DISPLAY ERROR");
        return;
    }

    showStatus("PXLcam v1.2.0\nIniciando...", true);
    transitionTo(AppState::InitStorage);
}

void AppController::handleInitStorage() {
    sdAvailable_ = storage::initSD(storageConfig_);
    
    if (sdAvailable_) {
        showStatus("SD READY", true);
    } else {
        PXLCAM_LOGW("SD not available - captures will not be saved");
#if PXLCAM_ENABLE_MENU
        pxlcam::ui::drawErrorScreen("AVISO", "SD nao encontrado\nApenas preview", false);
        delay(2000);
#else
        showStatus("NO SD\nPreview only", true);
        delay(1500);
#endif
    }
    
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

#if PXLCAM_AUTO_EXPOSURE
    // Initialize auto exposure control
    pxlcam::exposure::ExposureConfig expConfig;
    expConfig.autoExposure = true;
    expConfig.autoGain = true;
    expConfig.targetBrightness = 128;
    expConfig.tolerance = 20;
    pxlcam::exposure::init(expConfig);
    PXLCAM_LOGI("Auto exposure initialized");
#endif

#if PXLCAM_STYLIZED_CAPTURE
    // Initialize capture pipeline for stylized captures
    pxlcam::capture::init();
    PXLCAM_LOGI("Capture pipeline initialized");
#endif

    showIdleScreen();
    feedbackShown_ = false;
    transitionTo(AppState::Idle);
}

void AppController::handleIdle() {
    // Event-based handling moved to tick()
    // Legacy: direct press handling
    if (button_.consumePressed()) {
        captureDurationMs_ = filterDurationMs_ = saveDurationMs_ = 0;
        feedbackShown_ = false;
        transitionTo(AppState::Capture);
    }
}

void AppController::handleCapture(uint32_t nowMs) {
    const uint32_t start = nowMs;
    
#if PXLCAM_ENABLE_MENU
    pxlcam::ui::drawProgressScreen("Capturando...", 20);
#else
    showStatus("Capturando...");
#endif

#if PXLCAM_STYLIZED_CAPTURE
    // Use new capture pipeline with stylization
    pxlcam::capture::ProcessedImage processedImg;
    pxlcam::capture::CaptureResult result = pxlcam::capture::captureFrame(processedImg);
    
    captureDurationMs_ = pxlcam::capture::getLastCaptureDuration();
    filterDurationMs_ = pxlcam::capture::getLastProcessDuration();
    
    if (result != pxlcam::capture::CaptureResult::Success) {
        strncpy(lastMessage_, pxlcam::capture::getResultMessage(result), sizeof(lastMessage_) - 1);
        lastMessage_[sizeof(lastMessage_) - 1] = '\0';
#if PXLCAM_ENABLE_MENU
        pxlcam::ui::drawErrorScreen("CAPTURA", lastMessage_, true);
#else
        showStatus(lastMessage_);
#endif
        feedbackExpiryMs_ = millis() + kFeedbackDurationMs;
        feedbackShown_ = false;
        transitionTo(AppState::Feedback);
        return;
    }
    
    // Store processed image info for save
    processedImageData_ = processedImg.data;
    processedImageLen_ = processedImg.length;
    processedExtension_ = processedImg.extension;
    
    // Skip Filter state since pipeline already processed
    transitionTo(AppState::Save);
#else
    // Legacy capture path
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
#endif
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
#if PXLCAM_ENABLE_MENU
    pxlcam::ui::drawProgressScreen("Salvando...", 60);
#endif

    // Check if SD is available
    if (!sdAvailable_) {
        strncpy(lastMessage_, "SEM SD\nFoto perdida", sizeof(lastMessage_) - 1);
        lastMessage_[sizeof(lastMessage_) - 1] = '\0';
        PXLCAM_LOGW("No SD card - frame not saved");
        
#if PXLCAM_STYLIZED_CAPTURE
        pxlcam::capture::releaseFrame();
#else
        releaseActiveFrame();
#endif
        
        feedbackExpiryMs_ = millis() + kFeedbackDurationMs;
        feedbackShown_ = false;
        transitionTo(AppState::Feedback);
        return;
    }

    const uint32_t start = millis();
    const uint32_t fileNum = getNextFileNumber();

#if PXLCAM_STYLIZED_CAPTURE
    // Use processed image from capture pipeline
    if (!processedImageData_ || processedImageLen_ == 0) {
        strncpy(lastMessage_, "ERRO DADOS", sizeof(lastMessage_) - 1);
        lastMessage_[sizeof(lastMessage_) - 1] = '\0';
        pxlcam::capture::releaseFrame();
        feedbackExpiryMs_ = millis() + kFeedbackDurationMs;
        feedbackShown_ = false;
        transitionTo(AppState::Feedback);
        return;
    }
    
    // Build filename with mode indicator
    char modePrefix = pxlcam::mode::getModeChar(pxlcam::mode::getCurrentMode());
    char filePath[64];
    snprintf(filePath, sizeof(filePath), "/DCIM/PXL_%c%04lu.%s", 
             modePrefix, static_cast<unsigned long>(fileNum), processedExtension_);
    
    const bool saved = storage::saveFile(filePath, processedImageData_, processedImageLen_);
    saveDurationMs_ = millis() - start;
    
    pxlcam::capture::releaseFrame();
    processedImageData_ = nullptr;
    processedImageLen_ = 0;
#else
    // Legacy save path
    if (!activeFrame_) {
        transitionTo(AppState::Feedback);
        return;
    }
    
    const char *extension = cameraUsesRgb_ ? "raw" : "jpg";
    char filePath[64];
    snprintf(filePath, sizeof(filePath), "/DCIM/PXL_%04lu.%s", static_cast<unsigned long>(fileNum), extension);

    const bool saved = storage::saveFile(filePath, activeFrame_);
    saveDurationMs_ = millis() - start;
    
    releaseActiveFrame();
#endif

    if (saved) {
        snprintf(lastMessage_, sizeof(lastMessage_), "SALVO!\n%s", filePath);
        PXLCAM_LOGI("Frame saved to %s", filePath);
#if PXLCAM_ENABLE_MENU
        pxlcam::ui::drawSuccessScreen("FOTO SALVA", filePath + 6, 0);  // Skip "/DCIM/"
#endif
    } else {
        strncpy(lastMessage_, "ERRO SAVE", sizeof(lastMessage_) - 1);
        lastMessage_[sizeof(lastMessage_) - 1] = '\0';
        PXLCAM_LOGE("Failed to save frame");
#if PXLCAM_ENABLE_MENU
        pxlcam::ui::drawErrorScreen("ERRO", "Falha ao salvar", true);
#endif
    }

    feedbackExpiryMs_ = millis() + kFeedbackDurationMs;
    feedbackShown_ = false;

    if (kEnableMetrics) {
        logMetrics();
    }

    transitionTo(AppState::Feedback);
}

void AppController::handleFeedback(uint32_t nowMs) {
    if (!feedbackShown_) {
#if !PXLCAM_ENABLE_MENU
        showStatus(lastMessage_);
#endif
        feedbackShown_ = true;
    }

    if (nowMs >= feedbackExpiryMs_) {
        showIdleScreen();
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

void AppController::handleError() {
    // Allow exit from error state via button press
    if (button_.consumePressed()) {
        initializationFailed_ = false;
        transitionTo(AppState::InitDisplay);
    }
}

uint32_t AppController::getNextFileNumber() {
    return ++fileCounter_;
}

void AppController::logMetrics() const {
    const uint32_t freePsram = psramFound() ? ESP.getFreePsram() : 0;
    const uint32_t freeHeap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    PXLCAM_LOGI("Metrics - capture:%ums filter:%ums save:%ums free_psram:%u free_heap:%u", captureDurationMs_, filterDurationMs_, saveDurationMs_, freePsram, freeHeap);
}

}  // namespace pxlcam
