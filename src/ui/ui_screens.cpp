/**
 * @file ui_screens.cpp
 * @brief UI Screen Components Implementation for PXLcam v1.2.0
 * 
 * @author PXLcam Team
 * @version 1.2.0
 * @date 2024
 */

#include "ui_screens.h"
#include <Arduino.h>
#include <cstring>

namespace pxlcam {
namespace ui {

// ============================================================================
// UiTheme Implementation
// ============================================================================

UiTheme::UiTheme() {
    init();
}

void UiTheme::init() {
    // Status bar at top
    statusBar_ = {
        .x = 0,
        .y = 0,
        .width = Display::WIDTH,
        .height = 10,
        .padding = 2,
        .iconSize = 8,
        .iconSpacing = 4,
        .showBattery = true,
        .showMode = true,
        .showStorage = true
    };

    // Hint bar at bottom
    hintBar_ = {
        .x = 0,
        .y = static_cast<uint8_t>(Display::HEIGHT - 10),
        .width = Display::WIDTH,
        .height = 10,
        .padding = 2,
        .maxHints = 3,
        .showSeparator = true
    };

    // Menu layout (between status and hint bars)
    menuLayout_ = {
        .x = 0,
        .y = static_cast<uint8_t>(statusBar_.height + 2),
        .width = Display::WIDTH,
        .height = static_cast<uint8_t>(Display::HEIGHT - statusBar_.height - hintBar_.height - 4),
        .itemHeight = 12,
        .visibleItems = 4,
        .padding = 2,
        .scrollbarWidth = 4,
        .showTitle = true,
        .showScrollbar = true
    };
}

void UiTheme::getContentArea(uint8_t& x, uint8_t& y, uint8_t& w, uint8_t& h) const {
    x = 0;
    y = statusBar_.height + 2;
    w = Display::WIDTH;
    h = Display::HEIGHT - statusBar_.height - hintBar_.height - 4;
}

uint16_t UiTheme::calculateTextWidth(const char* text, const FontConfig& font) const {
    if (!text) return 0;
    return strlen(text) * (font.width + font.spacing);
}

uint8_t UiTheme::centerTextX(const char* text, const FontConfig& font, uint8_t areaWidth) const {
    uint16_t textWidth = calculateTextWidth(text, font);
    if (textWidth >= areaWidth) return 0;
    return (areaWidth - textWidth) / 2;
}

// ============================================================================
// MockDisplay Implementation
// ============================================================================

void MockDisplay::clear() {
    Serial.println("[Display] Clear");
    dirty_ = true;
}

void MockDisplay::drawText(uint8_t x, uint8_t y, const char* text, const FontConfig& font) {
    Serial.printf("[Display] Text @(%d,%d) s%d: '%s'\n", x, y, font.scale, text);
    dirty_ = true;
}

void MockDisplay::drawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool filled) {
    Serial.printf("[Display] Rect @(%d,%d) %dx%d %s\n", x, y, w, h, filled ? "filled" : "outline");
    dirty_ = true;
}

void MockDisplay::drawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
    Serial.printf("[Display] Line (%d,%d)->(%d,%d)\n", x1, y1, x2, y2);
    dirty_ = true;
}

void MockDisplay::drawPixel(uint8_t x, uint8_t y, uint8_t color) {
    // Too verbose for normal logging
    dirty_ = true;
}

void MockDisplay::setTextSize(uint8_t size) {
    textSize_ = size;
}

void MockDisplay::setCursor(uint8_t x, uint8_t y) {
    cursorX_ = x;
    cursorY_ = y;
}

void MockDisplay::print(const char* text) {
    Serial.printf("[Display] Print @(%d,%d): '%s'\n", cursorX_, cursorY_, text);
    dirty_ = true;
}

void MockDisplay::display() {
    if (dirty_) {
        Serial.println("[Display] Buffer committed");
        dirty_ = false;
    }
}

