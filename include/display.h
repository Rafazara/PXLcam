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

bool initDisplay(const DisplayConfig &config);
void clearDisplay();
void printDisplay(const char *message, uint8_t textSize = 1, int16_t cursorX = 0, int16_t cursorY = 0, bool clear = true, bool wrapText = true);
void shutdownDisplay();

}  // namespace pxlcam::display
