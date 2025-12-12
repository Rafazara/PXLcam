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
    if (fadeLevel_ < 128) return;  // Don't draw when faded out
    Serial.printf("[Display] Text @(%d,%d) s%d: '%s'\n", x, y, font.scale, text);
    dirty_ = true;
}

void MockDisplay::drawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool filled) {
    if (fadeLevel_ < 128 && !filled) return;
    Serial.printf("[Display] Rect @(%d,%d) %dx%d %s\n", x, y, w, h, filled ? "filled" : "outline");
    dirty_ = true;
}

void MockDisplay::drawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
    if (fadeLevel_ < 128) return;
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
    if (fadeLevel_ < 128) return;
    Serial.printf("[Display] Print @(%d,%d): '%s'\n", cursorX_, cursorY_, text);
    dirty_ = true;
}

void MockDisplay::display() {
    if (dirty_) {
        if (fadeLevel_ < 255) {
            Serial.printf("[Display] Buffer committed (fade: %d%%)\n", (fadeLevel_ * 100) / 255);
        } else {
            Serial.println("[Display] Buffer committed");
        }
        dirty_ = false;
    }
}

void MockDisplay::setFadeLevel(uint8_t level) {
    if (fadeLevel_ != level) {
        fadeLevel_ = level;
        Serial.printf("[Display] Fade level: %d%%\n", (level * 100) / 255);
    }
}

void MockDisplay::drawShutter(uint8_t closePercent) {
    if (closePercent == 0) return;
    
    // Simulate shutter effect - draw horizontal bars from top and bottom
    uint8_t shutterHeight = (Display::HEIGHT * closePercent) / 200;  // Half from each side
    
    Serial.printf("[Display] Shutter effect: %d%% closed\n", closePercent);
    
    // Top shutter blade
    if (shutterHeight > 0) {
        drawRect(0, 0, Display::WIDTH, shutterHeight, true);
    }
    // Bottom shutter blade
    if (shutterHeight > 0) {
        drawRect(0, Display::HEIGHT - shutterHeight, Display::WIDTH, shutterHeight, true);
    }
    
    dirty_ = true;
}

// ============================================================================
// StatusBarRenderer Implementation
// ============================================================================

void StatusBarRenderer::render(const char* modeText, float fps) {
    auto& display = MockDisplay::instance();
    auto& theme = UiTheme::instance();
    auto& statusBar = theme.getStatusBar();
    
    // Left: Mode indicator with blinking dot for active state
    static uint32_t lastBlink = 0;
    static bool blinkOn = true;
    if (millis() - lastBlink > Timing::MODE_INDICATOR_BLINK) {
        blinkOn = !blinkOn;
        lastBlink = millis();
    }
    
    char modeWithDot[16];
    snprintf(modeWithDot, sizeof(modeWithDot), "%s%s", blinkOn ? "*" : " ", modeText);
    display.drawText(2, 1, modeWithDot, Fonts::SMALL);
    
    // Center: FPS if provided
    if (fps > 0.0f) {
        renderFps(50, 1, fps);
    }
    
    // Right side: Battery + Clock
    renderBattery(Display::WIDTH - 45, 1);
    renderClock(Display::WIDTH - 22, 1);
    
    // Status bar separator line
    display.drawLine(0, statusBar.height, Display::WIDTH, statusBar.height);
}

void StatusBarRenderer::renderBattery(uint8_t x, uint8_t y) {
    auto& display = MockDisplay::instance();
    
    // Simulated battery level (cycle for demo)
    static uint8_t fakeBatteryLevel = 3;
    static uint32_t lastUpdate = 0;
    if (millis() - lastUpdate > 10000) {
        fakeBatteryLevel = (fakeBatteryLevel > 0) ? fakeBatteryLevel - 1 : 3;
        lastUpdate = millis();
    }
    
    const char* icon;
    switch (fakeBatteryLevel) {
        case 3: icon = Icons::BATTERY_FULL; break;
        case 2: icon = Icons::BATTERY_MID; break;
        case 1: icon = Icons::BATTERY_LOW; break;
        default: icon = Icons::BATTERY_EMPTY; break;
    }
    display.drawText(x, y, icon, Fonts::SMALL);
}