// ============================================================================
// SplashScreen Implementation
// ============================================================================

SplashScreen::SplashScreen()
    : startTime_(0)
    , complete_(false)
{
}

void SplashScreen::onEnter() {
    Serial.println("[SplashScreen] Enter");
    startTime_ = millis();
    complete_ = false;
}

void SplashScreen::onExit() {
    Serial.println("[SplashScreen] Exit");
}

void SplashScreen::update() {
    if (!complete_ && (millis() - startTime_ >= Timing::SPLASH_DURATION)) {
        complete_ = true;
        Serial.println("[SplashScreen] Complete");
    }
}

void SplashScreen::render() {
    auto& display = MockDisplay::instance();
    auto& theme = UiTheme::instance();
    
    display.clear();
    
    // Center "PXLcam" title
    const char* title = "PXLcam";
    uint8_t x = theme.centerTextX(title, Fonts::LARGE, Display::WIDTH);
    display.drawText(x, 15, title, Fonts::LARGE);
    
    // Version
    const char* version = "v1.2.0";
    x = theme.centerTextX(version, Fonts::SMALL, Display::WIDTH);
    display.drawText(x, 45, version, Fonts::SMALL);
    
    // Loading indicator
    const char* loading = "Loading...";
    x = theme.centerTextX(loading, Fonts::SMALL, Display::WIDTH);
    display.drawText(x, 55, loading, Fonts::SMALL);
    
    display.display();
}

// ============================================================================
// IdleScreen Implementation
// ============================================================================

IdleScreen::IdleScreen()
    : statusText_("Ready")
    , lastUpdateTime_(0)
{
}

void IdleScreen::onEnter() {
    Serial.println("[IdleScreen] Enter");
    lastUpdateTime_ = millis();
}

void IdleScreen::onExit() {
    Serial.println("[IdleScreen] Exit");
}

void IdleScreen::update() {
    // Could update status periodically
}

void IdleScreen::render() {
    auto& display = MockDisplay::instance();
    auto& theme = UiTheme::instance();
    auto& statusBar = theme.getStatusBar();
    
    display.clear();
    
    // ===== Status Bar =====
    // Left: Current mode
    display.drawText(2, 1, "IDLE", Fonts::SMALL);
    // Right: Fake battery icon + clock
    display.drawText(Display::WIDTH - 45, 1, "[###]", Fonts::SMALL);
    display.drawText(Display::WIDTH - 22, 1, "12:00", Fonts::SMALL);
    // Status bar separator line
    display.drawLine(0, statusBar.height, Display::WIDTH, statusBar.height);
    
    // Center content
    const char* msg = "Press button";
    uint8_t x = theme.centerTextX(msg, Fonts::MEDIUM, Display::WIDTH);
    display.drawText(x, 25, msg, Fonts::MEDIUM);
    
    // Status text
    x = theme.centerTextX(statusText_, Fonts::SMALL, Display::WIDTH);
    display.drawText(x, 45, statusText_, Fonts::SMALL);
    
    // Hint bar
    display.drawLine(0, theme.getHintBar().y - 1, Display::WIDTH, theme.getHintBar().y - 1);
    display.drawText(2, theme.getHintBar().y + 1, "TAP: Menu", Fonts::SMALL);
    
    display.display();
}

void IdleScreen::setStatusText(const char* text) {
    statusText_ = text ? text : "Ready";
}

// ============================================================================
// MenuScreen Implementation
// ============================================================================

MenuScreen::MenuScreen(features::MenuSystem* menuSystem)
    : menuSystem_(menuSystem)
    , scrollOffset_(0)
{
}

void MenuScreen::onEnter() {
    Serial.println("[MenuScreen] Enter");
    scrollOffset_ = 0;
}

void MenuScreen::onExit() {
    Serial.println("[MenuScreen] Exit");
}

