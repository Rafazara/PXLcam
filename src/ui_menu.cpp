/**
 * @file ui_menu.cpp
 * @brief OLED menu system implementation for PXLcam v1.2.0
 */

#include "ui_menu.h"
#include "display.h"
#include "display_ui.h"
#include "mode_manager.h"
#include "logging.h"

#include <Adafruit_SSD1306.h>
#include <cstring>
#include <cstdio>

namespace pxlcam::ui {

namespace {

constexpr const char* kLogTag = "pxlcam-menu";

// Menu state
MenuState g_menuState = MenuState::Hidden;
int g_selectedIndex = 0;
uint32_t g_lastUpdateMs = 0;
uint32_t g_autoCloseMs = 0;

// Menu items
constexpr int kMenuItemCount = 3;
const char* kModeNames[kMenuItemCount] = {"Normal", "GameBoy", "Night"};
const char* kModeIcons[kMenuItemCount] = {"[N]", "[G]", "[X]"};

// UI constants
constexpr int kMenuX = 10;
constexpr int kMenuY = 12;
constexpr int kItemHeight = 14;
constexpr int kMenuWidth = 108;

void drawMenuItem(int index, bool selected) {
    Adafruit_SSD1306* disp = pxlcam::display::getDisplayPtr();
    if (!disp) return;
    
    int y = kMenuY + index * kItemHeight;
    
    // Draw selection highlight
    if (selected) {
        disp->fillRect(kMenuX - 2, y - 1, kMenuWidth, kItemHeight - 2, SSD1306_WHITE);
        disp->setTextColor(SSD1306_BLACK);
    } else {
        disp->setTextColor(SSD1306_WHITE);
    }
    
    // Draw icon and name
    disp->setCursor(kMenuX, y);
    disp->print(kModeIcons[index]);
    disp->setCursor(kMenuX + 24, y);
    disp->print(kModeNames[index]);
    
    // Show checkmark if this is current mode
    if (static_cast<int>(pxlcam::mode::getCurrentMode()) == index) {
        disp->setCursor(kMenuX + 90, y);
        disp->print("*");
    }
}

}  // anonymous namespace

void init() {
    g_menuState = MenuState::Hidden;
    g_selectedIndex = static_cast<int>(pxlcam::mode::getCurrentMode());
    PXLCAM_LOGI_TAG(kLogTag, "Menu system initialized");
}

bool isMenuVisible() {
    return g_menuState != MenuState::Hidden;
}

void showModeMenu() {
    g_menuState = MenuState::ModeSelect;
    g_selectedIndex = static_cast<int>(pxlcam::mode::getCurrentMode());
    g_autoCloseMs = millis() + 10000;  // Auto-close after 10s
    PXLCAM_LOGI_TAG(kLogTag, "Mode menu opened");
    drawModeSelectScreen();
}

void hideMenu() {
    g_menuState = MenuState::Hidden;
    PXLCAM_LOGI_TAG(kLogTag, "Menu closed");
}

MenuAction handleTap() {
    if (g_menuState == MenuState::Hidden) {
        return MenuAction::None;
    }
    
    g_autoCloseMs = millis() + 10000;  // Reset auto-close timer
    
    switch (g_menuState) {
        case MenuState::ModeSelect:
            // Tap cycles through menu items
            g_selectedIndex = (g_selectedIndex + 1) % kMenuItemCount;
            PXLCAM_LOGI_TAG(kLogTag, "Selected: %s", kModeNames[g_selectedIndex]);
            drawModeSelectScreen();
            return MenuAction::None;
            
        case MenuState::Confirmation:
            // Tap confirms selection
            pxlcam::mode::setMode(static_cast<pxlcam::mode::CaptureMode>(g_selectedIndex), true);
            drawSuccessScreen("Modo alterado", kModeNames[g_selectedIndex], 1000);
            g_menuState = MenuState::Hidden;
            return MenuAction::ModeChanged;
            
        default:
            return MenuAction::None;
    }
}

MenuAction handleHold() {
    if (g_menuState == MenuState::Hidden) {
        return MenuAction::None;
    }
    
    switch (g_menuState) {
        case MenuState::ModeSelect:
            // Hold confirms selection
            if (g_selectedIndex != static_cast<int>(pxlcam::mode::getCurrentMode())) {
                // Change mode
                pxlcam::mode::setMode(static_cast<pxlcam::mode::CaptureMode>(g_selectedIndex), true);
                drawSuccessScreen("Modo alterado", kModeNames[g_selectedIndex], 1000);
            }
            g_menuState = MenuState::Hidden;
            return MenuAction::ModeChanged;
            
        case MenuState::Confirmation:
        case MenuState::Info:
        default:
            // Hold exits menu
            g_menuState = MenuState::Hidden;
            return MenuAction::MenuClosed;
    }
}

void updateDisplay() {
    if (g_menuState == MenuState::Hidden) {
        return;
    }
    
    // Check auto-close timeout
    if (millis() > g_autoCloseMs) {
        PXLCAM_LOGI_TAG(kLogTag, "Menu auto-closed (timeout)");
        hideMenu();
        return;
    }
    
    // Refresh if needed
    uint32_t now = millis();
    if (now - g_lastUpdateMs >= 100) {
        g_lastUpdateMs = now;
        
        switch (g_menuState) {
            case MenuState::ModeSelect:
                drawModeSelectScreen();
                break;
            case MenuState::Info:
                drawInfoScreen();
                break;
            default:
                break;
        }
    }
}

int getSelectedIndex() {
    return g_selectedIndex;
}

void setSelectedIndex(int index) {
    if (index >= 0 && index < kMenuItemCount) {
        g_selectedIndex = index;
    }
}

void drawModeSelectScreen() {
    Adafruit_SSD1306* disp = pxlcam::display::getDisplayPtr();
    if (!disp) return;
    
    disp->clearDisplay();
    
    // Title
    disp->setTextSize(1);
    disp->setTextColor(SSD1306_WHITE);
    disp->setCursor(28, 0);
    disp->print("MODO CAPTURA");
    
    // Draw line under title
    disp->drawFastHLine(0, 9, 128, SSD1306_WHITE);
    
    // Draw menu items
    for (int i = 0; i < kMenuItemCount; ++i) {
        drawMenuItem(i, i == g_selectedIndex);
    }
    
    // Hint bar
    disp->setTextColor(SSD1306_WHITE);
    disp->setCursor(2, 56);
    disp->print("Tap:nav  Hold:OK");
    
    disp->display();
}

void drawConfirmScreen(const char* modeName) {
    Adafruit_SSD1306* disp = pxlcam::display::getDisplayPtr();
    if (!disp) return;
    
    disp->clearDisplay();
    
    disp->setTextSize(1);
    disp->setTextColor(SSD1306_WHITE);
    
    disp->setCursor(20, 10);
    disp->print("Confirmar modo:");
    
    disp->setTextSize(2);
    disp->setCursor(20, 28);
    disp->print(modeName);
    
    disp->setTextSize(1);
    disp->setCursor(10, 54);
    disp->print("Hold:OK  Tap:cancel");
    
    disp->display();
}

void drawErrorScreen(const char* title, const char* message, bool canRetry) {
    Adafruit_SSD1306* disp = pxlcam::display::getDisplayPtr();
    if (!disp) return;
    
    disp->clearDisplay();
    
    // Error icon (X in box)
    disp->drawRect(4, 4, 16, 16, SSD1306_WHITE);
    disp->drawLine(6, 6, 18, 18, SSD1306_WHITE);
    disp->drawLine(18, 6, 6, 18, SSD1306_WHITE);
    
    // Title
    disp->setTextSize(1);
    disp->setTextColor(SSD1306_WHITE);
    disp->setCursor(24, 4);
    disp->print("ERRO");
    
    disp->setCursor(24, 14);
    if (title && strlen(title) < 14) {
        disp->print(title);
    }
    
    // Message
    disp->setCursor(4, 28);
    if (message) {
        // Word wrap if needed
        disp->print(message);
    }
    
    // Hint
    if (canRetry) {
        disp->setCursor(10, 54);
        disp->print("Tap: tentar novamente");
    }
    
    disp->display();
}

void drawSuccessScreen(const char* title, const char* message, uint16_t durationMs) {
    Adafruit_SSD1306* disp = pxlcam::display::getDisplayPtr();
    if (!disp) return;
    
    disp->clearDisplay();
    
    // Checkmark icon
    disp->drawCircle(12, 12, 10, SSD1306_WHITE);
    disp->drawLine(6, 12, 10, 16, SSD1306_WHITE);
    disp->drawLine(10, 16, 18, 8, SSD1306_WHITE);
    
    // Title
    disp->setTextSize(1);
    disp->setTextColor(SSD1306_WHITE);
    disp->setCursor(28, 4);
    disp->print("OK!");
    
    disp->setCursor(28, 14);
    if (title) {
        disp->print(title);
    }
    
    // Message centered
    if (message) {
        disp->setTextSize(2);
        int len = strlen(message);
        int x = (128 - len * 12) / 2;
        if (x < 0) x = 0;
        disp->setCursor(x, 32);
        disp->print(message);
    }
    
    disp->display();
    
    if (durationMs > 0) {
        delay(durationMs);
    }
}

void drawProgressScreen(const char* title, uint8_t progress) {
    Adafruit_SSD1306* disp = pxlcam::display::getDisplayPtr();
    if (!disp) return;
    
    disp->clearDisplay();
    
    // Title
    disp->setTextSize(1);
    disp->setTextColor(SSD1306_WHITE);
    disp->setCursor(4, 8);
    if (title) {
        disp->print(title);
    }
    
    // Progress bar background
    disp->drawRect(10, 28, 108, 12, SSD1306_WHITE);
    
    // Progress bar fill
    int fillWidth = (progress * 104) / 100;
    if (fillWidth > 0) {
        disp->fillRect(12, 30, fillWidth, 8, SSD1306_WHITE);
    }
    
    // Percentage
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", progress);
    int textX = (128 - strlen(buf) * 6) / 2;
    disp->setCursor(textX, 48);
    disp->print(buf);
    
    disp->display();
}

void drawInfoScreen() {
    Adafruit_SSD1306* disp = pxlcam::display::getDisplayPtr();
    if (!disp) return;
    
    disp->clearDisplay();
    
    disp->setTextSize(1);
    disp->setTextColor(SSD1306_WHITE);
    
    // Title
    disp->setCursor(30, 0);
    disp->print("PXLcam Info");
    disp->drawFastHLine(0, 9, 128, SSD1306_WHITE);
    
    // Version
    disp->setCursor(4, 14);
    disp->print("Versao: 1.2.0");
    
    // Current mode
    disp->setCursor(4, 26);
    disp->print("Modo: ");
    disp->print(pxlcam::mode::getModeName(pxlcam::mode::getCurrentMode()));
    
    // Free heap
    disp->setCursor(4, 38);
    disp->print("Heap: ");
    disp->print(ESP.getFreeHeap() / 1024);
    disp->print("KB");
    
    // PSRAM
    if (psramFound()) {
        disp->setCursor(4, 50);
        disp->print("PSRAM: ");
        disp->print(ESP.getFreePsram() / 1024);
        disp->print("KB");
    }
    
    disp->display();
}

MenuState getMenuState() {
    return g_menuState;
}

}  // namespace pxlcam::ui