void StatusBarRenderer::renderClock(uint8_t x, uint8_t y) {
    auto& display = MockDisplay::instance();
    
    // Simulated clock (increments)
    static uint8_t fakeHour = 12;
    static uint8_t fakeMin = 0;
    static uint32_t lastUpdate = 0;
    if (millis() - lastUpdate > 60000) {
        fakeMin++;
        if (fakeMin >= 60) {
            fakeMin = 0;
            fakeHour = (fakeHour + 1) % 24;
        }
        lastUpdate = millis();
    }
    
    char timeStr[6];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", fakeHour, fakeMin);
    display.drawText(x, y, timeStr, Fonts::SMALL);
}

void StatusBarRenderer::renderFps(uint8_t x, uint8_t y, float fps) {
    auto& display = MockDisplay::instance();
    char fpsStr[10];
    snprintf(fpsStr, sizeof(fpsStr), "%.0fFPS", fps);
    display.drawText(x, y, fpsStr, Fonts::SMALL);
}

// ============================================================================
// HintBar Implementation (with auto-hide)
// ============================================================================

void HintBar::show(const char* hint) {
    hint_ = hint;
    visible_ = true;
    fadeOut_ = false;
    fadeProgress_ = 255;
    lastActivityTime_ = millis();
    Serial.printf("[HintBar] Show: '%s'\n", hint ? hint : "(null)");
}

void HintBar::hide() {
    if (visible_ && !fadeOut_) {
        fadeOut_ = true;
        fadeProgress_ = 255;
        Serial.println("[HintBar] Starting fade out");
    }
}

void HintBar::update() {
    uint32_t now = millis();
    
    // Auto-hide after inactivity
    if (visible_ && !fadeOut_ && (now - lastActivityTime_ > Timing::HINT_AUTO_HIDE)) {
        fadeOut_ = true;
        fadeProgress_ = 255;
        Serial.println("[HintBar] Auto-hide triggered");
    }
    
    // Update fade animation
    if (fadeOut_ && fadeProgress_ > 0) {
        uint32_t elapsed = now - lastActivityTime_ - Timing::HINT_AUTO_HIDE;
        fadeProgress_ = 255 - min((elapsed * 255) / Timing::HINT_FADE, (uint32_t)255);
        
        if (fadeProgress_ == 0) {
            visible_ = false;
            fadeOut_ = false;
            Serial.println("[HintBar] Hidden");
        }
    }
}

void HintBar::render() {
    if (!visible_ || !hint_) return;
    
    auto& display = MockDisplay::instance();
    auto& theme = UiTheme::instance();
    auto& hintBar = theme.getHintBar();
    
    // Draw separator line
    display.drawLine(0, hintBar.y - 1, Display::WIDTH, hintBar.y - 1);
    
    // Draw hint text (with fade effect simulation)
    if (fadeProgress_ > 128) {
        display.drawText(2, hintBar.y + 1, hint_, Fonts::SMALL);
    } else if (fadeProgress_ > 0) {
        // Fading - show dimmed (in real hardware, would use contrast)
        Serial.printf("[HintBar] Fading: %d%%\n", (fadeProgress_ * 100) / 255);
    }
}

void HintBar::resetAutoHide() {
    lastActivityTime_ = millis();
    fadeOut_ = false;
    fadeProgress_ = 255;
}

// ============================================================================
// SplashScreen Implementation
// ============================================================================

SplashScreen::SplashScreen()
    : startTime_(0)
    , complete_(false)
    , fadePhase_(0)
{
}

void SplashScreen::onEnter() {
    Serial.println("[SplashScreen] Enter - PXLcam v1.2");
    startTime_ = millis();
    complete_ = false;
    fadePhase_ = 0;  // Start with fade in
    MockDisplay::instance().setFadeLevel(0);  // Start black
}

void SplashScreen::onExit() {
    Serial.println("[SplashScreen] Exit");
    MockDisplay::instance().setFadeLevel(255);  // Ensure full brightness
}

