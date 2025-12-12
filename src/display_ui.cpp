/**
 * @file display_ui.cpp
 * @brief UI/UX system implementation for PXLcam v1.2.0
 * 
 * Elegant, minimalist UI with:
 * - Error screens with timeout
 * - Success animations (blink)
 * - Menu transitions (fade)
 * - Enhanced status bar
 * - Toast notifications
 */

#include "display_ui.h"
#include "display.h"
#include "pxlcam_config.h"
#include "logging.h"

#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <cstring>
#include <cstdio>

namespace pxlcam::display {

namespace {

constexpr const char* kLogTag = "ui";

//==============================================================================
// Internal State
//==============================================================================

struct UIState {
    // Error state
    bool errorVisible = false;
    char errorTitle[24] = {0};
    char errorMessage[48] = {0};
    uint32_t errorExpireMs = 0;
    
    // Toast state
    bool toastVisible = false;
    char toastMessage[32] = {0};
    ToastType toastType = ToastType::Info;
    uint32_t toastExpireMs = 0;
    
    // Progress state
    bool progressVisible = false;
    char progressTitle[24] = {0};
    uint8_t progressValue = 0;
    
    // Animation state
    uint8_t animFrame = 0;
    uint32_t lastAnimMs = 0;
};

UIState g_state;

//==============================================================================
// 8x8 Icon Bitmaps
//==============================================================================

// SD card icon
const uint8_t kIconSD[] PROGMEM = {
    0b01111100,
    0b01000110,
    0b01000010,
    0b01111110,
    0b01000010,
    0b01000010,
    0b01000010,
    0b01111110
};

// SD missing icon (X)
const uint8_t kIconNoSD[] PROGMEM = {
    0b10000001,
    0b01000010,
    0b00100100,
    0b00011000,
    0b00011000,
    0b00100100,
    0b01000010,
    0b10000001
};

// Memory OK (bar graph high)
const uint8_t kIconMemOk[] PROGMEM = {
    0b00000000,
    0b01111110,
    0b01111110,
    0b01111110,
    0b01111110,
    0b01111110,
    0b01111110,
    0b00000000
};

// Memory Low (bar graph low)
const uint8_t kIconMemLow[] PROGMEM = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b01111110,
    0b01111110,
    0b01111110,
    0b00000000
};

// Checkmark (success)
const uint8_t kIconCheck[] PROGMEM = {
    0b00000000,
    0b00000001,
    0b00000010,
    0b00000100,
    0b10001000,
    0b01010000,
    0b00100000,
    0b00000000
};

// Error X
const uint8_t kIconError[] PROGMEM = {
    0b00000000,
    0b01000010,
    0b00100100,
    0b00011000,
    0b00011000,
    0b00100100,
    0b01000010,
    0b00000000
};

void drawIcon8x8(int16_t x, int16_t y, const uint8_t* bitmap) {
    Adafruit_SSD1306* disp = getDisplayPtr();
    if (!disp || !bitmap) return;
    
    for (int row = 0; row < 8; ++row) {
        uint8_t bits = pgm_read_byte(&bitmap[row]);
        for (int col = 0; col < 8; ++col) {
            if (bits & (0x80 >> col)) {
                disp->drawPixel(x + col, y + row, SSD1306_WHITE);
            }
        }
    }
}

//==============================================================================
// Drawing Helpers
//==============================================================================

void drawRoundedBox(int16_t x, int16_t y, int16_t w, int16_t h, bool filled = false) {
    Adafruit_SSD1306* disp = getDisplayPtr();
    if (!disp) return;
    
    if (filled) {
        disp->fillRoundRect(x, y, w, h, 2, SSD1306_WHITE);
    } else {
        disp->drawRoundRect(x, y, w, h, 2, SSD1306_WHITE);
    }
}

void clearArea(int16_t x, int16_t y, int16_t w, int16_t h) {
    Adafruit_SSD1306* disp = getDisplayPtr();
    if (disp) {
        disp->fillRect(x, y, w, h, SSD1306_BLACK);
    }
}

}  // anonymous namespace

//==============================================================================
// Core UI Functions
//==============================================================================

void initUI() {
    memset(&g_state, 0, sizeof(g_state));
    PXLCAM_LOGI_TAG(kLogTag, "UI system initialized (v1.2.0)");
}

