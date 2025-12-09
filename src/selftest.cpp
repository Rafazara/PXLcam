#include "selftest.h"

#ifdef PXLCAM_SELFTEST

#include <Arduino.h>
#include <esp_system.h>

#include "display.h"

namespace pxlcam::selftest {

namespace {

constexpr uint8_t kButtonPin = 12;
constexpr uint32_t kButtonTestDurationMs = 3000;
constexpr uint32_t kButtonPollIntervalMs = 50;

void printHeader() {
    Serial.println();
    Serial.println("========================================");
    Serial.println("         PXLcam Self-Test");
    Serial.println("========================================");
    Serial.println();
}

void testPsram() {
    Serial.print("[PSRAM]       ");
    if (psramFound()) {
        const uint32_t psramSize = ESP.getPsramSize();
        const uint32_t psramFree = ESP.getFreePsram();
        Serial.print("OK - Total: ");
        Serial.print(psramSize / 1024);
        Serial.print(" KB, Free: ");
        Serial.print(psramFree / 1024);
        Serial.println(" KB");
    } else {
        Serial.println("NOT FOUND (camera will use JPEG fallback)");
    }
}

void testHeap() {
    Serial.print("[HEAP]        ");
    const uint32_t freeHeap = ESP.getFreeHeap();
    const uint32_t totalHeap = ESP.getHeapSize();
    Serial.print("OK - Free: ");
    Serial.print(freeHeap / 1024);
    Serial.print(" KB / Total: ");
    Serial.print(totalHeap / 1024);
    Serial.println(" KB");
}

void testDisplay() {
    Serial.print("[DISPLAY]     ");

    // Configuração padrão do display SSD1306 128x64
    pxlcam::display::DisplayConfig displayCfg{};
    displayCfg.width = 128;
    displayCfg.height = 64;
    displayCfg.rotation = 0;
    displayCfg.resetPin = -1;
    displayCfg.i2cAddress = 0x3C;
    displayCfg.sdaPin = 15;
    displayCfg.sclPin = 14;
    displayCfg.i2cFrequencyHz = 400000;

    if (pxlcam::display::initDisplay(displayCfg)) {
        pxlcam::display::printDisplay("SelfTest OK", 1, 0, 0, true, false);
        Serial.println("OK - SSD1306 initialized");
    } else {
        Serial.println("FAIL - Could not initialize display");
    }
}

void testButton() {
    Serial.print("[BUTTON]      ");
    Serial.println("Waiting for GPIO12 press (3s)...");

    pinMode(kButtonPin, INPUT_PULLUP);

    bool pressed = false;
    const uint32_t startMs = millis();

    while ((millis() - startMs) < kButtonTestDurationMs) {
        if (digitalRead(kButtonPin) == LOW) {
            pressed = true;
            break;
        }
        delay(kButtonPollIntervalMs);
    }

    Serial.print("              ");
    if (pressed) {
        Serial.println("OK - Button press detected!");
    } else {
        Serial.println("TIMEOUT - No press detected (may be OK if not pressed)");
    }
}

void printChipInfo() {
    Serial.println("[CHIP INFO]");
    Serial.print("              Model: ");
    Serial.println(ESP.getChipModel());
    Serial.print("              Cores: ");
    Serial.println(ESP.getChipCores());
    Serial.print("              Rev:   ");
    Serial.println(ESP.getChipRevision());
    Serial.print("              Flash: ");
    Serial.print(ESP.getFlashChipSize() / (1024 * 1024));
    Serial.println(" MB");
}

void printFooter() {
    Serial.println();
    Serial.println("========================================");
    Serial.println("         END SELF-TEST");
    Serial.println("========================================");
    Serial.println();
    Serial.println("Recompile WITHOUT -DPXLCAM_SELFTEST to return to normal firmware.");
    Serial.println();
}

}  // namespace

void run() {
    // Garante que serial está pronta
    delay(100);

    printHeader();
    printChipInfo();
    Serial.println();

    testPsram();
    testHeap();
    testDisplay();
    testButton();

    printFooter();

    // Loop infinito — não continua para o app principal
    while (true) {
        delay(500);
    }
}

}  // namespace pxlcam::selftest

#endif  // PXLCAM_SELFTEST