void SplashScreen::update() {
    uint32_t elapsed = millis() - startTime_;
    
    // Phase timing: 400ms fade in, display, 400ms fade out
    constexpr uint32_t FADE_IN_TIME = 400;
    constexpr uint32_t FADE_OUT_START = Timing::SPLASH_DURATION - 400;
    
    if (elapsed < FADE_IN_TIME) {
        // Fade in phase
        fadePhase_ = 0;
        uint8_t fadeLevel = (elapsed * 255) / FADE_IN_TIME;
        MockDisplay::instance().setFadeLevel(fadeLevel);
    } else if (elapsed < FADE_OUT_START) {
        // Display phase
        fadePhase_ = 1;
        MockDisplay::instance().setFadeLevel(255);
    } else if (elapsed < Timing::SPLASH_DURATION) {
        // Fade out phase
        fadePhase_ = 2;
        uint32_t fadeElapsed = elapsed - FADE_OUT_START;
        uint8_t fadeLevel = 255 - ((fadeElapsed * 255) / 400);
        MockDisplay::instance().setFadeLevel(fadeLevel);
    } else if (!complete_) {
        complete_ = true;
        Serial.println("[SplashScreen] Complete");
    }
}

void SplashScreen::render() {
    auto& display = MockDisplay::instance();
    auto& theme = UiTheme::instance();
    
    display.clear();
    
    // ===== Main Title: PXLcam =====
    const char* title = "PXLcam";
    uint8_t x = theme.centerTextX(title, Fonts::LARGE, Display::WIDTH);
    display.drawText(x, 12, title, Fonts::LARGE);
    
    // ===== Subtitle with em dash =====
    const char* subtitle = "- v1.2 -";
    x = theme.centerTextX(subtitle, Fonts::SMALL, Display::WIDTH);
    display.drawText(x, 38, subtitle, Fonts::SMALL);
    
    // ===== Loading bar animation =====
    renderLoadingBar();
    
    // ===== Decorative lines =====
    display.drawLine(20, 8, 108, 8);   // Top line
    display.drawLine(20, 56, 108, 56); // Bottom line
    
    display.display();
}

void SplashScreen::renderFadeEffect() {
    // Fade effect is handled by setFadeLevel in update()
}

void SplashScreen::renderLoadingBar() {
    auto& display = MockDisplay::instance();
    
    uint32_t elapsed = millis() - startTime_;
    uint8_t progress = min((elapsed * 100) / Timing::SPLASH_DURATION, (uint32_t)100);
    
    // Loading bar position
    uint8_t barX = 24;
    uint8_t barY = 48;
    uint8_t barW = 80;
    uint8_t barH = 4;
    
    // Draw outline
    display.drawRect(barX, barY, barW, barH, false);
    
    // Draw fill
    uint8_t fillW = (barW - 2) * progress / 100;
    if (fillW > 0) {
        display.drawRect(barX + 1, barY + 1, fillW, barH - 2, true);
    }
    
    // Progress percentage
    char progStr[8];
    snprintf(progStr, sizeof(progStr), "%d%%", progress);
    display.drawText(barX + barW + 4, barY - 1, progStr, Fonts::SMALL);
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
    HintBar::instance().show("TAP: Menu");
}

void IdleScreen::onExit() {
    Serial.println("[IdleScreen] Exit");
}

void IdleScreen::update() {
    HintBar::instance().update();
}

void IdleScreen::render() {
    auto& display = MockDisplay::instance();
    auto& theme = UiTheme::instance();
    
    display.clear();
    
    // ===== Status Bar (using shared renderer) =====
    StatusBarRenderer::render("IDLE");
    
    // Center content
    const char* msg = "Press button";
    uint8_t x = theme.centerTextX(msg, Fonts::MEDIUM, Display::WIDTH);
    display.drawText(x, 25, msg, Fonts::MEDIUM);
    
    // Status text
    x = theme.centerTextX(statusText_, Fonts::SMALL, Display::WIDTH);
    display.drawText(x, 45, statusText_, Fonts::SMALL);
    
    // Hint bar with auto-hide
    HintBar::instance().render();
    
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
    HintBar::instance().show("TAP:Next  HOLD:Select");
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
    
    // Reset hint bar auto-hide on menu navigation
    HintBar::instance().resetAutoHide();
    HintBar::instance().update();
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
    
    // ===== Status Bar (using shared renderer) =====
    StatusBarRenderer::render("MENU");
    
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
    // Use shared HintBar with auto-hide
    HintBar::instance().render();
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
    lastHintTime_ = millis();
    HintBar::instance().show("TAP: Capture  HOLD: Menu");
}

void PreviewScreen::onExit() {
    Serial.println("[PreviewScreen] Exit");
}

