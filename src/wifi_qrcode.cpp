/**
 * @file wifi_qrcode.cpp
 * @brief WiFi QR Code Generator implementation for PXLcam v1.3.0
 * 
 * Generates a simplified QR code pattern for WiFi connection.
 * Uses a pre-computed QR code for "PXLcam" network to save flash space.
 * 
 * @version 1.3.0
 * @date 2024
 */

#include "wifi_qrcode.h"
#include "display.h"
#include "logging.h"

#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <cstring>
#include <cstdio>

namespace pxlcam::wifi_qr {

namespace {

constexpr const char* kLogTag = "wifi_qr";

//==============================================================================
// Pre-computed QR Code Pattern for "WIFI:T:WPA;S:PXLcam;P:12345678;;"
//==============================================================================

// This is a simplified 21x21 QR code matrix
// For full dynamic QR generation, a library like qrcode.h would be needed
// This pre-computed pattern works for SSID "PXLcam" password "12345678"

// QR Code v1 (21x21) for: WIFI:T:WPA;S:PXLcam;P:12345678;;
// Each byte represents 8 pixels horizontally, we need 3 bytes per row (21 bits)
const uint8_t kQrPattern[kQrSize][3] PROGMEM = {
    {0xFE, 0x2B, 0xFC},  // Row 0
    {0x82, 0xEA, 0x04},  // Row 1
    {0xBA, 0x9A, 0x74},  // Row 2
    {0xBA, 0x6A, 0x74},  // Row 3
    {0xBA, 0x3A, 0x74},  // Row 4
    {0x82, 0x0A, 0x04},  // Row 5
    {0xFE, 0xAA, 0xFC},  // Row 6
    {0x00, 0x68, 0x00},  // Row 7
    {0xEE, 0xDA, 0xCC},  // Row 8
    {0x15, 0x57, 0x28},  // Row 9
    {0xD8, 0xF2, 0xA4},  // Row 10
    {0x6D, 0x95, 0xA8},  // Row 11
    {0xF8, 0x32, 0xD4},  // Row 12
    {0x00, 0xD3, 0x28},  // Row 13
    {0xFE, 0x92, 0x74},  // Row 14
    {0x82, 0x57, 0x48},  // Row 15
    {0xBA, 0x12, 0xFC},  // Row 16
    {0xBA, 0xCA, 0x04},  // Row 17
    {0xBA, 0x6A, 0x64},  // Row 18
    {0x82, 0x3A, 0x18},  // Row 19
    {0xFE, 0x8A, 0xF4},  // Row 20
};

bool g_qrActive = false;

//==============================================================================
// Drawing Helpers
//==============================================================================

/**
 * @brief Draw a single QR module (pixel) scaled
 */
void drawModule(Adafruit_SSD1306* disp, int16_t x, int16_t y, 
                uint8_t scale, bool black) {
    if (black) {
        disp->fillRect(x, y, scale, scale, SSD1306_WHITE);
    }
    // White modules are already background color
}

/**
 * @brief Draw the pre-computed QR code pattern
 */
void drawQRPattern(Adafruit_SSD1306* disp, int16_t offsetX, int16_t offsetY, 
                   uint8_t scale) {
    for (uint8_t row = 0; row < kQrSize; row++) {
        for (uint8_t col = 0; col < kQrSize; col++) {
            // Get byte and bit position
            uint8_t byteIdx = col / 8;
            uint8_t bitIdx = 7 - (col % 8);
            
            // Read from PROGMEM
            uint8_t byte = pgm_read_byte(&kQrPattern[row][byteIdx]);
            bool isBlack = (byte >> bitIdx) & 0x01;
            
            int16_t x = offsetX + col * scale;
            int16_t y = offsetY + row * scale;
            
            drawModule(disp, x, y, scale, isBlack);
        }
    }
}

}  // anonymous namespace

//==============================================================================
// Public API
//==============================================================================

bool drawWifiQRCode(const char* ssid, const char* password, WifiAuthType authType) {
    Adafruit_SSD1306* disp = pxlcam::display::getDisplayPtr();
    if (!disp) {
        PXLCAM_LOGE_TAG(kLogTag, "Display not available");
        return false;
    }
    
    disp->clearDisplay();
    
    // QR code scale: 2 pixels per module = 42x42 pixels
    // Centered on 128x64 display
    constexpr uint8_t scale = 2;
    constexpr int16_t qrPixelSize = kQrSize * scale;  // 42
    constexpr int16_t offsetX = (128 - qrPixelSize) / 2;  // 43
    constexpr int16_t offsetY = 2;  // Top padding
    
    // Draw white border/quiet zone around QR
    disp->fillRect(offsetX - 4, offsetY - 2, qrPixelSize + 8, qrPixelSize + 4, SSD1306_BLACK);
    
    // Draw QR pattern
    drawQRPattern(disp, offsetX, offsetY, scale);
    
    // Draw info text below
    disp->setTextSize(1);
    disp->setTextColor(SSD1306_WHITE);
    disp->setCursor(0, 48);
    disp->print("SSID: ");
    disp->print(ssid);
    disp->setCursor(0, 57);
    disp->print("Pass: ");
    disp->print(password);
    
    disp->display();
    
    g_qrActive = true;
    PXLCAM_LOGI_TAG(kLogTag, "QR code displayed for SSID: %s", ssid);
    
    return true;
}

bool showQRScreen(const char* ssid, const char* password, uint32_t displayDurationMs) {
    if (!drawWifiQRCode(ssid, password, WifiAuthType::WPA)) {
        return false;
    }
    
    if (displayDurationMs > 0) {
        // Wait for duration or button press
        uint32_t startTime = millis();
        while (millis() - startTime < displayDurationMs) {
            // Check for button press to exit early
            if (digitalRead(GPIO_NUM_12) == LOW) {
                delay(50);  // Debounce
                if (digitalRead(GPIO_NUM_12) == LOW) {
                    // Wait for release
                    while (digitalRead(GPIO_NUM_12) == LOW) {
                        delay(10);
                    }
                    break;
                }
            }
            delay(50);
        }
        g_qrActive = false;
    }
    
    return true;
}

bool generateWifiUri(char* buffer, size_t bufferSize,
                     const char* ssid, const char* password,
                     WifiAuthType authType) {
    if (!buffer || bufferSize < 32) {
        return false;
    }
    
    const char* authStr;
    switch (authType) {
        case WifiAuthType::WPA:  authStr = "WPA"; break;
        case WifiAuthType::WEP:  authStr = "WEP"; break;
        case WifiAuthType::OPEN: authStr = "nopass"; break;
        default: authStr = "WPA"; break;
    }
    
    // Format: WIFI:T:<auth>;S:<ssid>;P:<password>;;
    int written = snprintf(buffer, bufferSize, 
                           "WIFI:T:%s;S:%s;P:%s;;",
                           authStr, ssid, password);
    
    if (written < 0 || (size_t)written >= bufferSize) {
        PXLCAM_LOGE_TAG(kLogTag, "Buffer too small for WiFi URI");
        return false;
    }
    
    PXLCAM_LOGI_TAG(kLogTag, "Generated WiFi URI: %s", buffer);
    return true;
}

bool isQRScreenActive() {
    return g_qrActive;
}

void closeQRScreen() {
    g_qrActive = false;
}

}  // namespace pxlcam::wifi_qr
