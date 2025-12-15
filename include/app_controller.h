#pragma once

#include <Arduino.h>

#include "button_manager.h"
#include "camera_config.h"
#include "display.h"
#include "pixel_filter.h"
#include "storage.h"

namespace pxlcam {

enum class AppState {
    Boot,
    InitDisplay,
    InitStorage,
    InitCamera,
    Idle,
    Capture,
    Filter,
    Save,
    Feedback,
    Error
};

class AppController {
   public:
    void begin();
    void tick();

   private:
    void transitionTo(AppState nextState);
    void enterError(const char *message);
    void showStatus(const char *message, bool clear = true);
    void showIdleScreen();  // v1.2.0: unified idle display
    void handleMenuInput(); // v1.2.0: menu button handling

    void handleInitDisplay();
    void handleInitStorage();
    void handleInitCamera();
    void handleIdle();
    void handleCapture(uint32_t nowMs);
    void handleFilter();
    void handleSave();
    void handleFeedback(uint32_t nowMs);
    void handleError();
    
    // v1.3.0: Timelapse handling
    void handleTimelapseMenu();
    void updateTimelapseDisplay();
    
    // v1.3.0: WiFi Preview handling
    void handleWifiMenu();
    void handleWifiPreviewToggle();
    void updateWifiPreviewDisplay();

    bool configureCamera();
    void releaseActiveFrame();
    void logMetrics() const;
    uint32_t getNextFileNumber();

    AppState state_ = AppState::Boot;
    ButtonManager button_{GPIO_NUM_12, LOW, 150};

    CameraPins cameraPins_{};
    CameraSettings cameraSettings_{};
    display::DisplayConfig displayConfig_{128, 64, 0, -1, 0x3C, 14, 15, 400000};
    storage::StorageConfig storageConfig_{"/sdcard", 0, true};
    filter::FilterConfig filterConfig_{true, 8, 0};

    camera_fb_t *activeFrame_ = nullptr;
    bool psramAvailable_ = false;
    bool cameraUsesRgb_ = false;
    bool initializationFailed_ = false;
    bool fallbackToJpeg_ = false;
    bool sdAvailable_ = false;

    uint32_t startupGuardExpiryMs_ = 0;
    uint32_t feedbackExpiryMs_ = 0;
    bool feedbackShown_ = false;
    uint32_t fileCounter_ = 0;

    uint32_t captureDurationMs_ = 0;
    uint32_t filterDurationMs_ = 0;
    uint32_t saveDurationMs_ = 0;

    char lastMessage_[64] = {0};
    
    // v1.2.0: stylized capture state
    uint8_t* processedImageData_ = nullptr;
    size_t processedImageLen_ = 0;
    const char* processedExtension_ = "raw";
    
    // v1.3.0: WiFi Preview state
    bool wifiPreviewActive_ = false;
};

}  // namespace pxlcam