void updateUI() {
    uint32_t now = millis();
    
    // Check error timeout
    if (g_state.errorVisible && g_state.errorExpireMs > 0 && now >= g_state.errorExpireMs) {
        clearError();
        PXLCAM_LOGD_TAG(kLogTag, "Error dismissed (timeout)");
    }
    
    // Check toast timeout
    if (g_state.toastVisible && g_state.toastExpireMs > 0 && now >= g_state.toastExpireMs) {
        clearToast();
        PXLCAM_LOGD_TAG(kLogTag, "Toast dismissed (timeout)");
    }
}

bool hasActiveOverlay() {
    return g_state.errorVisible || g_state.toastVisible || g_state.progressVisible;
}

//==============================================================================
// Error Screens
//==============================================================================

void showError(const char* message, uint32_t timeoutMs) {
    showErrorFull("ERRO", message, timeoutMs);
}

void showErrorFull(const char* title, const char* message, uint32_t timeoutMs) {
    Adafruit_SSD1306* disp = getDisplayPtr();
    if (!disp) return;
    
    // Store state
    strncpy(g_state.errorTitle, title ? title : "ERRO", sizeof(g_state.errorTitle) - 1);
    strncpy(g_state.errorMessage, message ? message : "", sizeof(g_state.errorMessage) - 1);
    g_state.errorVisible = true;
    g_state.errorExpireMs = (timeoutMs > 0) ? millis() + timeoutMs : 0;
    
    PXLCAM_LOGI_TAG(kLogTag, "Error: %s - %s", g_state.errorTitle, g_state.errorMessage);
    
    // Draw error screen
    disp->clearDisplay();
    
    // Error box background
    disp->fillRect(4, 8, 120, 48, SSD1306_BLACK);
    disp->drawRect(4, 8, 120, 48, SSD1306_WHITE);
    
    // Error icon
    drawIcon8x8(10, 14, kIconError);
    
    // Title (inverted)
    disp->fillRect(20, 12, 100, 12, SSD1306_WHITE);
    disp->setTextSize(1);
    disp->setTextColor(SSD1306_BLACK);
    disp->setCursor(22, 14);
    disp->print(g_state.errorTitle);
    
    // Message (wrapped if needed)
    disp->setTextColor(SSD1306_WHITE);
    disp->setCursor(10, 30);
    
    // Simple word wrap
    const char* p = g_state.errorMessage;
    int line = 0;
    char lineBuf[20];
    int lineIdx = 0;
    
    while (*p && line < 2) {
        if (*p == '\n' || lineIdx >= 18) {
            lineBuf[lineIdx] = '\0';
            disp->setCursor(10, 30 + line * 10);
            disp->print(lineBuf);
            lineIdx = 0;
            line++;
            if (*p == '\n') p++;
        } else {
            lineBuf[lineIdx++] = *p++;
        }
    }
    if (lineIdx > 0 && line < 2) {
        lineBuf[lineIdx] = '\0';
        disp->setCursor(10, 30 + line * 10);
        disp->print(lineBuf);
    }
    
    disp->display();
}

void clearError() {
    g_state.errorVisible = false;
    g_state.errorExpireMs = 0;
}

bool isErrorVisible() {
    return g_state.errorVisible;
}

//==============================================================================
// Success Animation
//==============================================================================

void showSuccess(const char* message) {
    Adafruit_SSD1306* disp = getDisplayPtr();
    if (!disp) return;
    
    PXLCAM_LOGI_TAG(kLogTag, "Success: %s", message ? message : "OK");
    
    // Quick blink animation (3 flashes, 100ms each)
    for (int i = 0; i < 3; i++) {
        // Flash ON (inverted)
        disp->invertDisplay(true);
        delay(50);
        
        // Flash OFF
        disp->invertDisplay(false);
        delay(50);
    }
    
    // Show success toast
    if (message && message[0]) {
        // Centered success message
        disp->clearDisplay();
        
        // Success icon
        int iconX = 56;
        drawIcon8x8(iconX, 16, kIconCheck);
        
        // Message
        disp->setTextSize(1);
        disp->setTextColor(SSD1306_WHITE);
        int msgLen = strlen(message);
        int msgX = (128 - msgLen * 6) / 2;
        disp->setCursor(msgX > 0 ? msgX : 0, 32);
        disp->print(message);
        
        disp->display();
        delay(800);
    }
}

