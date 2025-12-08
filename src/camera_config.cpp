#include "camera_config.h"

#include <esp32-hal-psram.h>
#include <esp_err.h>
#include <esp_log.h>

#include "logging.h"

namespace pxlcam {

namespace {

constexpr const char *kLogTag = "pxlcam-camera";

bool g_cameraInitialized = false;
bool g_ledConfigured = false;
CameraPins g_activePins{};
[[maybe_unused]] CameraSettings g_activeSettings{};

void configureLed(const CameraPins &pins, bool enableFlash) {
    g_ledConfigured = false;
    if (!enableFlash || pins.pinLed == GPIO_NUM_NC) {
        return;
    }

    if (gpio_reset_pin(pins.pinLed) != ESP_OK) {
        ESP_LOGW(kLogTag, "Failed to reset LED GPIO %d", static_cast<int>(pins.pinLed));
        return;
    }

    if (gpio_set_direction(pins.pinLed, GPIO_MODE_OUTPUT) != ESP_OK) {
        ESP_LOGW(kLogTag, "Failed to configure LED GPIO %d as output", static_cast<int>(pins.pinLed));
        return;
    }

    if (gpio_set_level(pins.pinLed, 0) != ESP_OK) {
        ESP_LOGW(kLogTag, "Failed to drive LED GPIO %d low", static_cast<int>(pins.pinLed));
        return;
    }

    g_ledConfigured = true;
}

void deactivateLed() {
    if (!g_ledConfigured || g_activePins.pinLed == GPIO_NUM_NC) {
        return;
    }
    if (gpio_set_level(g_activePins.pinLed, 0) != ESP_OK) {
        ESP_LOGW(kLogTag, "Failed to drive LED GPIO %d low", static_cast<int>(g_activePins.pinLed));
    }

    if (gpio_reset_pin(g_activePins.pinLed) != ESP_OK) {
        ESP_LOGW(kLogTag, "Failed to reset LED GPIO %d", static_cast<int>(g_activePins.pinLed));
    }

    g_ledConfigured = false;
}

}  // namespace

CameraPins makeDefaultPins() {
    CameraPins pins{};
    pins.pinPclk = GPIO_NUM_22;
    pins.pinVsync = GPIO_NUM_25;
    pins.pinHref = GPIO_NUM_23;
    pins.pinSccbSda = GPIO_NUM_26;
    pins.pinSccbScl = GPIO_NUM_27;
    pins.pinXclk = GPIO_NUM_0;
    pins.pinPwdn = GPIO_NUM_32;
    pins.pinReset = GPIO_NUM_NC;
    pins.pinD0 = GPIO_NUM_5;
    pins.pinD1 = GPIO_NUM_18;
    pins.pinD2 = GPIO_NUM_19;
    pins.pinD3 = GPIO_NUM_21;
    pins.pinD4 = GPIO_NUM_36;
    pins.pinD5 = GPIO_NUM_39;
    pins.pinD6 = GPIO_NUM_34;
    pins.pinD7 = GPIO_NUM_35;
    pins.pinLed = GPIO_NUM_4;
    return pins;
}

CameraSettings makeDefaultSettings() {
    CameraSettings settings{};
    settings.frameSize = FRAMESIZE_UXGA;
    settings.pixelFormat = PIXFORMAT_JPEG;
    settings.jpegQuality = 12;
    settings.frameBufferCount = 2;
    settings.enableLedFlash = false;
    return settings;
}

camera_config_t buildCameraConfig(const CameraPins &pins, const CameraSettings &settings) {
    camera_config_t config{};

    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;

    config.pin_pwdn = static_cast<int>(pins.pinPwdn);
    config.pin_reset = static_cast<int>(pins.pinReset);
    config.pin_xclk = static_cast<int>(pins.pinXclk);
    config.pin_sscb_sda = static_cast<int>(pins.pinSccbSda);
    config.pin_sscb_scl = static_cast<int>(pins.pinSccbScl);
    config.pin_pclk = static_cast<int>(pins.pinPclk);
    config.pin_vsync = static_cast<int>(pins.pinVsync);
    config.pin_href = static_cast<int>(pins.pinHref);
    config.pin_d0 = static_cast<int>(pins.pinD0);
    config.pin_d1 = static_cast<int>(pins.pinD1);
    config.pin_d2 = static_cast<int>(pins.pinD2);
    config.pin_d3 = static_cast<int>(pins.pinD3);
    config.pin_d4 = static_cast<int>(pins.pinD4);
    config.pin_d5 = static_cast<int>(pins.pinD5);
    config.pin_d6 = static_cast<int>(pins.pinD6);
    config.pin_d7 = static_cast<int>(pins.pinD7);

    config.xclk_freq_hz = 20000000;
    config.pixel_format = settings.pixelFormat;
    config.frame_size = settings.frameSize;
    config.jpeg_quality = settings.jpegQuality;

    const uint8_t fbCount = settings.frameBufferCount == 0 ? 1 : settings.frameBufferCount;
    config.fb_count = fbCount;
    config.grab_mode = fbCount > 1 ? CAMERA_GRAB_LATEST : CAMERA_GRAB_WHEN_EMPTY;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.sccb_i2c_port = 0;

    return config;
}

bool initCamera(const CameraPins &pins, const CameraSettings &settings) {
    if (g_cameraInitialized) {
        shutdownCamera();
    }

    bool psramAvailable = psramFound();
    if (!psramAvailable) {
        PXLCAM_LOGW_TAG(kLogTag, "PSRAM not detected; forcing single framebuffer in DRAM");
    }

    camera_config_t config = buildCameraConfig(pins, settings);

    if (!psramAvailable) {
        config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_DRAM;
        if (settings.pixelFormat != PIXFORMAT_JPEG) {
            PXLCAM_LOGW_TAG(kLogTag, "RGB formats may fail without PSRAM");
        }
    } else {
        config.fb_location = CAMERA_FB_IN_PSRAM;
        if (config.fb_count < 2) {
            config.fb_count = 2;
            config.grab_mode = CAMERA_GRAB_LATEST;
        }
    }

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        PXLCAM_LOGE_TAG(kLogTag, "Camera init failed, err=0x%X", static_cast<unsigned>(err));
        return false;
    }

    g_cameraInitialized = true;
    g_activePins = pins;
    g_activeSettings = settings;

    configureLed(pins, settings.enableLedFlash);

    return true;
}

camera_fb_t *captureFrame() {
    if (!g_cameraInitialized) {
        PXLCAM_LOGW_TAG(kLogTag, "captureFrame called before camera initialized");
        return nullptr;
    }

    return esp_camera_fb_get();
}

void releaseFrame(camera_fb_t *frame) {
    if (!frame) {
        return;
    }
    esp_camera_fb_return(frame);
}

void shutdownCamera() {
    if (!g_cameraInitialized) {
        return;
    }

    esp_err_t err = esp_camera_deinit();
    if (err != ESP_OK) {
        PXLCAM_LOGW_TAG(kLogTag, "Camera deinit returned err=0x%X", static_cast<unsigned>(err));
    }

    g_cameraInitialized = false;
    deactivateLed();
}

}  // namespace pxlcam
