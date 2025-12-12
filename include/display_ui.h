#pragma once
/**
 * @file display_ui.h
 * @brief UI/UX system for PXLcam v1.2.0
 * 
 * Provides elegant, minimalista UI components:
 * - Status bar with mode, FPS, memory indicator
 * - Error screens with auto-timeout
 * - Success animations (blink)
 * - Menu transitions (fade)
 * - 1-bit preview rendering
 * - Toast notifications
 */

#include <stdint.h>

namespace pxlcam::display {

//==============================================================================
// Enums & Constants
//==============================================================================

/// Icon identifiers for status bar
enum class IconId : uint8_t {
    SD_Empty = 0,
    SD_Present = 1,
    Battery0 = 2,
    Battery25 = 3,
    Battery50 = 4,
    Battery75 = 5,
    Battery100 = 6,
    ModeNormal = 7,
    ModeGameBoy = 8,
    ModeNight = 9,
    Recording = 10,
    MemLow = 11,
    MemOk = 12
};

/// Preview mode names for display
enum class PreviewMode : uint8_t {
    Normal = 0,
    GameBoy = 1,
    Night = 2
};

/// Toast types for styling
enum class ToastType : uint8_t {
    Info = 0,
    Success = 1,
    Warning = 2,
    Error = 3
};

/// UI Layout constants
struct UILayout {
    static constexpr int ScreenWidth = 128;
    static constexpr int ScreenHeight = 64;
    static constexpr int StatusBarHeight = 10;
    static constexpr int StatusBarY = 0;
    static constexpr int PreviewX = 32;           ///< Centered on 128px display
    static constexpr int PreviewY = 12;
    static constexpr int PreviewW = 64;
    static constexpr int PreviewH = 48;  
    static constexpr int HintBarY = 54;
    static constexpr int HintBarHeight = 10;
    static constexpr int IconSize = 8;
    static constexpr int IconSpacing = 10;
    static constexpr int ToastY = 20;
    static constexpr int ToastH = 24;
};

/// Default timeouts (ms)
struct UITimeout {
    static constexpr uint32_t Error = 3000;
    static constexpr uint32_t Success = 1500;
    static constexpr uint32_t Toast = 2000;
    static constexpr uint32_t Info = 2500;
};

//==============================================================================
// Core UI Functions
//==============================================================================

/// Initialize UI system
void initUI();

/// Update UI (check timeouts, animations) - call in main loop
void updateUI();

/// Check if UI has active overlay (toast, error, etc.)
bool hasActiveOverlay();

//==============================================================================
// Error Screens
//==============================================================================

/// Show error screen with message (auto-timeout)
/// @param message Error message to display
/// @param timeoutMs Auto-dismiss time (0 = manual dismiss)
void showError(const char* message, uint32_t timeoutMs = UITimeout::Error);

/// Show error with title and message
/// @param title Error title (e.g., "SD Error")
/// @param message Error details
/// @param timeoutMs Auto-dismiss time
void showErrorFull(const char* title, const char* message, uint32_t timeoutMs = UITimeout::Error);

/// Clear error screen
void clearError();

/// Check if error is currently displayed
bool isErrorVisible();

//==============================================================================
// Success Animation
//==============================================================================

/// Show success animation (quick blink)
/// @param message Optional message (e.g., "Saved!")
void showSuccess(const char* message = nullptr);

/// Show success with filename
/// @param filename Saved filename
void showSuccessFile(const char* filename);

//==============================================================================
// Menu Transitions
//==============================================================================

/// Fade in transition (for menu open)
/// @param steps Number of fade steps (1-5)
/// @param delayMs Delay between steps
void transitionFadeIn(uint8_t steps = 3, uint16_t delayMs = 30);

/// Fade out transition (for menu close)
/// @param steps Number of fade steps
/// @param delayMs Delay between steps
void transitionFadeOut(uint8_t steps = 3, uint16_t delayMs = 30);

/// Slide transition (menu items)
void transitionSlide();

//==============================================================================
// Status Bar
//==============================================================================

/// Draw full status bar
/// @param fps Current FPS
/// @param sdPresent SD card present
/// @param mode Current capture mode
/// @param freeHeapKb Free heap in KB
void drawStatusBar(int fps, bool sdPresent, PreviewMode mode, uint16_t freeHeapKb);

/// Draw simplified status bar (mode + indicator only)
/// @param mode Current mode
/// @param indicator Right-side indicator text
void drawStatusBarSimple(PreviewMode mode, const char* indicator);

/// Draw mode indicator only
/// @param mode Current mode
void drawModeIndicator(PreviewMode mode);

//==============================================================================
// Toast Notifications
//==============================================================================

/// Show toast notification (auto-timeout)
/// @param message Toast message
/// @param type Toast type for styling
/// @param timeoutMs Duration
void showToast(const char* message, ToastType type = ToastType::Info, 
               uint32_t timeoutMs = UITimeout::Toast);

/// Clear any active toast
void clearToast();

//==============================================================================
// Quick Feedback (v1.2.0)
//==============================================================================

/// Show quick feedback message (positioned top-right, auto-timeout)
/// @param message Short feedback message (e.g., "Saved!", "Style: GameBoy")
/// @param timeoutMs Display duration (default 1500ms)
void showQuickFeedback(const char* message, uint32_t timeoutMs = 1500);

/// Show mode change feedback
/// @param modeName Name of new mode
void showModeFeedback(const char* modeName);

/// Show "Saved!" feedback
void showSavedFeedback();

/// Clear quick feedback
void clearQuickFeedback();

//==============================================================================
// Status Indicator (v1.2.0 - top-right corner)
//==============================================================================

/// Status indicator types
enum class StatusIndicator : uint8_t {
    None = 0,
    Ready,      ///< Green dot - ready
    Busy,       ///< Yellow dot - processing
    Error,      ///< Red dot - error state
    Recording   ///< Blinking red - capturing
};

/// Set status indicator (top-right corner)
/// @param status Status type
void setStatusIndicator(StatusIndicator status);

/// Get current status indicator
StatusIndicator getStatusIndicator();

/// Draw status indicator at current state
void drawStatusIndicator();

//==============================================================================
// Progress & Loading
//==============================================================================

/// Show progress bar
/// @param title Operation name
/// @param progress Percentage (0-100)
void showProgress(const char* title, uint8_t progress);

/// Show indeterminate loading animation
/// @param message Loading message
void showLoading(const char* message);

/// Clear progress/loading display
void clearProgress();

//==============================================================================
// Preview Area
//==============================================================================

/// Draw 1-bit preview bitmap (packed)
/// @param packedBitmap 1-bit packed bitmap
/// @param w Width
/// @param h Height
void drawPreviewBitmap(const uint8_t* packedBitmap, int w, int h);

/// Draw preview frame border
void drawPreviewFrame();

/// Clear preview area only
void clearPreviewArea();

//==============================================================================
// Utility Functions
//==============================================================================

/// Draw icon at position
void drawIcon(int16_t x, int16_t y, IconId iconId);

/// Draw small text
void drawSmallText(int16_t x, int16_t y, const char* text);

/// Draw centered text
void drawCenteredText(int16_t y, const char* text);

/// Draw hint bar at bottom
void drawHintBar(const char* hintText);

/// Swap display buffers
void swapDisplayBuffers();

/// Get mode display character
/// @param mode Preview mode
/// @return Single character: 'N', 'G', or 'X'
char getModeChar(PreviewMode mode);

/// Get mode name string
const char* getModeName(PreviewMode mode);

/// Full UI refresh
void refreshFullUI(int fps, bool sdPresent, PreviewMode mode, const char* hint);

}  // namespace pxlcam::display