void showSuccessFile(const char* filename) {
    Adafruit_SSD1306* disp = getDisplayPtr();
    if (!disp) return;
    
    // Quick blink
    for (int i = 0; i < 2; i++) {
        disp->invertDisplay(true);
        delay(50);
        disp->invertDisplay(false);
        delay(50);
    }
    
    // Show saved screen
    disp->clearDisplay();
    
    // Box
    disp->drawRoundRect(8, 8, 112, 48, 4, SSD1306_WHITE);
    
    // Checkmark
    drawIcon8x8(56, 14, kIconCheck);
    
    // "SALVO" text
    disp->setTextSize(1);
    disp->setTextColor(SSD1306_WHITE);
    disp->setCursor(48, 26);
    disp->print("SALVO");
    
    // Filename (truncated if needed)
    if (filename) {
        char truncName[18];
        const char* name = filename;
        
        // Skip path
        const char* lastSlash = strrchr(filename, '/');
        if (lastSlash) name = lastSlash + 1;
        
        strncpy(truncName, name, sizeof(truncName) - 1);
        truncName[sizeof(truncName) - 1] = '\0';
        
        int nameLen = strlen(truncName);
        int nameX = (128 - nameLen * 6) / 2;
        disp->setCursor(nameX > 0 ? nameX : 8, 40);
        disp->print(truncName);
    }
    
    disp->display();
    delay(1200);
}

//==============================================================================
// Menu Transitions
//==============================================================================

void transitionFadeIn(uint8_t steps, uint16_t delayMs) {
    Adafruit_SSD1306* disp = getDisplayPtr();
    if (!disp || steps == 0) return;
    
    // Start with black screen
    disp->clearDisplay();
    disp->display();
    delay(delayMs);
    
    // Gradually reveal by drawing random pixels
    // Simple fade simulation using contrast
    for (uint8_t i = 0; i < steps; i++) {
        // Draw some pattern
        for (int y = 0; y < 64; y += (steps - i)) {
            disp->drawFastHLine(0, y, 128, SSD1306_WHITE);
        }
        disp->display();
        delay(delayMs);
    }
}

void transitionFadeOut(uint8_t steps, uint16_t delayMs) {
    Adafruit_SSD1306* disp = getDisplayPtr();
    if (!disp || steps == 0) return;
    
    // Gradually hide content
    for (uint8_t i = steps; i > 0; i--) {
        for (int y = 0; y < 64; y += i) {
            disp->drawFastHLine(0, y, 128, SSD1306_BLACK);
        }
        disp->display();
        delay(delayMs);
    }
    
    // Final clear
    disp->clearDisplay();
    disp->display();
}

void transitionSlide() {
    Adafruit_SSD1306* disp = getDisplayPtr();
    if (!disp) return;
    
    // Quick slide effect (scroll content off)
    for (int i = 0; i < 16; i += 4) {
        disp->startscrollleft(0, 7);
        delay(20);
    }
    disp->stopscroll();
    disp->clearDisplay();
    disp->display();
}

//==============================================================================
// Status Bar
//==============================================================================

void drawStatusBar(int fps, bool sdPresent, PreviewMode mode, uint16_t freeHeapKb) {
    Adafruit_SSD1306* disp = getDisplayPtr();
    if (!disp) return;
    
    // Clear status bar area
    disp->fillRect(0, UILayout::StatusBarY, 128, UILayout::StatusBarHeight, SSD1306_BLACK);
    
    int x = 2;
    disp->setTextSize(1);
    disp->setTextColor(SSD1306_WHITE);
    
    // Mode indicator (boxed)
    char modeChar = getModeChar(mode);
    disp->drawRect(x, 0, 10, 9, SSD1306_WHITE);
    disp->setCursor(x + 2, 1);
    disp->print(modeChar);
    x += 14;
    
    // SD indicator
    if (sdPresent) {
        drawIcon8x8(x, 1, kIconSD);
    } else {
        drawIcon8x8(x, 1, kIconNoSD);
    }
    x += 12;
    
    // Memory indicator (simple bar)
    bool memOk = freeHeapKb > 50;  // >50KB is OK
    if (memOk) {
        drawIcon8x8(x, 1, kIconMemOk);
    } else {
        drawIcon8x8(x, 1, kIconMemLow);
    }
    x += 12;
    
    // FPS (right-aligned)
    char buf[12];
    snprintf(buf, sizeof(buf), "%dfps", fps);
    int textW = strlen(buf) * 6;
    disp->setCursor(128 - textW - 2, 1);
    disp->print(buf);
    
    // Separator line
    disp->drawFastHLine(0, UILayout::StatusBarHeight, 128, SSD1306_WHITE);
}

