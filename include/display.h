#pragma once

#include <stdint.h>

namespace pxlcam::display {

// Configuration for the SSD1306 OLED driven over I2C.
struct DisplayConfig {
    uint16_t width;
    uint16_t height;
    uint8_t rotation;
    int8_t resetPin;
    uint8_t i2cAddress;
    int sdaPin;
    int sclPin;
    uint32_t i2cFrequencyHz;
};

// Returns default configuration for ESP32-CAM SSD1306 setup
inline DisplayConfig makeDefaultDisplayConfig() {
    return DisplayConfig{128, 64, 0, -1, 0x3C, 15, 14, 400000};
}

bool initDisplay(const DisplayConfig &config);
void clearDisplay();
void printDisplay(const char *message, uint8_t textSize = 1, int16_t cursorX = 0, int16_t cursorY = 0, bool clear = true, bool wrapText = true);
void shutdownDisplay();

// Low-level drawing for preview mode
void drawPixel(int16_t x, int16_t y, uint16_t color);
void updateDisplay();

// Renders a 64x64 grayscale image to the OLED (1-bit threshold)
void drawGrayscale64x64(const uint8_t *gray64);

}  // namespace pxlcam::display
