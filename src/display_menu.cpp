/**
 * @file display_menu.cpp
 * @brief Modal menu system implementation for PXLcam v1.2.0
 * 
 * Implements a blocking modal menu with:
 * - Vertical item list with ">" indicator
 * - Short press navigation, long press selection
 * - Fade-in animation
 * - Detailed debug logging
 */

#include "display_menu.h"
#include "display.h"
#include "logging.h"

#include <Arduino.h>
#include <Adafruit_SSD1306.h>

namespace pxlcam::menu {

//==============================================================================
// Constants & Configuration
//==============================================================================

namespace {

constexpr const char* kLogTag = "menu";

// Menu items - order: GameBoy, Night, Normal (as requested)
constexpr uint8_t kItemCount = 3;

struct MenuItem {
    const char* label;      // Display text
    const char* bracket;    // Bracketed format
    MenuResult result;      // Return value
    uint8_t modeValue;      // mode_manager value
};

// Menu items in display order
const MenuItem kMenuItems[kItemCount] = {
    {"GameBoy", "[ GameBoy ]", MODE_GAMEBOY, 1},  // CaptureMode::GameBoy = 1
    {"Night",   "[ Night ]",   MODE_NIGHT,   2},  // CaptureMode::Night = 2
    {"Normal",  "[ Normal ]",  MODE_NORMAL,  0}   // CaptureMode::Normal = 0
};

// Layout constants (128x64 OLED)
constexpr int16_t kScreenWidth = 128;
constexpr int16_t kScreenHeight = 64;
constexpr int16_t kTitleY = 2;
constexpr int16_t kDividerY = 12;
constexpr int16_t kListStartY = 18;
constexpr int16_t kItemHeight = 14;
constexpr int16_t kIndicatorX = 4;
constexpr int16_t kLabelX = 16;
constexpr int16_t kHintY = 54;

// State
MenuConfig g_config;
bool g_isOpen = false;
uint8_t g_currentIndex = 0;
bool g_debugLogging = true;  // Enable by default for v1.2.0 debugging

// Button GPIO (same as AppController)
constexpr gpio_num_t kButtonPin = GPIO_NUM_12;
constexpr uint8_t kButtonActive = LOW;

//==============================================================================
// Internal Helpers
//==============================================================================

/**
 * @brief Log message if debug enabled
 */
void logDebug(const char* format, ...) {
    if (!g_debugLogging) return;
    
    char buf[128];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    
    PXLCAM_LOGI_TAG(kLogTag, "%s", buf);
}

/**
 * @brief Read button state with debounce
 */
bool readButton() {
    return digitalRead(kButtonPin) == kButtonActive;
}

/**
 * @brief Wait for button release
 */
void waitForRelease() {
    while (readButton()) {
        delay(10);
    }
    delay(50);  // Extra debounce
}

/**
 * @brief Draw single menu item
 */
void drawItem(Adafruit_SSD1306* disp, uint8_t index, bool selected) {
    int16_t y = kListStartY + index * kItemHeight;
    
    // Clear item area
    disp->fillRect(0, y - 2, kScreenWidth, kItemHeight, SSD1306_BLACK);
    
    // Draw selection indicator ">"
    disp->setTextColor(SSD1306_WHITE);
    disp->setCursor(kIndicatorX, y);
    if (selected) {
        disp->print(">");
    } else {
        disp->print(" ");
    }
    
    // Draw bracketed label
    disp->setCursor(kLabelX, y);
    
    if (selected) {
        // Highlight: inverted colors
        int16_t labelWidth = strlen(kMenuItems[index].bracket) * 6 + 4;
        disp->fillRect(kLabelX - 2, y - 1, labelWidth, 10, SSD1306_WHITE);
        disp->setTextColor(SSD1306_BLACK);
    }
    
    disp->print(kMenuItems[index].bracket);
    disp->setTextColor(SSD1306_WHITE);
}

/**
 * @brief Draw complete menu screen
 */
void drawMenu(Adafruit_SSD1306* disp) {
    disp->clearDisplay();
    disp->setTextSize(1);
    disp->setTextColor(SSD1306_WHITE);
    
    // Title
    disp->setCursor(28, kTitleY);
    disp->print("MODO CAPTURA");
    
    // Divider line
    disp->drawFastHLine(0, kDividerY, kScreenWidth, SSD1306_WHITE);
    
    // Draw all items
    for (uint8_t i = 0; i < kItemCount; i++) {
        drawItem(disp, i, i == g_currentIndex);
    }
    
    // Hint bar at bottom
    disp->drawFastHLine(0, kHintY - 2, kScreenWidth, SSD1306_WHITE);
    disp->setCursor(2, kHintY);
    disp->print("Tap:nav  Hold:OK");
    
    disp->display();
}

/**
 * @brief Perform fade-in animation
 */
void performFadeIn(Adafruit_SSD1306* disp) {
    if (!g_config.enableFadeIn) {
        drawMenu(disp);
        return;
    }
    
    logDebug("Starting fade-in animation (%d steps)", g_config.fadeSteps);
    
    // Step 1: Black screen
    disp->clearDisplay();
    disp->display();
    delay(g_config.fadeDelayMs);
    
    // Step 2: Draw with partial content
    if (g_config.fadeSteps >= 2) {
        disp->clearDisplay();
        disp->setTextSize(1);
        disp->setTextColor(SSD1306_WHITE);
        disp->setCursor(28, kTitleY);
        disp->print("MODO CAPTURA");
        disp->drawFastHLine(0, kDividerY, kScreenWidth, SSD1306_WHITE);
        disp->display();
        delay(g_config.fadeDelayMs);
    }
    
    // Step 3: Add menu items one by one
    if (g_config.fadeSteps >= 3) {
        for (uint8_t i = 0; i < kItemCount; i++) {
            drawItem(disp, i, i == g_currentIndex);
            disp->display();
            delay(g_config.fadeDelayMs / 2);
        }
    }
    
    // Final: complete menu
    drawMenu(disp);
}

/**
 * @brief Handle button input
 * @return true if selection made
 */
bool handleInput(uint32_t& pressStartMs, bool& wasPressed) {
    bool isPressed = readButton();
    uint32_t now = millis();
    
    if (isPressed && !wasPressed) {
        // Button just pressed
        pressStartMs = now;
        wasPressed = true;
        logDebug("Button pressed at %lu ms", now);
    }
    else if (!isPressed && wasPressed) {
        // Button released
        uint32_t holdDuration = now - pressStartMs;
        wasPressed = false;
        
        logDebug("Button released, held for %lu ms", holdDuration);
        
        if (holdDuration >= g_config.longPressMs) {
            // Long press: SELECT
            logDebug("Long press detected -> SELECT item %d (%s)", 
                     g_currentIndex, kMenuItems[g_currentIndex].label);
            return true;
        } else {
            // Short press: NEXT ITEM
            g_currentIndex = (g_currentIndex + 1) % kItemCount;
            logDebug("Short press -> NEXT item %d (%s)", 
                     g_currentIndex, kMenuItems[g_currentIndex].label);
        }
    }
    else if (isPressed && wasPressed) {
        // Button still held - check for visual feedback
        uint32_t holdDuration = now - pressStartMs;
        if (holdDuration >= g_config.longPressMs) {
            // Show "selecting" feedback while still held
            Adafruit_SSD1306* disp = pxlcam::display::getDisplayPtr();
            if (disp) {
                disp->fillRect(2, kHintY, 124, 10, SSD1306_BLACK);
                disp->setCursor(2, kHintY);
                disp->print("Selecionando...");
                disp->display();
            }
        }
    }
    
    return false;
}

}  // anonymous namespace

//==============================================================================
// Public API Implementation
//==============================================================================

void init(const MenuConfig* config) {
    if (config) {
        g_config = *config;
    } else {
        // Defaults
        g_config.longPressMs = 1000;
        g_config.autoCloseMs = 15000;
        g_config.enableFadeIn = true;
        g_config.fadeSteps = 3;
        g_config.fadeDelayMs = 50;
    }
    
    g_isOpen = false;
    g_currentIndex = 0;
    
    logDebug("Menu system initialized (longPress=%lums, autoClose=%lums)", 
             g_config.longPressMs, g_config.autoCloseMs);
}

MenuResult showModal() {
    return showModalAt(0);
}

MenuResult showModalAt(uint8_t initialIndex) {
    Adafruit_SSD1306* disp = pxlcam::display::getDisplayPtr();
    if (!disp) {
        PXLCAM_LOGE_TAG(kLogTag, "Display not available");
        return MODE_CANCELLED;
    }
    
    // Set initial selection
    g_currentIndex = (initialIndex < kItemCount) ? initialIndex : 0;
    g_isOpen = true;
    
    logDebug("=== MENU OPENED ===");
    logDebug("Initial index: %d (%s)", g_currentIndex, kMenuItems[g_currentIndex].label);
    
    // Wait for any existing button press to release
    waitForRelease();
    
    // Fade-in animation
    performFadeIn(disp);
    
    // Modal loop variables
    uint32_t openTime = millis();
    uint32_t pressStartMs = 0;
    bool wasPressed = false;
    uint32_t lastRedraw = 0;
    MenuResult result = MODE_CANCELLED;
    
    logDebug("Entering modal loop...");
    
    // Modal loop - blocks until selection
    while (g_isOpen) {
        uint32_t now = millis();
        
        // Check auto-close timeout
        if (g_config.autoCloseMs > 0 && (now - openTime) >= g_config.autoCloseMs) {
            logDebug("Auto-close timeout reached");
            g_isOpen = false;
            result = MODE_CANCELLED;
            break;
        }
        
        // Handle button input
        if (handleInput(pressStartMs, wasPressed)) {
            // Selection made
            result = kMenuItems[g_currentIndex].result;
            g_isOpen = false;
            break;
        }
        
        // Redraw menu periodically (for cursor blink effect, etc.)
        if (now - lastRedraw >= 100) {
            lastRedraw = now;
            drawMenu(disp);
        }
        
        delay(10);  // Small delay to avoid busy-waiting
    }
    
    logDebug("=== MENU CLOSED ===");
    logDebug("Result: %s (value=%d)", getResultName(result), result);
    
    // Brief visual feedback on selection
    if (result != MODE_CANCELLED) {
        disp->clearDisplay();
        disp->setTextSize(1);
        disp->setTextColor(SSD1306_WHITE);
        disp->setCursor(20, 24);
        disp->print("Modo: ");
        disp->print(kMenuItems[g_currentIndex].label);
        
        // Draw checkmark indicator
        disp->setCursor(52, 40);
        disp->print("[OK]");
        disp->display();
        
        delay(500);
    }
    
    return result;
}

bool isOpen() {
    return g_isOpen;
}

void forceClose() {
    logDebug("Force close requested");
    g_isOpen = false;
}

const char* getResultName(MenuResult result) {
    switch (result) {
        case MODE_GAMEBOY:   return "GameBoy";
        case MODE_NIGHT:     return "Night";
        case MODE_NORMAL:    return "Normal";
        case MODE_CANCELLED: return "Cancelled";
        default:             return "Unknown";
    }
}

uint8_t toCaptureModeValue(MenuResult result) {
    for (uint8_t i = 0; i < kItemCount; i++) {
        if (kMenuItems[i].result == result) {
            return kMenuItems[i].modeValue;
        }
    }
    return 0;  // Default to Normal
}

MenuResult fromCaptureModeValue(uint8_t modeValue) {
    for (uint8_t i = 0; i < kItemCount; i++) {
        if (kMenuItems[i].modeValue == modeValue) {
            return kMenuItems[i].result;
        }
    }
    return MODE_NORMAL;  // Default
}

void setDebugLogging(bool enable) {
    g_debugLogging = enable;
    logDebug("Debug logging %s", enable ? "enabled" : "disabled");
}

uint8_t getItemCount() {
    return kItemCount;
}

uint8_t getCurrentIndex() {
    return g_currentIndex;
}

}  // namespace pxlcam::menu