void drawStatusBarSimple(PreviewMode mode, const char* indicator) {
    Adafruit_SSD1306* disp = getDisplayPtr();
    if (!disp) return;
    
    disp->fillRect(0, 0, 128, 10, SSD1306_BLACK);
    
    disp->setTextSize(1);
    disp->setTextColor(SSD1306_WHITE);
    
    // Mode
    disp->setCursor(2, 1);
    disp->print('[');
    disp->print(getModeChar(mode));
    disp->print(']');
    
    // Right indicator
    if (indicator && indicator[0]) {
        int len = strlen(indicator);
        disp->setCursor(128 - len * 6 - 2, 1);
        disp->print(indicator);
    }
    
    disp->drawFastHLine(0, 10, 128, SSD1306_WHITE);
}

void drawModeIndicator(PreviewMode mode) {
    Adafruit_SSD1306* disp = getDisplayPtr();
    if (!disp) return;
    
    char modeChar = getModeChar(mode);
    
    // Draw in top-left corner
    disp->fillRect(0, 0, 12, 10, SSD1306_BLACK);
    disp->drawRect(0, 0, 12, 10, SSD1306_WHITE);
    disp->setTextSize(1);
    disp->setTextColor(SSD1306_WHITE);
    disp->setCursor(3, 1);
    disp->print(modeChar);
}

//==============================================================================
// Toast Notifications
//==============================================================================

void showToast(const char* message, ToastType type, uint32_t timeoutMs) {
    Adafruit_SSD1306* disp = getDisplayPtr();
    if (!disp || !message) return;
    
    strncpy(g_state.toastMessage, message, sizeof(g_state.toastMessage) - 1);
    g_state.toastType = type;
    g_state.toastVisible = true;
    g_state.toastExpireMs = (timeoutMs > 0) ? millis() + timeoutMs : 0;
    
    // Draw toast box (centered)
    int msgLen = strlen(message);
    int boxW = msgLen * 6 + 16;
    if (boxW > 120) boxW = 120;
    int boxX = (128 - boxW) / 2;
    
    disp->fillRect(boxX, UILayout::ToastY, boxW, UILayout::ToastH, SSD1306_BLACK);
    disp->drawRoundRect(boxX, UILayout::ToastY, boxW, UILayout::ToastH, 3, SSD1306_WHITE);
    
    // Icon based on type
    const uint8_t* icon = nullptr;
    switch (type) {
        case ToastType::Success: icon = kIconCheck; break;
        case ToastType::Error:   icon = kIconError; break;
        default: break;
    }
    
    int textX = boxX + 8;
    if (icon) {
        drawIcon8x8(boxX + 4, UILayout::ToastY + 8, icon);
        textX = boxX + 16;
    }
    
    // Message
    disp->setTextSize(1);
    disp->setTextColor(SSD1306_WHITE);
    disp->setCursor(textX, UILayout::ToastY + 8);
    disp->print(message);
    
    disp->display();
}

void clearToast() {
    g_state.toastVisible = false;
    g_state.toastExpireMs = 0;
    
    // Clear toast area
    Adafruit_SSD1306* disp = getDisplayPtr();
    if (disp) {
        disp->fillRect(0, UILayout::ToastY, 128, UILayout::ToastH, SSD1306_BLACK);
    }
}

//==============================================================================
// Progress & Loading
//==============================================================================

void showProgress(const char* title, uint8_t progress) {
    Adafruit_SSD1306* disp = getDisplayPtr();
    if (!disp) return;
    
    strncpy(g_state.progressTitle, title ? title : "", sizeof(g_state.progressTitle) - 1);
    g_state.progressValue = progress > 100 ? 100 : progress;
    g_state.progressVisible = true;
    
    // Draw progress box
    disp->fillRect(10, 20, 108, 24, SSD1306_BLACK);
    disp->drawRect(10, 20, 108, 24, SSD1306_WHITE);
    
    // Title
    disp->setTextSize(1);
    disp->setTextColor(SSD1306_WHITE);
    disp->setCursor(14, 23);
    disp->print(g_state.progressTitle);
    
    // Progress bar
    disp->drawRect(14, 34, 100, 6, SSD1306_WHITE);
    int barW = (progress * 96) / 100;
    if (barW > 0) {
        disp->fillRect(16, 36, barW, 2, SSD1306_WHITE);
    }
    
    disp->display();
}