void MenuScreen::update() {
    if (!menuSystem_) return;
    
    // Adjust scroll offset based on selection
    int selected = menuSystem_->getSelectedIndex();
    auto& layout = UiTheme::instance().getMenuLayout();
    
    if (selected < scrollOffset_) {
        scrollOffset_ = selected;
    } else if (selected >= scrollOffset_ + layout.visibleItems) {
        scrollOffset_ = selected - layout.visibleItems + 1;
    }
}

void MenuScreen::render() {
    if (!menuSystem_ || !menuSystem_->isOpen()) return;
    
    auto& display = MockDisplay::instance();
    
    display.clear();
    
    renderTitle();
    renderItems();
    renderHints();
    
    if (menuSystem_->getItemCount() > UiTheme::instance().getMenuLayout().visibleItems) {
        renderScrollbar();
    }
    
    display.display();
}

void MenuScreen::renderTitle() {
    auto& display = MockDisplay::instance();
    auto& theme = UiTheme::instance();
    auto& statusBar = theme.getStatusBar();
    
    // ===== Status Bar =====
    // Left: Current mode
    display.drawText(2, 1, "MENU", Fonts::SMALL);
    
    // Right: Fake battery icon + clock
    // Battery: simple representation [###]
    display.drawText(Display::WIDTH - 45, 1, "[###]", Fonts::SMALL);
    // Fake clock
    display.drawText(Display::WIDTH - 22, 1, "12:00", Fonts::SMALL);
    
    // Status bar separator line
    display.drawLine(0, statusBar.height, Display::WIDTH, statusBar.height);
    
    // ===== Menu Title =====
    const char* title = menuSystem_->getCurrentMenuTitle();
    uint8_t titleX = theme.centerTextX(title, Fonts::SMALL, Display::WIDTH);
    display.drawText(titleX, statusBar.height + 2, title, Fonts::SMALL);
    
    // Title underline
    display.drawLine(0, statusBar.height + 11, Display::WIDTH, statusBar.height + 11);
}

void MenuScreen::renderItems() {
    auto& display = MockDisplay::instance();
    auto& theme = UiTheme::instance();
    auto& layout = theme.getMenuLayout();
    
    const auto* items = menuSystem_->getCurrentItems();
    if (!items) return;
    
    int selected = menuSystem_->getSelectedIndex();
    uint8_t y = layout.y;
    
    for (size_t i = scrollOffset_; i < items->size() && i < scrollOffset_ + layout.visibleItems; i++) {
        const auto& item = (*items)[i];
        
        bool isSelected = (i == (size_t)selected);
        
        if (isSelected) {
            // Draw selection highlight
            display.drawRect(layout.x, y, layout.width - layout.scrollbarWidth - 2, layout.itemHeight, true);
        }
        
        // Draw item label
        char label[32];
        snprintf(label, sizeof(label), "%s%s", isSelected ? "> " : "  ", item.label);
        display.drawText(layout.x + layout.padding, y + 2, label, Fonts::SMALL);
        
        y += layout.itemHeight;
    }
}

void MenuScreen::renderHints() {
    auto& display = MockDisplay::instance();
    auto& theme = UiTheme::instance();
    auto& hintBar = theme.getHintBar();
    
    // Hint bar separator line
    display.drawLine(0, hintBar.y - 1, Display::WIDTH, hintBar.y - 1);
    
    // Single-button navigation hints:
    // Short press = Next item, Long press (1s) = Select, Hold (2s) = Back to idle
    display.drawText(2, hintBar.y + 1, "TAP:Next  HOLD:Select", Fonts::SMALL);
}

