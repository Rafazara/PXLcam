/**
 * @file timelapse_menu.cpp
 * @brief Timelapse menu implementation for PXLcam v1.3.0
 * 
 * @version 1.3.0
 * @date 2024
 */

#include "timelapse_menu.h"

#if PXLCAM_FEATURE_TIMELAPSE

#include "timelapse.h"
#include "timelapse_settings.h"
#include "display.h"
#include "logging.h"

#include <Arduino.h>
#include <Adafruit_SSD1306.h>

namespace pxlcam::timelapse {

namespace {

constexpr const char* kLogTag = "tl_menu";

// Layout constants (128x64 OLED)
constexpr int16_t kScreenWidth = 128;
constexpr int16_t kScreenHeight = 64;
constexpr int16_t kTitleY = 2;
constexpr int16_t kDividerY = 12;
constexpr int16_t kListStartY = 16;
constexpr int16_t kItemHeight = 12;
constexpr int16_t kIndicatorX = 2;
constexpr int16_t kLabelX = 12;
constexpr int16_t kHintY = 54;

// Menu items
constexpr uint8_t kMenuItemCount = 4;

enum class MenuItem : uint8_t {
    START_STOP = 0,
    INTERVAL = 1,
    MAX_FRAMES = 2,
    BACK = 3
};

// State
bool g_menuActive = false;
uint8_t g_currentIndex = 0;

// Button GPIO (same as main menu)
constexpr gpio_num_t kButtonPin = GPIO_NUM_12;
constexpr uint8_t kButtonActive = LOW;

// Timing
constexpr uint32_t kLongPressMs = 1000;
constexpr uint32_t kAutoCloseMs = 20000;

//==============================================================================
// Internal Helpers
//==============================================================================

bool readButton() {
    return digitalRead(kButtonPin) == kButtonActive;
}

void waitForRelease() {
    while (readButton()) {
        delay(10);
    }
    delay(50);
}

const char* getMenuItemLabel(MenuItem item, bool isRunning) {
    switch (item) {
        case MenuItem::START_STOP:
            return isRunning ? "Parar" : "Iniciar";
        case MenuItem::INTERVAL:
            return "Intervalo";
        case MenuItem::MAX_FRAMES:
            return "Frames";
        case MenuItem::BACK:
            return "Voltar";
        default:
            return "?";
    }
}

void drawMenuItem(Adafruit_SSD1306* disp, uint8_t index, bool selected, bool isRunning) {
    int16_t y = kListStartY + index * kItemHeight;
    MenuItem item = static_cast<MenuItem>(index);
    
    // Clear item area
    disp->fillRect(0, y, kScreenWidth, kItemHeight, SSD1306_BLACK);
    
    // Selection indicator
    disp->setTextColor(SSD1306_WHITE);
    disp->setCursor(kIndicatorX, y + 2);
    disp->print(selected ? ">" : " ");
    
    // Item label
    disp->setCursor(kLabelX, y + 2);
    disp->print(getMenuItemLabel(item, isRunning));
    
    // Value for interval and max frames
    if (item == MenuItem::INTERVAL) {
        disp->setCursor(70, y + 2);
        disp->print(intervalName(getCurrentInterval()));
    } else if (item == MenuItem::MAX_FRAMES) {
        disp->setCursor(70, y + 2);
        disp->print(maxFramesName(getCurrentMaxFrames()));
    }
}

void drawMenu(Adafruit_SSD1306* disp) {
    bool isRunning = pxlcam::TimelapseController::instance().isRunning();
    
    disp->clearDisplay();
    disp->setTextSize(1);
    disp->setTextColor(SSD1306_WHITE);
    
    // Title
    disp->setCursor(28, kTitleY);
    disp->print("TIMELAPSE");
    
    // Divider
    disp->drawFastHLine(0, kDividerY, kScreenWidth, SSD1306_WHITE);
    
    // Draw items
    for (uint8_t i = 0; i < kMenuItemCount; i++) {
        drawMenuItem(disp, i, i == g_currentIndex, isRunning);
    }
    
    // Hint bar
    disp->drawFastHLine(0, kHintY - 2, kScreenWidth, SSD1306_WHITE);
    disp->setCursor(2, kHintY);
    disp->print("Tap:nav  Hold:OK");
    
    disp->display();
}

bool handleInput(uint32_t& pressStartMs, bool& wasPressed) {
    bool isPressed = readButton();
    uint32_t now = millis();
    
    if (isPressed && !wasPressed) {
        pressStartMs = now;
        wasPressed = true;
    }
    else if (!isPressed && wasPressed) {
        uint32_t holdDuration = now - pressStartMs;
        wasPressed = false;
        
        if (holdDuration >= kLongPressMs) {
            // Long press: SELECT
            return true;
        } else {
            // Short press: NEXT
            g_currentIndex = (g_currentIndex + 1) % kMenuItemCount;
        }
    }
    
    return false;
}

} // anonymous namespace

//==============================================================================
// Public API Implementation
//==============================================================================

void menuInit() {
    settingsInit();
    g_menuActive = false;
    g_currentIndex = 0;
    PXLCAM_LOGI_TAG(kLogTag, "Timelapse menu initialized");
}

MenuResult showMenu() {
    Adafruit_SSD1306* disp = pxlcam::display::getDisplayPtr();
    if (!disp) {
        PXLCAM_LOGE_TAG(kLogTag, "Display not available");
        return MenuResult::CANCELLED;
    }
    
    g_currentIndex = 0;
    g_menuActive = true;
    
    waitForRelease();
    drawMenu(disp);
    
    uint32_t openTime = millis();
    uint32_t pressStartMs = 0;
    bool wasPressed = false;
    MenuResult result = MenuResult::CANCELLED;
    
    bool isRunning = pxlcam::TimelapseController::instance().isRunning();
    
    while (g_menuActive) {
        uint32_t now = millis();
        
        // Auto-close timeout
        if ((now - openTime) >= kAutoCloseMs) {
            g_menuActive = false;
            result = MenuResult::CANCELLED;
            break;
        }
        
        // Handle input
        if (handleInput(pressStartMs, wasPressed)) {
            MenuItem item = static_cast<MenuItem>(g_currentIndex);
            
            switch (item) {
                case MenuItem::START_STOP:
                    result = isRunning ? MenuResult::STOP : MenuResult::START;
                    g_menuActive = false;
                    break;
                    
                case MenuItem::INTERVAL:
                    showIntervalSelect();
                    drawMenu(disp);
                    result = MenuResult::INTERVAL;
                    openTime = millis(); // Reset timeout
                    break;
                    
                case MenuItem::MAX_FRAMES:
                    showMaxFramesSelect();
                    drawMenu(disp);
                    result = MenuResult::MAX_FRAMES;
                    openTime = millis();
                    break;
                    
                case MenuItem::BACK:
                    result = MenuResult::BACK;
                    g_menuActive = false;
                    break;
            }
        }
        
        // Redraw
        drawMenu(disp);
        delay(50);
    }
    
    g_menuActive = false;
    return result;
}

void showIntervalSelect() {
    Adafruit_SSD1306* disp = pxlcam::display::getDisplayPtr();
    if (!disp) return;
    
    waitForRelease();
    
    TimelapseInterval current = getCurrentInterval();
    uint32_t pressStartMs = 0;
    bool wasPressed = false;
    bool selecting = true;
    
    while (selecting) {
        // Draw interval selection screen
        disp->clearDisplay();
        disp->setTextSize(1);
        disp->setTextColor(SSD1306_WHITE);
        
        disp->setCursor(20, 2);
        disp->print("INTERVALO");
        disp->drawFastHLine(0, 12, kScreenWidth, SSD1306_WHITE);
        
        // Show current value large
        disp->setTextSize(2);
        disp->setCursor(30, 24);
        disp->print(intervalName(current));
        
        disp->setTextSize(1);
        disp->drawFastHLine(0, kHintY - 2, kScreenWidth, SSD1306_WHITE);
        disp->setCursor(2, kHintY);
        disp->print("Tap:+  Hold:OK");
        
        disp->display();
        
        // Handle input
        bool isPressed = readButton();
        uint32_t now = millis();
        
        if (isPressed && !wasPressed) {
            pressStartMs = now;
            wasPressed = true;
        }
        else if (!isPressed && wasPressed) {
            uint32_t holdDuration = now - pressStartMs;
            wasPressed = false;
            
            if (holdDuration >= kLongPressMs) {
                // Confirm selection
                setCurrentInterval(current, true);
                selecting = false;
            } else {
                // Next interval
                current = nextInterval(current);
            }
        }
        
        delay(50);
    }
    
    waitForRelease();
}

void showMaxFramesSelect() {
    Adafruit_SSD1306* disp = pxlcam::display::getDisplayPtr();
    if (!disp) return;
    
    waitForRelease();
    
    MaxFramesOption current = getCurrentMaxFrames();
    uint32_t pressStartMs = 0;
    bool wasPressed = false;
    bool selecting = true;
    
    while (selecting) {
        // Draw max frames selection screen
        disp->clearDisplay();
        disp->setTextSize(1);
        disp->setTextColor(SSD1306_WHITE);
        
        disp->setCursor(20, 2);
        disp->print("MAX FRAMES");
        disp->drawFastHLine(0, 12, kScreenWidth, SSD1306_WHITE);
        
        // Show current value large
        disp->setTextSize(2);
        disp->setCursor(40, 24);
        disp->print(maxFramesName(current));
        
        disp->setTextSize(1);
        disp->drawFastHLine(0, kHintY - 2, kScreenWidth, SSD1306_WHITE);
        disp->setCursor(2, kHintY);
        disp->print("Tap:+  Hold:OK");
        
        disp->display();
        
        // Handle input
        bool isPressed = readButton();
        uint32_t now = millis();
        
        if (isPressed && !wasPressed) {
            pressStartMs = now;
            wasPressed = true;
        }
        else if (!isPressed && wasPressed) {
            uint32_t holdDuration = now - pressStartMs;
            wasPressed = false;
            
            if (holdDuration >= kLongPressMs) {
                // Confirm selection
                setCurrentMaxFrames(current, true);
                selecting = false;
            } else {
                // Next option
                current = nextMaxFrames(current);
            }
        }
        
        delay(50);
    }
    
    waitForRelease();
}

void drawActiveScreen() {
    Adafruit_SSD1306* disp = pxlcam::display::getDisplayPtr();
    if (!disp) return;
    
    TimelapseController& ctrl = TimelapseController::instance();
    TimelapseStatus status = ctrl.getStatus();
    
    disp->clearDisplay();
    disp->setTextSize(1);
    disp->setTextColor(SSD1306_WHITE);
    
    // Title with status
    disp->setCursor(8, 2);
    if (status.paused) {
        disp->print("TIMELAPSE PAUSADO");
    } else {
        disp->print("TIMELAPSE ATIVO");
    }
    disp->drawFastHLine(0, 12, kScreenWidth, SSD1306_WHITE);
    
    // Frame counter
    disp->setCursor(4, 16);
    disp->print("Frames: ");
    disp->print(status.framesCaptured);
    
    uint32_t maxFrames = maxFramesToValue(getCurrentMaxFrames());
    if (maxFrames > 0) {
        disp->print("/");
        disp->print(maxFrames);
    } else {
        disp->print("/inf");
    }
    
    // Progress bar (if max frames set)
    if (maxFrames > 0) {
        uint8_t progress = ctrl.getProgress();
        int16_t barWidth = (kScreenWidth - 8) * progress / 100;
        
        disp->drawRect(4, 28, kScreenWidth - 8, 8, SSD1306_WHITE);
        disp->fillRect(4, 28, barWidth, 8, SSD1306_WHITE);
        
        disp->setCursor(54, 28);
        disp->setTextColor(progress > 50 ? SSD1306_BLACK : SSD1306_WHITE);
        disp->print(progress);
        disp->print("%");
        disp->setTextColor(SSD1306_WHITE);
    }
    
    // Next capture countdown
    char timeBuf[16];
    formatTime(status.nextCaptureMs, timeBuf, sizeof(timeBuf));
    
    disp->setCursor(4, 40);
    disp->print("Proxima: ");
    disp->print(timeBuf);
    
    // Elapsed time
    formatTime(status.elapsedMs, timeBuf, sizeof(timeBuf));
    disp->setCursor(4, 50);
    disp->print("Decorrido: ");
    disp->print(timeBuf);
    
    disp->display();
}

void drawStartScreen(uint32_t intervalMs, uint32_t maxFrames) {
    Adafruit_SSD1306* disp = pxlcam::display::getDisplayPtr();
    if (!disp) return;
    
    char timeBuf[16];
    
    disp->clearDisplay();
    disp->setTextSize(1);
    disp->setTextColor(SSD1306_WHITE);
    
    disp->setCursor(20, 8);
    disp->print("TIMELAPSE ON");
    
    disp->drawFastHLine(0, 20, kScreenWidth, SSD1306_WHITE);
    
    disp->setCursor(4, 26);
    disp->print("Intervalo: ");
    formatTime(intervalMs, timeBuf, sizeof(timeBuf));
    disp->print(timeBuf);
    
    disp->setCursor(4, 38);
    disp->print("Frames: 0/");
    if (maxFrames > 0) {
        disp->print(maxFrames);
    } else {
        disp->print("inf");
    }
    
    disp->setCursor(20, 52);
    disp->print("Iniciando...");
    
    disp->display();
}

void drawStoppedScreen(uint32_t framesCaptured) {
    Adafruit_SSD1306* disp = pxlcam::display::getDisplayPtr();
    if (!disp) return;
    
    disp->clearDisplay();
    disp->setTextSize(1);
    disp->setTextColor(SSD1306_WHITE);
    
    disp->setCursor(4, 12);
    disp->print("TIMELAPSE PARADO");
    
    disp->drawFastHLine(0, 24, kScreenWidth, SSD1306_WHITE);
    
    disp->setTextSize(2);
    disp->setCursor(20, 32);
    disp->print(framesCaptured);
    disp->setTextSize(1);
    disp->setCursor(60, 38);
    disp->print(" frames");
    
    disp->setCursor(30, 52);
    disp->print("salvos!");
    
    disp->display();
}

char* formatTime(uint32_t ms, char* buf, size_t bufSize) {
    if (ms < 1000) {
        snprintf(buf, bufSize, "%lums", ms);
    } else if (ms < 60000) {
        snprintf(buf, bufSize, "%lus", ms / 1000);
    } else if (ms < 3600000) {
        uint32_t mins = ms / 60000;
        uint32_t secs = (ms % 60000) / 1000;
        snprintf(buf, bufSize, "%lum%lus", mins, secs);
    } else {
        uint32_t hours = ms / 3600000;
        uint32_t mins = (ms % 3600000) / 60000;
        snprintf(buf, bufSize, "%luh%lum", hours, mins);
    }
    return buf;
}

bool isMenuActive() {
    return g_menuActive;
}

} // namespace pxlcam::timelapse

#endif // PXLCAM_FEATURE_TIMELAPSE