void showLoading(const char* message) {
    Adafruit_SSD1306* disp = getDisplayPtr();
    if (!disp) return;
    
    g_state.progressVisible = true;
    
    disp->fillRect(10, 20, 108, 24, SSD1306_BLACK);
    disp->drawRect(10, 20, 108, 24, SSD1306_WHITE);
    
    // Animated dots
    disp->setTextSize(1);
    disp->setTextColor(SSD1306_WHITE);
    disp->setCursor(14, 28);
    disp->print(message ? message : "Aguarde");
    
    // Spinning indicator
    uint8_t dots = (millis() / 300) % 4;
    for (uint8_t i = 0; i < dots; i++) {
        disp->print('.');
    }
    
    disp->display();
}

void clearProgress() {
    g_state.progressVisible = false;
    
    Adafruit_SSD1306* disp = getDisplayPtr();
    if (disp) {
        disp->fillRect(10, 20, 108, 24, SSD1306_BLACK);
    }
}

//==============================================================================
// Preview Area
//==============================================================================

void drawPreviewBitmap(const uint8_t* packedBitmap, int w, int h) {
    Adafruit_SSD1306* disp = getDisplayPtr();
    if (!disp || !packedBitmap) return;
    
    const int offsetX = UILayout::PreviewX;
    const int offsetY = UILayout::PreviewY;
    
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
    
    disp->drawRect(UILayout::PreviewX - 1, UILayout::PreviewY - 1,
                   UILayout::PreviewW + 2, UILayout::PreviewH + 2, SSD1306_WHITE);
}

void clearPreviewArea() {
    Adafruit_SSD1306* disp = getDisplayPtr();
    if (!disp) return;
    
    disp->fillRect(UILayout::PreviewX, UILayout::PreviewY,
                   UILayout::PreviewW, UILayout::PreviewH, SSD1306_BLACK);
}

//==============================================================================
// Utility Functions
//==============================================================================

