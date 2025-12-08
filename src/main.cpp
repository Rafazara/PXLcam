#include <Arduino.h>

#include "app_controller.h"

pxlcam::AppController app;

void setup() {
    Serial.begin(115200);
    Serial.println();
    Serial.println("PXLcam booting...");
    app.begin();
}

void loop() {
    app.tick();
    yield();
}