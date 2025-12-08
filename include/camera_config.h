#pragma once

#include <driver/gpio.h>
#include <esp_camera.h>

namespace pxlcam {

// Camera configuration interface for the PXLcam firmware.

// Logical mapping of the ESP32-CAM AI Thinker pins.
struct CameraPins {
    gpio_num_t pinPclk;
    gpio_num_t pinVsync;
    gpio_num_t pinHref;
    gpio_num_t pinSccbSda;
    gpio_num_t pinSccbScl;
    gpio_num_t pinXclk;
    gpio_num_t pinPwdn;
    gpio_num_t pinReset;
    gpio_num_t pinD0;
    gpio_num_t pinD1;
    gpio_num_t pinD2;
    gpio_num_t pinD3;
    gpio_num_t pinD4;
    gpio_num_t pinD5;
    gpio_num_t pinD6;
    gpio_num_t pinD7;
    gpio_num_t pinLed;
};

// High-level camera tuning parameters exposed to the application.
struct CameraSettings {
    framesize_t frameSize;
    pixformat_t pixelFormat;
    uint8_t jpegQuality;
    uint8_t frameBufferCount;
    bool enableLedFlash;
};

CameraPins makeDefaultPins();
CameraSettings makeDefaultSettings();
camera_config_t buildCameraConfig(const CameraPins &pins, const CameraSettings &settings);
bool initCamera(const CameraPins &pins, const CameraSettings &settings);
camera_fb_t *captureFrame();
void releaseFrame(camera_fb_t *frame);
void shutdownCamera();

}  // namespace pxlcam
