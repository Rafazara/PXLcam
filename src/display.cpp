#include "display.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <esp_log.h>

#include <cstdint>
#include <new>

#include "logging.h"

namespace pxlcam::display {

namespace {

constexpr const char *kLogTag = "pxlcam-display";

alignas(Adafruit_SSD1306) static std::uint8_t g_displayBuffer[sizeof(Adafruit_SSD1306)];
Adafruit_SSD1306 *g_display = nullptr;
DisplayConfig g_activeConfig{};
bool g_initialized = false;

bool ensureDisplayAllocated(const DisplayConfig &config) {
    if (g_display && g_display->width() == static_cast<int16_t>(config.width) &&
        g_display->height() == static_cast<int16_t>(config.height)) {
        return true;
    }

    if (g_display) {
        g_display->~Adafruit_SSD1306();
        g_display = nullptr;
    }

    g_display = new (g_displayBuffer) Adafruit_SSD1306(config.width, config.height, &Wire, config.resetPin);
    return g_display != nullptr;
}

void destroyDisplay() {
    if (g_display) {
        g_display->~Adafruit_SSD1306();
        g_display = nullptr;
    }
}

}  // namespace

bool initDisplay(const DisplayConfig &config) {
    if (config.width == 0 || config.height == 0) {
        PXLCAM_LOGE_TAG(kLogTag, "Invalid display dimensions: %ux%u", config.width, config.height);
        return false;
    }

    if (g_initialized) {
        shutdownDisplay();
    }

    Wire.begin(config.sdaPin, config.sclPin);
    if (config.i2cFrequencyHz != 0) {
        Wire.setClock(config.i2cFrequencyHz);
    }

    if (!ensureDisplayAllocated(config)) {
        PXLCAM_LOGE_TAG(kLogTag, "Failed to allocate Adafruit_SSD1306 instance");
        return false;
    }

    if (!g_display->begin(SSD1306_SWITCHCAPVCC, config.i2cAddress, true, false)) {
        PXLCAM_LOGE_TAG(kLogTag, "SSD1306 begin() failed at address 0x%02X", config.i2cAddress);
        destroyDisplay();
        return false;
    }

    g_display->setRotation(config.rotation);
    g_display->clearDisplay();
    g_display->setTextColor(SSD1306_WHITE);
    g_display->setTextSize(1);
    g_display->display();

    g_activeConfig = config;
    g_initialized = true;

    PXLCAM_LOGI_TAG(kLogTag, "Display initialized (%ux%u) on SDA=%d SCL=%d", config.width, config.height, config.sdaPin, config.sclPin);
    return true;
}

void clearDisplay() {
    if (!g_initialized || !g_display) {
        return;
    }

    g_display->clearDisplay();
    g_display->display();
}

void printDisplay(const char *message, uint8_t textSize, int16_t cursorX, int16_t cursorY, bool clear, bool wrapText) {
    if (!g_initialized || !g_display || message == nullptr) {
        return;
    }

    const uint8_t safeSize = textSize == 0 ? 1 : textSize;

    if (clear) {
        g_display->clearDisplay();
    }
    g_display->setCursor(cursorX, cursorY);
    g_display->setTextWrap(wrapText);
    g_display->setTextSize(safeSize);
    g_display->setTextColor(SSD1306_WHITE);
    g_display->println(message);
    g_display->display();
}

void shutdownDisplay() {
    if (!g_initialized || !g_display) {
        return;
    }

    g_display->ssd1306_command(SSD1306_DISPLAYOFF);
    g_display->clearDisplay();
    g_display->display();
    destroyDisplay();
    g_initialized = false;

    PXLCAM_LOGI_TAG(kLogTag, "Display shutdown completed");
}

}  // namespace pxlcam::display