void PreviewScreen::update() {
    frameCount_++;
    HintBar::instance().update();
}

void PreviewScreen::render() {
    auto& display = MockDisplay::instance();
    auto& theme = UiTheme::instance();
    
    display.clear();
    
    // Status bar at top
    renderStatusBar();
    
    // Preview area (below status bar)
    renderPreviewArea();
    
    // Hint bar at bottom (auto-hides)
    HintBar::instance().render();
    
    display.display();
}

void PreviewScreen::renderStatusBar() {
    StatusBarRenderer::render("PREVIEW", fps_);
}

void PreviewScreen::renderPreviewArea() {
    auto& display = MockDisplay::instance();
    auto& theme = UiTheme::instance();
    auto& statusBar = theme.getStatusBar();
    
    // Preview area (below status bar, above hint bar)
    uint8_t previewY = statusBar.height + 2;
    uint8_t previewHeight = Display::HEIGHT - previewY - 12;
    
    display.drawRect(2, previewY, Display::WIDTH - 4, previewHeight, false);
    
    const char* msg = "[LIVE]";
    uint8_t x = theme.centerTextX(msg, Fonts::SMALL, Display::WIDTH);
    display.drawText(x, previewY + previewHeight / 2 - 4, msg, Fonts::SMALL);
}

// ============================================================================
// CaptureScreen Implementation (with shutter animation and mini preview)
// ============================================================================

CaptureScreen::CaptureScreen()
    : enterTime_(0)
    , captureComplete_(false)
    , shutterFrame_(0)
    , lastFrameTime_(0)
    , shutterDone_(false)
{
}

void CaptureScreen::onEnter() {
    Serial.println("[CaptureScreen] Enter - starting shutter animation");
    enterTime_ = millis();
    captureComplete_ = false;
    shutterFrame_ = 0;
    lastFrameTime_ = millis();
    shutterDone_ = false;
}

void CaptureScreen::onExit() {
    Serial.println("[CaptureScreen] Exit");
}

void CaptureScreen::update() {
    // Update shutter animation
    if (!shutterDone_) {
        uint32_t now = millis();
        if (now - lastFrameTime_ >= ShutterAnim::FRAME_DURATION) {
            lastFrameTime_ = now;
            shutterFrame_++;
            if (shutterFrame_ >= ShutterAnim::FRAME_COUNT) {
                shutterDone_ = true;
            }
        }
    }
    
    // Mark complete after pipeline has finished
    const auto& stats = features::capture::getLastStats();
    if (stats.totalTimeMs > 0 && !captureComplete_) {
        captureComplete_ = true;
    }
}

void CaptureScreen::render() {
    auto& display = MockDisplay::instance();
    auto& theme = UiTheme::instance();
    
    display.clear();
    
    // Show shutter animation first if not done
    if (!shutterDone_) {
        renderShutterAnimation();
        display.display();
        return;
    }
    
    // Status bar
    StatusBarRenderer::render("CAPTURE");
    
    if (captureComplete_) {
        // Show confirmation with mini preview
        renderMiniPreview();
        renderStats();
        
        // Hint bar
        display.drawLine(0, theme.getHintBar().y - 1, Display::WIDTH, theme.getHintBar().y - 1);
        display.drawText(2, theme.getHintBar().y + 1, "Saved! TAP: Continue", Fonts::SMALL);
    } else {
        // Capture in progress (after shutter)
        const char* msg = "Processing...";
        uint8_t x = theme.centerTextX(msg, Fonts::MEDIUM, Display::WIDTH);
        display.drawText(x, 30, msg, Fonts::MEDIUM);
        
        // Progress indicator (simple dots)
        uint32_t elapsed = millis() - enterTime_;
        uint8_t dots = (elapsed / 200) % 4;
        char progress[8] = "    ";
        for (uint8_t i = 0; i < dots; i++) progress[i] = '.';
        display.drawText(60, 45, progress, Fonts::SMALL);
    }
    
    display.display();
}