void MenuScreen::renderScrollbar() {
    auto& display = MockDisplay::instance();
    auto& layout = UiTheme::instance().getMenuLayout();
    
    size_t itemCount = menuSystem_->getItemCount();
    if (itemCount == 0) return;
    
    uint8_t scrollbarX = Display::WIDTH - layout.scrollbarWidth;
    uint8_t scrollbarHeight = layout.height;
    uint8_t thumbHeight = scrollbarHeight / itemCount;
    if (thumbHeight < 4) thumbHeight = 4;
    
    uint8_t thumbY = layout.y + (scrollbarHeight - thumbHeight) * scrollOffset_ / (itemCount - layout.visibleItems);
    
    display.drawRect(scrollbarX, layout.y, layout.scrollbarWidth, scrollbarHeight, false);
    display.drawRect(scrollbarX, thumbY, layout.scrollbarWidth, thumbHeight, true);
}

// ============================================================================
// PreviewScreen Implementation
// ============================================================================

PreviewScreen::PreviewScreen()
    : fps_(0.0f)
    , frameCount_(0)
{
}

void PreviewScreen::onEnter() {
    Serial.println("[PreviewScreen] Enter");
    frameCount_ = 0;
    fps_ = 0.0f;
}

void PreviewScreen::onExit() {
    Serial.println("[PreviewScreen] Exit");
}

void PreviewScreen::update() {
    frameCount_++;
}

void PreviewScreen::render() {
    auto& display = MockDisplay::instance();
    auto& theme = UiTheme::instance();
    
    display.clear();
    
    // Preview area placeholder
    display.drawRect(0, 0, Display::WIDTH, Display::HEIGHT - 10, false);
    
    const char* msg = "[PREVIEW]";
    uint8_t x = theme.centerTextX(msg, Fonts::SMALL, Display::WIDTH);
    display.drawText(x, 28, msg, Fonts::SMALL);
    
    // FPS counter
    char fpsText[16];
    snprintf(fpsText, sizeof(fpsText), "%.1f FPS", fps_);
    display.drawText(2, Display::HEIGHT - 9, fpsText, Fonts::SMALL);
    
    // Hint
    display.drawText(70, Display::HEIGHT - 9, "[SEL] Capture", Fonts::SMALL);
    
    display.display();
}

// ============================================================================
// CaptureScreen Implementation (with mini preview confirmation)
// ============================================================================

CaptureScreen::CaptureScreen()
    : enterTime_(0)
    , captureComplete_(false)
{
}

void CaptureScreen::onEnter() {
    Serial.println("[CaptureScreen] Enter - showing capture confirmation");
    enterTime_ = millis();
    captureComplete_ = false;
}

void CaptureScreen::onExit() {
    Serial.println("[CaptureScreen] Exit");
}

void CaptureScreen::update() {
    // Mark complete after pipeline has finished
    const auto& stats = features::capture::getLastStats();
    if (stats.totalTimeMs > 0 && !captureComplete_) {
        captureComplete_ = true;
    }
}

void CaptureScreen::render() {
    auto& display = MockDisplay::instance();
    auto& theme = UiTheme::instance();
    auto& statusBar = theme.getStatusBar();
    
    display.clear();
    
    // ===== Status Bar =====
    display.drawText(2, 1, "CAPTURE", Fonts::SMALL);
    display.drawText(Display::WIDTH - 45, 1, "[###]", Fonts::SMALL);
    display.drawText(Display::WIDTH - 22, 1, "12:00", Fonts::SMALL);
    display.drawLine(0, statusBar.height, Display::WIDTH, statusBar.height);
    
    if (captureComplete_) {
        // Show confirmation with mini preview
        renderMiniPreview();
        renderStats();
        
        // Hint bar
        display.drawLine(0, theme.getHintBar().y - 1, Display::WIDTH, theme.getHintBar().y - 1);
        display.drawText(2, theme.getHintBar().y + 1, "Saved! TAP: Continue", Fonts::SMALL);
    } else {
        // Capture in progress
        const char* msg = "Capturing...";
        uint8_t x = theme.centerTextX(msg, Fonts::MEDIUM, Display::WIDTH);
        display.drawText(x, 30, msg, Fonts::MEDIUM);
        
        // Progress indicator (simple)
        uint32_t elapsed = millis() - enterTime_;
        uint8_t dots = (elapsed / 200) % 4;
        char progress[8] = "    ";
        for (uint8_t i = 0; i < dots; i++) progress[i] = '.';
        display.drawText(60, 45, progress, Fonts::SMALL);
    }
    
    display.display();
}

