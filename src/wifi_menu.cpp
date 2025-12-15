/**
 * @file wifi_menu.cpp
 * @brief WiFi Preview Submenu implementation for PXLcam v1.3.0
 * 
 * @version 1.3.0
 * @date 2024
 */

#include "wifi_menu.h"
#include "display.h"
#include "logging.h"

#if PXLCAM_FEATURE_WIFI_PREVIEW
#include "wifi_preview.h"
#endif

#include <Arduino.h>
#include <Adafruit_SSD1306.h>

namespace pxlcam::wifi_menu {

namespace {

constexpr const char* kLogTag = "wifi_menu";

//==============================================================================
// Menu Items
//==============================================================================

constexpr uint8_t kItemCount = 5;

struct MenuItem {
    const char* label;
    const char* bracket;
    WifiMenuResult result;
};

const MenuItem kMenuItems[kItemCount] = {
    {"Iniciar",  "[ Iniciar ]",  WifiMenuResult::START},
    {"Parar",    "[ Parar ]",    WifiMenuResult::STOP},
    {"Info",     "[ Info ]",     WifiMenuResult::SHOW_INFO},
    {"QR Code",  "[ QR Code ]",  WifiMenuResult::SHOW_QR},
    {"Voltar",   "[ Voltar ]",   WifiMenuResult::BACK}
};

//==============================================================================
// Layout Constants
//==============================================================================

constexpr int16_t kScreenWidth = 128;
constexpr int16_t kScreenHeight = 64;
constexpr int16_t kTitleY = 2;
constexpr int16_t kDividerY = 12;
constexpr int16_t kListStartY = 16;
constexpr int16_t kItemHeight = 10;
constexpr int16_t kIndicatorX = 4;
constexpr int16_t kLabelX = 16;
constexpr int16_t kHintY = 56;

//==============================================================================
// State
//==============================================================================

WifiMenuConfig g_config;
bool g_isOpen = false;
uint8_t g_currentIndex = 0;

// Button GPIO
constexpr gpio_num_t kButtonPin = GPIO_NUM_12;
constexpr uint8_t kButtonActive = LOW;

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

void drawItem(Adafruit_SSD1306* disp, uint8_t index, bool selected) {
    int16_t y = kListStartY + index * kItemHeight;
    
    disp->fillRect(0, y - 1, kScreenWidth, kItemHeight, SSD1306_BLACK);
    disp->setTextColor(SSD1306_WHITE);
    
    disp->setCursor(kIndicatorX, y);
    disp->print(selected ? ">" : " ");
    
    disp->setCursor(kLabelX, y);
    
    if (selected) {
        int16_t labelWidth = strlen(kMenuItems[index].bracket) * 6 + 4;
        disp->fillRect(kLabelX - 2, y - 1, labelWidth, 9, SSD1306_WHITE);
        disp->setTextColor(SSD1306_BLACK);
    }
    
    disp->print(kMenuItems[index].bracket);
    disp->setTextColor(SSD1306_WHITE);
}

void drawMenu(Adafruit_SSD1306* disp) {
    disp->clearDisplay();
    disp->setTextSize(1);
    disp->setTextColor(SSD1306_WHITE);
    
    // Title with WiFi icon indicator
    disp->setCursor(20, kTitleY);
    disp->print("WIFI PREVIEW");
    
    // Show WiFi status indicator
#if PXLCAM_FEATURE_WIFI_PREVIEW
    if (pxlcam::WifiPreview::instance().isActive()) {
        disp->setCursor(110, kTitleY);
        disp->print("*");  // Active indicator
    }
#endif
    
    disp->drawFastHLine(0, kDividerY, kScreenWidth, SSD1306_WHITE);
    
    // Draw all items
    for (uint8_t i = 0; i < kItemCount; i++) {
        drawItem(disp, i, i == g_currentIndex);
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
        
        if (holdDuration >= g_config.longPressMs) {
            return true;  // Selection made
        } else {
            g_currentIndex = (g_currentIndex + 1) % kItemCount;
        }
    }
    else if (isPressed && wasPressed) {
        uint32_t holdDuration = now - pressStartMs;
        if (holdDuration >= g_config.longPressMs) {
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
// Public API
//==============================================================================

void init(const WifiMenuConfig* config) {
    if (config) {
        g_config = *config;
    } else {
        g_config.longPressMs = 1000;
        g_config.autoCloseMs = 20000;
        g_config.infoDisplayMs = 5000;
        g_config.qrDisplayMs = 15000;
    }
    
    g_isOpen = false;
    g_currentIndex = 0;
    
    PXLCAM_LOGI_TAG(kLogTag, "WiFi menu initialized");
}

WifiMenuResult showMenu() {
    Adafruit_SSD1306* disp = pxlcam::display::getDisplayPtr();
    if (!disp) {
        PXLCAM_LOGE_TAG(kLogTag, "Display not available");
        return WifiMenuResult::CANCELLED;
    }
    
    g_currentIndex = 0;
    g_isOpen = true;
    
    waitForRelease();
    drawMenu(disp);
    
    uint32_t openTime = millis();
    uint32_t pressStartMs = 0;
    bool wasPressed = false;
    uint32_t lastRedraw = 0;
    WifiMenuResult result = WifiMenuResult::CANCELLED;
    
    PXLCAM_LOGI_TAG(kLogTag, "WiFi menu opened");
    
    while (g_isOpen) {
        uint32_t now = millis();
        
        // Auto-close timeout
        if (g_config.autoCloseMs > 0 && (now - openTime) >= g_config.autoCloseMs) {
            PXLCAM_LOGI_TAG(kLogTag, "Auto-close timeout");
            g_isOpen = false;
            result = WifiMenuResult::CANCELLED;
            break;
        }
        
        // Handle input
        if (handleInput(pressStartMs, wasPressed)) {
            result = kMenuItems[g_currentIndex].result;
            g_isOpen = false;
            break;
        }
        
        // Redraw periodically
        if (now - lastRedraw >= 100) {
            lastRedraw = now;
            drawMenu(disp);
        }
        
        delay(10);
    }
    
    PXLCAM_LOGI_TAG(kLogTag, "WiFi menu result: %s", getResultName(result));
    
    // Brief feedback
    if (result != WifiMenuResult::CANCELLED && result != WifiMenuResult::BACK) {
        disp->clearDisplay();
        disp->setTextSize(1);
        disp->setTextColor(SSD1306_WHITE);
        disp->setCursor(20, 28);
        disp->print(kMenuItems[g_currentIndex].label);
        disp->setCursor(52, 44);
        disp->print("[OK]");
        disp->display();
        delay(400);
    }
    
    return result;
}

void drawInfoScreen(const char* ssid, const char* password, 
                    const char* ipAddress, bool isActive) {
    Adafruit_SSD1306* disp = pxlcam::display::getDisplayPtr();
    if (!disp) return;
    
    disp->clearDisplay();
    disp->setTextSize(1);
    disp->setTextColor(SSD1306_WHITE);
    
    // Title
    disp->setCursor(25, 0);
    disp->print("WIFI INFO");
    disp->drawFastHLine(0, 10, kScreenWidth, SSD1306_WHITE);
    
    // Status
    disp->setCursor(0, 14);
    disp->print("Status: ");
    disp->print(isActive ? "ATIVO" : "PARADO");
    
    // SSID
    disp->setCursor(0, 26);
    disp->print("SSID: ");
    disp->print(ssid);
    
    // Password
    disp->setCursor(0, 38);
    disp->print("Senha: ");
    disp->print(password);
    
    // IP
    disp->setCursor(0, 50);
    disp->print("IP: ");
    disp->print(ipAddress);
    
    disp->display();
}

void drawStatusOverlay(const char* ipAddress, uint8_t clientCount) {
    Adafruit_SSD1306* disp = pxlcam::display::getDisplayPtr();
    if (!disp) return;
    
    // Draw compact overlay at bottom
    disp->fillRect(0, 56, kScreenWidth, 8, SSD1306_BLACK);
    disp->setTextSize(1);
    disp->setTextColor(SSD1306_WHITE);
    disp->setCursor(0, 56);
    
    char buf[32];
    snprintf(buf, sizeof(buf), "WiFi:%s(%d)", ipAddress, clientCount);
    disp->print(buf);
    
    disp->display();
}

void drawStartingScreen() {
    Adafruit_SSD1306* disp = pxlcam::display::getDisplayPtr();
    if (!disp) return;
    
    disp->clearDisplay();
    disp->setTextSize(1);
    disp->setTextColor(SSD1306_WHITE);
    
    disp->setCursor(20, 20);
    disp->print("Iniciando WiFi");
    disp->setCursor(40, 36);
    disp->print("...");
    
    disp->display();
}

void drawStoppedScreen() {
    Adafruit_SSD1306* disp = pxlcam::display::getDisplayPtr();
    if (!disp) return;
    
    disp->clearDisplay();
    disp->setTextSize(1);
    disp->setTextColor(SSD1306_WHITE);
    
    disp->setCursor(20, 24);
    disp->print("WiFi Desligado");
    disp->setCursor(52, 40);
    disp->print("[OK]");
    
    disp->display();
}

const char* getResultName(WifiMenuResult result) {
    switch (result) {
        case WifiMenuResult::START:      return "Start";
        case WifiMenuResult::STOP:       return "Stop";
        case WifiMenuResult::SHOW_INFO:  return "Show Info";
        case WifiMenuResult::SHOW_QR:    return "Show QR";
        case WifiMenuResult::BACK:       return "Back";
        case WifiMenuResult::CANCELLED:  return "Cancelled";
        default:                         return "Unknown";
    }
}

bool isOpen() {
    return g_isOpen;
}

}  // namespace pxlcam::wifi_menu
