#include "camera_bringup.h"
#include "camera_config.h"
#include "display.h"
#include <Arduino.h>

namespace pxlcam::bringup {

void run() {
    Serial.println();
    Serial.println("===== CAMERA BRING-UP =====");

    // Build config from our defaults
    auto pins = pxlcam::makeDefaultPins();
    auto settings = pxlcam::makeDefaultSettings();
    auto config = pxlcam::buildCameraConfig(pins, settings);

    Serial.println("[INIT] Initializing camera...");
    pxlcam::display::printDisplay("CAM INIT...", 1, 0, 0, true, false);

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("[ERROR] Camera init failed: 0x%x\n", err);
        pxlcam::display::printDisplay("CAM ERROR", 1, 0, 0, true, false);
        return;
    }

    Serial.println("[OK] Camera initialized.");
    pxlcam::display::printDisplay("CAM OK", 1, 0, 0, true, false);

    delay(800);

    Serial.println("[CAPTURE] Capturing frame...");
    pxlcam::display::printDisplay("CAPTURING...", 1, 0, 0, true, false);

    uint32_t start = millis();
    camera_fb_t *fb = esp_camera_fb_get();
    uint32_t elapsed = millis() - start;

    if (!fb) {
        Serial.println("[ERROR] Failed to capture frame!");
        pxlcam::display::printDisplay("CAP ERR", 1, 0, 0, true, false);
        return;
    }

    Serial.printf("[OK] Frame captured. Format=%d  Size=%d bytes  Time=%lu ms\n",
                  fb->format, fb->len, elapsed);

    pxlcam::display::printDisplay("CAP OK", 1, 0, 0, true, false);

    esp_camera_fb_return(fb);

    Serial.println("===== END CAMERA BRING-UP =====");
}

} // namespace pxlcam::bringup
