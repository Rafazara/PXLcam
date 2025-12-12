/**
 * @file ui_screens.h
 * @brief UI Screen Components for PXLcam v1.2.0
 * 
 * Abstract screen rendering interface and common screen implementations.
 * Uses mocks for display output during development.
 * 
 * @author PXLcam Team
 * @version 1.2.0
 * @date 2024
 */

#ifndef PXLCAM_UI_SCREENS_H
#define PXLCAM_UI_SCREENS_H

#include <cstdint>
#include <functional>
#include "ui_theme.h"
#include "../features/menu_system.h"
#include "../features/capture_pipeline.h"

namespace pxlcam {
namespace ui {

/**
 * @brief Screen identifiers
 */
enum class ScreenId : uint8_t {
    NONE = 0,
    SPLASH,         ///< Boot splash screen
    IDLE,           ///< Idle/standby screen
    MENU,           ///< Menu screen
    PREVIEW,        ///< Camera preview screen
    CAPTURE,        ///< Capture in progress screen
    SETTINGS,       ///< Settings screen
    ABOUT,          ///< About/info screen
    ERROR,          ///< Error screen
    SCREEN_COUNT
};

/**
 * @brief Abstract screen interface
 */
class IScreen {
public:
    virtual ~IScreen() = default;

    /**
     * @brief Called when screen becomes active
     */
    virtual void onEnter() = 0;

    /**
     * @brief Called when screen becomes inactive
     */
    virtual void onExit() = 0;

    /**
     * @brief Update screen state (called each frame)
     */
    virtual void update() = 0;

    /**
     * @brief Render screen content
     */
    virtual void render() = 0;

    /**
     * @brief Get screen identifier
     * @return ScreenId Screen ID
     */
    virtual ScreenId getId() const = 0;
};

/**
 * @brief Mock display output (prints to Serial)
 * 
 * Simulates display rendering during development.
 */
class MockDisplay {
public:
    static MockDisplay& instance() {
        static MockDisplay display;
        return display;
    }

    void clear();
    void drawText(uint8_t x, uint8_t y, const char* text, const FontConfig& font);
    void drawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool filled = false);
    void drawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);
    void drawPixel(uint8_t x, uint8_t y, uint8_t color);
    void setTextSize(uint8_t size);
    void setCursor(uint8_t x, uint8_t y);
    void print(const char* text);
    void display();  // Commit buffer to "screen"
    
    // Animation support
    void setFadeLevel(uint8_t level);  ///< 0=black, 255=full brightness
    uint8_t getFadeLevel() const { return fadeLevel_; }
    void drawShutter(uint8_t closePercent);  ///< Draw shutter overlay

    // Status tracking
    bool isDirty() const { return dirty_; }
    void setDirty(bool dirty) { dirty_ = dirty; }

private:
    MockDisplay() : dirty_(false), textSize_(1), cursorX_(0), cursorY_(0), fadeLevel_(255) {}
    bool dirty_;
    uint8_t textSize_;
    uint8_t cursorX_;
    uint8_t cursorY_;
    uint8_t fadeLevel_;
};

/**
 * @brief Common status bar renderer (shared across screens)
 */
class StatusBarRenderer {
public:
    static void render(const char* modeText, float fps = 0.0f);
    static void renderBattery(uint8_t x, uint8_t y);
    static void renderClock(uint8_t x, uint8_t y);
    static void renderFps(uint8_t x, uint8_t y, float fps);
};

/**
 * @brief Hint bar with auto-hide support
 */
class HintBar {
public:
    static HintBar& instance() {
        static HintBar bar;
        return bar;
    }
    
    void show(const char* hint);
    void hide();
    void update();
    void render();
    
    void resetAutoHide();
    bool isVisible() const { return visible_ && !fadeOut_; }
    
private:
    HintBar() : hint_(nullptr), visible_(false), lastActivityTime_(0), fadeOut_(false), fadeProgress_(0) {}
    