void CaptureScreen::renderMiniPreview() {
    auto& display = MockDisplay::instance();
    const auto& preview = features::capture::getLastPreview();
    
    if (!preview.valid) {
        display.drawText(10, 20, "[No Preview]", Fonts::SMALL);
        return;
    }
    
    // Draw mini preview representation (64x64 -> simplified for mock display)
    // Center it horizontally
    uint8_t previewX = 2;
    uint8_t previewY = 14;
    uint8_t displaySize = 40;  // Scaled down for mock display
    
    // Draw frame around preview area
    display.drawRect(previewX, previewY, displaySize, displaySize, false);
    
    // Simulated preview content (show "[64x64]" text inside)
    display.drawText(previewX + 4, previewY + 15, "64x64", Fonts::SMALL);
    
    // Show "OK" checkmark indicator
    display.drawText(previewX + displaySize + 4, previewY + 10, "OK!", Fonts::SMALL);
}

void CaptureScreen::renderStats() {
    auto& display = MockDisplay::instance();
    const auto& stats = features::capture::getLastStats();
    
    // Stats on right side of screen
    uint8_t statsX = 65;
    uint8_t statsY = 14;
    
    char buf[24];
    
    // Resolution
    snprintf(buf, sizeof(buf), "%dx%d", stats.width, stats.height);
    display.drawText(statsX, statsY, buf, Fonts::SMALL);
    
    // Total time
    snprintf(buf, sizeof(buf), "%lums", stats.totalTimeMs);
    display.drawText(statsX, statsY + 10, buf, Fonts::SMALL);
    
    // BMP size
    snprintf(buf, sizeof(buf), "%luB", (unsigned long)stats.bmpSizeBytes);
    display.drawText(statsX, statsY + 20, buf, Fonts::SMALL);
}

// ============================================================================
// ScreenManager Implementation
// ============================================================================

ScreenManager::ScreenManager()
    : currentScreen_(nullptr)
    , nextScreen_(nullptr)
{
    for (size_t i = 0; i < static_cast<size_t>(ScreenId::SCREEN_COUNT); i++) {
        screens_[i] = nullptr;
    }
}

void ScreenManager::init() {
    Serial.println("[ScreenManager] Initialized");
}

void ScreenManager::registerScreen(IScreen* screen) {
    if (!screen) return;
    
    size_t idx = static_cast<size_t>(screen->getId());
    if (idx < static_cast<size_t>(ScreenId::SCREEN_COUNT)) {
        screens_[idx] = screen;
        Serial.printf("[ScreenManager] Registered screen %d\n", idx);
    }
}

void ScreenManager::setScreen(ScreenId id) {
    size_t idx = static_cast<size_t>(id);
    if (idx >= static_cast<size_t>(ScreenId::SCREEN_COUNT)) return;
    
    IScreen* newScreen = screens_[idx];
    if (!newScreen || newScreen == currentScreen_) return;
    
    nextScreen_ = newScreen;
}

void ScreenManager::update() {
    // Handle screen transition
    if (nextScreen_ && nextScreen_ != currentScreen_) {
        if (currentScreen_) {
            currentScreen_->onExit();
        }
        currentScreen_ = nextScreen_;
        nextScreen_ = nullptr;
        currentScreen_->onEnter();
    }
    
    if (currentScreen_) {
        currentScreen_->update();
    }
}

void ScreenManager::render() {
    if (currentScreen_) {
        currentScreen_->render();
    }
}

ScreenId ScreenManager::getCurrentScreenId() const {
    return currentScreen_ ? currentScreen_->getId() : ScreenId::NONE;
}

} // namespace ui
} // namespace pxlcam