void CaptureScreen::renderShutterAnimation() {
    auto& display = MockDisplay::instance();
    
    // Calculate shutter progress (0-255)
    uint8_t progress = (shutterFrame_ * 255) / ShutterAnim::FRAME_COUNT;
    
    // Draw shutter effect using mock display
    display.drawShutter(progress);
    
    // Center text during shutter
    if (shutterFrame_ < ShutterAnim::FRAME_COUNT / 2) {
        const char* msg = "CLICK!";
        auto& theme = UiTheme::instance();
        uint8_t x = theme.centerTextX(msg, Fonts::MEDIUM, Display::WIDTH);
        display.drawText(x, 28, msg, Fonts::MEDIUM);
    }
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
// ScreenManager Implementation (with fade transitions)
// ============================================================================

ScreenManager::ScreenManager()
    : currentScreen_(nullptr)
    , nextScreen_(nullptr)
    , transitionState_()
    , transitionDuration_(Timing::FADE_DURATION)
    , fadeLevel_(255)
    , pendingTransition_(TransitionType::NONE)
{
    for (size_t i = 0; i < static_cast<size_t>(ScreenId::SCREEN_COUNT); i++) {
        screens_[i] = nullptr;
    }
}

void ScreenManager::init() {
    Serial.println("[ScreenManager] Initialized with transition support");
}

void ScreenManager::registerScreen(IScreen* screen) {
    if (!screen) return;
    
    size_t idx = static_cast<size_t>(screen->getId());
    if (idx < static_cast<size_t>(ScreenId::SCREEN_COUNT)) {
        screens_[idx] = screen;
        Serial.printf("[ScreenManager] Registered screen %d\n", idx);
    }
}

void ScreenManager::setScreen(ScreenId id, TransitionType transition) {
    size_t idx = static_cast<size_t>(id);
    if (idx >= static_cast<size_t>(ScreenId::SCREEN_COUNT)) return;
    
    IScreen* newScreen = screens_[idx];
    if (!newScreen || newScreen == currentScreen_) return;
    
    nextScreen_ = newScreen;
    pendingTransition_ = transition;
    
    if (transition != TransitionType::NONE) {
        transitionState_.start(transition, transitionDuration_);
        Serial.printf("[ScreenManager] Starting transition type %d\n", static_cast<int>(transition));
    }
}

void ScreenManager::update() {
    // Handle transition animation
    if (transitionState_.active) {
        updateTransition();
    }
    
    // Handle screen switch when transition halfway done or no transition
    bool shouldSwitch = nextScreen_ && nextScreen_ != currentScreen_;
    bool transitionMidpoint = transitionState_.active && transitionState_.progress >= 128;
    bool noTransition = !transitionState_.active;
    
    if (shouldSwitch && (transitionMidpoint || noTransition)) {
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

void ScreenManager::updateTransition() {
    uint32_t elapsed = millis() - transitionState_.startTime;
    
    if (elapsed >= transitionState_.duration) {
        // Transition complete
        transitionState_.stop();
        fadeLevel_ = 255;  // Fully visible
        Serial.println("[ScreenManager] Transition complete");
    } else {
        // Calculate progress (0-255)
        transitionState_.progress = (elapsed * 255) / transitionState_.duration;
        
        // Fade out then fade in (V-shape curve)
        if (transitionState_.progress < 128) {
            // Fade out: 255 -> 0
            fadeLevel_ = 255 - (transitionState_.progress * 2);
        } else {
            // Fade in: 0 -> 255
            fadeLevel_ = (transitionState_.progress - 128) * 2;
        }
    }
}

void ScreenManager::render() {
    if (currentScreen_) {
        currentScreen_->render();
    }
    
    // Apply transition overlay if active
    if (transitionState_.active) {
        renderTransition();
    }
}

void ScreenManager::renderTransition() {
    auto& display = MockDisplay::instance();
    
    switch (transitionState_.type) {
        case TransitionType::FADE:
            // Fade effect using current fade level
            display.setFadeLevel(fadeLevel_);
            break;
            
        case TransitionType::SHUTTER:
            // Shutter effect
            display.drawShutter(transitionState_.progress);
            break;
            
        case TransitionType::SLIDE_LEFT:
        case TransitionType::SLIDE_UP:
            // TODO: Implement slide transitions
            display.setFadeLevel(fadeLevel_);
            break;
            
        default:
            break;
    }
}

ScreenId ScreenManager::getCurrentScreenId() const {
    return currentScreen_ ? currentScreen_->getId() : ScreenId::NONE;
}

} // namespace ui
} // namespace pxlcam
