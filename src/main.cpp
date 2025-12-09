#include <Arduino.h>
#include "app_controller.h"
#include "preview.h"

#ifndef PXLCAM_VERSION
#define PXLCAM_VERSION "1.0.0"
#endif

pxlcam::AppController app;

void setup() {
    Serial.begin(115200);
    Serial.println();
    Serial.println("========================================");
    Serial.println("  PXLcam Firmware v" PXLCAM_VERSION);
    Serial.println("  ESP32-CAM Pixel Art Camera");
    Serial.println("========================================");
    Serial.println();

    app.begin();
    pxlcam::preview::begin();
}

void loop() {
    app.tick();
    yield();
}