    const char* hint_;
    bool visible_;
    uint32_t lastActivityTime_;
    bool fadeOut_;
    uint8_t fadeProgress_;
};

/**
 * @brief Splash screen implementation
 */
class SplashScreen : public IScreen {
public:
    SplashScreen();
    
    void onEnter() override;
    void onExit() override;
    void update() override;
    void render() override;
    ScreenId getId() const override { return ScreenId::SPLASH; }

    bool isComplete() const { return complete_; }

private:
    void renderFadeEffect();
    void renderLoadingBar();
    
    uint32_t startTime_;
    bool complete_;
    uint8_t fadePhase_;  ///< 0=fade in, 1=display, 2=fade out
};

/**
 * @brief Idle screen implementation
 */
class IdleScreen : public IScreen {
public:
    IdleScreen();
    
    void onEnter() override;
    void onExit() override;
    void update() override;
    void render() override;
    ScreenId getId() const override { return ScreenId::IDLE; }

    void setStatusText(const char* text);

private:
    const char* statusText_;
    uint32_t lastUpdateTime_;
};

/**
 * @brief Menu screen implementation
 */
class MenuScreen : public IScreen {
public:
    explicit MenuScreen(features::MenuSystem* menuSystem);
    
    void onEnter() override;
    void onExit() override;
    void update() override;
    void render() override;
    ScreenId getId() const override { return ScreenId::MENU; }

private:
    void renderTitle();
    void renderItems();
    void renderHints();
    void renderScrollbar();

    features::MenuSystem* menuSystem_;
    uint8_t scrollOffset_;
};

/**
 * @brief Preview screen implementation
 */
class PreviewScreen : public IScreen {
public:
    PreviewScreen();
    
    void onEnter() override;
    void onExit() override;
    void update() override;
    void render() override;
    ScreenId getId() const override { return ScreenId::PREVIEW; }

    void setFps(float fps) { fps_ = fps; }

private:
    void renderStatusBar();
    void renderPreviewArea();
    
    float fps_;
    uint32_t frameCount_;
    uint32_t lastHintTime_;
};

/**
 * @brief Capture screen implementation (with shutter animation and mini preview)
 */
class CaptureScreen : public IScreen {
public:
    CaptureScreen();
    
    void onEnter() override;
    void onExit() override;
    void update() override;
    void render() override;
    ScreenId getId() const override { return ScreenId::CAPTURE; }

private:
    void renderShutterAnimation();
    void renderMiniPreview();
    void renderStats();
    void renderConfirmation();
    
    uint32_t enterTime_;
    bool captureComplete_;
    uint8_t shutterFrame_;      ///< Current shutter animation frame
    uint32_t lastFrameTime_;    ///< For animation timing
    bool shutterDone_;          ///< Shutter animation complete
};

/**
 * @brief Screen Manager
 * 
 * Manages screen transitions and rendering with fade effects.
 */
class ScreenManager {
public:
    static ScreenManager& instance() {
        static ScreenManager mgr;
        return mgr;
    }

    void init();
    void update();
    void render();

    void setScreen(ScreenId id, TransitionType transition = TransitionType::FADE);
    void registerScreen(IScreen* screen);
    
    IScreen* getCurrentScreen() { return currentScreen_; }
    ScreenId getCurrentScreenId() const;
    
    // Transition control
    bool isTransitioning() const { return transitionState_.active; }
    void setTransitionDuration(uint16_t ms) { transitionDuration_ = ms; }

private:
    ScreenManager();
    
    void updateTransition();
    void renderTransition();

    IScreen* screens_[static_cast<size_t>(ScreenId::SCREEN_COUNT)];
    IScreen* currentScreen_;
    IScreen* nextScreen_;
    
    // Transition state
    AnimationState transitionState_;
    uint16_t transitionDuration_;
    uint8_t fadeLevel_;             ///< Current fade level for transitions
    TransitionType pendingTransition_;
};

} // namespace ui
} // namespace pxlcam

#endif // PXLCAM_UI_SCREENS_H
