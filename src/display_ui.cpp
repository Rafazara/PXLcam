/**
 * @file display_ui.cpp
 * @brief UI overlay system for PXLcam v1.1.0
 */

#include "display_ui.h"
#include "display.h"
#include "pxlcam_config.h"
#include "logging.h"

#include <Adafruit_SSD1306.h>
#include <cstring>
#include <cstdio>

namespace pxlcam::display {

namespace {
constexpr const char* kLogTag = "pxlcam-ui";

// 8x8 icon bitmaps (1-bit packed, row-major)
// SD card icon
const uint8_t kIconSD[] = {
    0b01111100,
    0b01000110,
    0b01000010,
    0b01111110,
    0b01000010,
    0b01000010,
    0b01000010,
    0b01111110
};

// Battery icons (different fill levels)
const uint8_t kIconBatteryEmpty[] = {
    0b01111110,
    0b11000001,
    0b10000001,
    0b10000001,
    0b10000001,
    0b10000001,
    0b10000001,
    0b11111111
};

const uint8_t kIconBatteryFull[] = {
    0b01111110,
    0b11000001,
    0b10111101,
    0b10111101,
    0b10111101,
    0b10111101,
    0b10111101,
    0b11111111
};

// Mode indicators (A/G/N as simple letters - we'll use text instead)

void drawIcon8x8(int16_t x, int16_t y, const uint8_t* bitmap) {
    Adafruit_SSD1306* disp = getDisplayPtr();
    if (!disp || !bitmap) return;
    
    for (int row = 0; row < 8; ++row) {
        uint8_t bits = bitmap[row];
        for (int col = 0; col < 8; ++col) {
            if (bits & (0x80 >> col)) {
                disp->drawPixel(x + col, y + row, SSD1306_WHITE);
            }
        }
    }
}

}  // anonymous namespace

void initUI() {
    PXLCAM_LOGI_TAG(kLogTag, "UI system initialized");
}

void drawIcon(int16_t x, int16_t y, IconId iconId) {
    const uint8_t* icon = nullptr;
    
    switch (iconId) {
        case IconId::SD_Present:
            icon = kIconSD;
            break;
        case IconId::SD_Empty:
            // Draw empty rect
            {
                Adafruit_SSD1306* disp = getDisplayPtr();
                if (disp) disp->drawRect(x, y, 8, 8, SSD1306_WHITE);
            }
            return;
        case IconId::Battery0:
        case IconId::Battery25:
            icon = kIconBatteryEmpty;
            break;
        case IconId::Battery50:
        case IconId::Battery75:
        case IconId::Battery100:
            icon = kIconBatteryFull;
            break;
        default:
            return;
    }
    
    if (icon) {
        drawIcon8x8(x, y, icon);
    }
}

void drawStatusBar(int fps, bool sdPresent, uint8_t batteryPct, PreviewMode mode) {
    Adafruit_SSD1306* disp = getDisplayPtr();
    if (!disp) return;
    
    // Clear status bar area
    disp->fillRect(0, UILayout::StatusBarY, 128, UILayout::StatusBarHeight, SSD1306_BLACK);
    
    int x = 2;
    
    // SD icon
    drawIcon(x, 1, sdPresent ? IconId::SD_Present : IconId::SD_Empty);
    x += UILayout::IconSpacing;
    
    // Battery icon
    IconId battIcon = IconId::Battery0;
    if (batteryPct >= 75) battIcon = IconId::Battery100;
    else if (batteryPct >= 50) battIcon = IconId::Battery75;
    else if (batteryPct >= 25) battIcon = IconId::Battery50;
    else if (batteryPct > 0) battIcon = IconId::Battery25;
    drawIcon(x, 1, battIcon);
    x += UILayout::IconSpacing;
    
    // Mode character
    disp->setTextSize(1);
    disp->setTextColor(SSD1306_WHITE);
    disp->setCursor(x, 1);
    disp->print(getModeName(mode)[0]);  // First letter: A/G/N
    x += 10;
    
    // FPS (right-aligned)
    char buf[8];
    snprintf(buf, sizeof(buf), "%dfps", fps);
    int textW = strlen(buf) * 6;
    disp->setCursor(128 - textW - 2, 1);
    disp->print(buf);
}

void drawPreviewBitmap(const uint8_t* packedBitmap, int w, int h) {
    Adafruit_SSD1306* disp = getDisplayPtr();
    if (!disp || !packedBitmap) return;
    
    const int offsetX = UILayout::PreviewX;
    const int offsetY = UILayout::PreviewY;
    
    // Draw packed 1-bit bitmap
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const int idx = y * w + x;
            const int byteIdx = idx >> 3;
            const int bitIdx = 7 - (idx & 7);
            const bool pixel = (packedBitmap[byteIdx] >> bitIdx) & 1;
            disp->drawPixel(offsetX + x, offsetY + y, pixel ? SSD1306_WHITE : SSD1306_BLACK);
        }
    }
}

void drawPreviewFrame() {
    Adafruit_SSD1306* disp = getDisplayPtr();
    if (!disp) return;
    
    // Draw border around preview area
    disp->drawRect(UILayout::PreviewX - 1, UILayout::PreviewY - 1, 
                   UILayout::PreviewW + 2, UILayout::PreviewH + 2, SSD1306_WHITE);
}

void swapDisplayBuffers() {
    Adafruit_SSD1306* disp = getDisplayPtr();
    if (disp) {
        disp->display();
    }
}

void drawSmallText(int16_t x, int16_t y, const char* text) {
    Adafruit_SSD1306* disp = getDisplayPtr();
    if (!disp || !text) return;
    
    disp->setTextSize(1);
    disp->setTextColor(SSD1306_WHITE);
    disp->setCursor(x, y);
    disp->print(text);
}

void drawHintBar(const char* hintText) {
    Adafruit_SSD1306* disp = getDisplayPtr();
    if (!disp) return;
    
    // Clear hint bar area
    disp->fillRect(0, UILayout::HintBarY, 128, UILayout::HintBarHeight, SSD1306_BLACK);
    
    if (hintText && hintText[0]) {
        disp->setTextSize(1);
        disp->setTextColor(SSD1306_WHITE);
        disp->setCursor(2, UILayout::HintBarY + 1);
        disp->print(hintText);
    }
}

void clearPreviewArea() {
    Adafruit_SSD1306* disp = getDisplayPtr();
    if (!disp) return;
    
    disp->fillRect(UILayout::PreviewX, UILayout::PreviewY, 
                   UILayout::PreviewW, UILayout::PreviewH, SSD1306_BLACK);
}

void refreshFullUI(int fps, bool sdPresent, uint8_t batteryPct, PreviewMode mode, const char* hint) {
    Adafruit_SSD1306* disp = getDisplayPtr();
    if (!disp) return;
    
    drawStatusBar(fps, sdPresent, batteryPct, mode);
    drawPreviewFrame();
    drawHintBar(hint);
}

const char* getModeName(PreviewMode mode) {
    switch (mode) {
        case PreviewMode::Auto:    return "Auto";
        case PreviewMode::GameBoy: return "GB";
        case PreviewMode::Night:   return "Night";
        default:                   return "?";
    }
}

}  // namespace pxlcam::display