void drawIcon(int16_t x, int16_t y, IconId iconId) {
    switch (iconId) {
        case IconId::SD_Present:   drawIcon8x8(x, y, kIconSD); break;
        case IconId::SD_Empty:     drawIcon8x8(x, y, kIconNoSD); break;
        case IconId::MemOk:        drawIcon8x8(x, y, kIconMemOk); break;
        case IconId::MemLow:       drawIcon8x8(x, y, kIconMemLow); break;
        default: break;
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

void drawCenteredText(int16_t y, const char* text) {
    if (!text) return;
    int len = strlen(text);
    int x = (128 - len * 6) / 2;
    drawSmallText(x > 0 ? x : 0, y, text);
}

void drawHintBar(const char* hintText) {
    Adafruit_SSD1306* disp = getDisplayPtr();
    if (!disp) return;
    
    disp->fillRect(0, UILayout::HintBarY, 128, UILayout::HintBarHeight, SSD1306_BLACK);
    disp->drawFastHLine(0, UILayout::HintBarY - 1, 128, SSD1306_WHITE);
    
    if (hintText && hintText[0]) {
        disp->setTextSize(1);
        disp->setTextColor(SSD1306_WHITE);
        disp->setCursor(2, UILayout::HintBarY + 1);
        disp->print(hintText);
    }
}

void swapDisplayBuffers() {
    Adafruit_SSD1306* disp = getDisplayPtr();
    if (disp) {
        disp->display();
    }
}

char getModeChar(PreviewMode mode) {
    switch (mode) {
        case PreviewMode::Normal:  return 'N';
        case PreviewMode::GameBoy: return 'G';
        case PreviewMode::Night:   return 'X';
        default:                   return '?';
    }
}

const char* getModeName(PreviewMode mode) {
    switch (mode) {
        case PreviewMode::Normal:  return "Normal";
        case PreviewMode::GameBoy: return "GameBoy";
        case PreviewMode::Night:   return "Night";
        default:                   return "?";
    }
}

void refreshFullUI(int fps, bool sdPresent, PreviewMode mode, const char* hint) {
    Adafruit_SSD1306* disp = getDisplayPtr();
    if (!disp) return;
    
    // Get free heap for status bar
    uint16_t freeHeapKb = ESP.getFreeHeap() / 1024;
    
    drawStatusBar(fps, sdPresent, mode, freeHeapKb);
    drawStatusIndicator();  // v1.2.0: status indicator
    drawPreviewFrame();
    drawHintBar(hint);
}

//==============================================================================
// Quick Feedback (v1.2.0)
//==============================================================================

namespace {
    bool g_quickFeedbackVisible = false;
    char g_quickFeedbackMsg[24] = {0};
    uint32_t g_quickFeedbackExpireMs = 0;
    StatusIndicator g_statusIndicator = StatusIndicator::Ready;
}

void showQuickFeedback(const char* message, uint32_t timeoutMs) {
    if (!message) return;
    
    strncpy(g_quickFeedbackMsg, message, sizeof(g_quickFeedbackMsg) - 1);
    g_quickFeedbackMsg[sizeof(g_quickFeedbackMsg) - 1] = '\0';
    g_quickFeedbackExpireMs = millis() + timeoutMs;
    g_quickFeedbackVisible = true;
    
    // Draw immediately
    Adafruit_SSD1306* disp = getDisplayPtr();
    if (disp) {
        // Position: top-right, below status bar
        int len = strlen(g_quickFeedbackMsg);
        int x = 128 - (len * 6) - 2;
        int y = UILayout::StatusBarHeight + 2;
        
        // Clear area and draw
        disp->fillRect(x - 2, y - 1, len * 6 + 4, 10, SSD1306_BLACK);
        disp->drawRect(x - 2, y - 1, len * 6 + 4, 10, SSD1306_WHITE);
        disp->setTextSize(1);
        disp->setTextColor(SSD1306_WHITE);
        disp->setCursor(x, y);
        disp->print(g_quickFeedbackMsg);
        disp->display();
    }
    
    PXLCAM_LOGI_TAG("ui", "Feedback: %s", message);
}

void showModeFeedback(const char* modeName) {
    char buf[24];
    snprintf(buf, sizeof(buf), "Style: %s", modeName);
    showQuickFeedback(buf, 1500);
}

void showSavedFeedback() {
    showQuickFeedback("Saved!", 1000);
}

void clearQuickFeedback() {
    g_quickFeedbackVisible = false;
    g_quickFeedbackExpireMs = 0;
}

//==============================================================================
// Status Indicator (v1.2.0 - top-right corner)
//==============================================================================

void setStatusIndicator(StatusIndicator status) {
    g_statusIndicator = status;
}

StatusIndicator getStatusIndicator() {
    return g_statusIndicator;
}

void drawStatusIndicator() {
    Adafruit_SSD1306* disp = getDisplayPtr();
    if (!disp) return;
    
    // Position: top-right corner (x=122, y=2)
    constexpr int kIndicatorX = 122;
    constexpr int kIndicatorY = 2;
    constexpr int kIndicatorR = 3;
    
    // Clear indicator area
    disp->fillRect(kIndicatorX - kIndicatorR, kIndicatorY - kIndicatorR, 
                   kIndicatorR * 2 + 2, kIndicatorR * 2 + 2, SSD1306_BLACK);
    
    switch (g_statusIndicator) {
        case StatusIndicator::Ready:
            // Filled circle (ready)
            disp->fillCircle(kIndicatorX, kIndicatorY + 2, kIndicatorR, SSD1306_WHITE);
            break;
            
        case StatusIndicator::Busy:
            // Half-filled circle (busy)
            disp->drawCircle(kIndicatorX, kIndicatorY + 2, kIndicatorR, SSD1306_WHITE);
            disp->fillRect(kIndicatorX - kIndicatorR, kIndicatorY + 2, 
                          kIndicatorR * 2 + 1, kIndicatorR + 1, SSD1306_WHITE);
            break;
            
        case StatusIndicator::Error:
            // X mark (error)
            disp->drawLine(kIndicatorX - 2, kIndicatorY, kIndicatorX + 2, kIndicatorY + 4, SSD1306_WHITE);
            disp->drawLine(kIndicatorX + 2, kIndicatorY, kIndicatorX - 2, kIndicatorY + 4, SSD1306_WHITE);
            break;
            
        case StatusIndicator::Recording: {
            // Blinking circle (recording)
            static bool blinkState = false;
            static uint32_t lastBlink = 0;
            if (millis() - lastBlink > 300) {
                blinkState = !blinkState;
                lastBlink = millis();
            }
            if (blinkState) {
                disp->fillCircle(kIndicatorX, kIndicatorY + 2, kIndicatorR, SSD1306_WHITE);
            } else {
                disp->drawCircle(kIndicatorX, kIndicatorY + 2, kIndicatorR, SSD1306_WHITE);
            }
            break;
        }
            
        case StatusIndicator::None:
        default:
            // No indicator
            break;
    }
    
    // Check quick feedback timeout
    if (g_quickFeedbackVisible && millis() > g_quickFeedbackExpireMs) {
        clearQuickFeedback();
    }
}

}  // namespace pxlcam::display
