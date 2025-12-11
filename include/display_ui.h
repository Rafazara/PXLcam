#pragma once
/**
 * @file display_ui.h
 * @brief UI overlay system for PXLcam v1.1.0
 * 
 * Provides:
 * - Status bar with icons (SD, battery, mode, FPS)
 * - 1-bit preview bitmap rendering
 * - Double buffer swap support
 * - Small text overlays
 */

#include <stdint.h>

namespace pxlcam::display {

/// Icon identifiers for status bar
enum class IconId : uint8_t {
    SD_Empty = 0,
    SD_Present = 1,
    Battery0 = 2,
    Battery25 = 3,
    Battery50 = 4,
    Battery75 = 5,
    Battery100 = 6,
    ModeAuto = 7,
    ModeGameBoy = 8,
    ModeNight = 9,
    Recording = 10
};

/// Preview mode names for display
enum class PreviewMode : uint8_t {
    Auto = 0,
    GameBoy = 1,
    Night = 2
};

/// UI Layout constants
struct UILayout {
    static constexpr int StatusBarHeight = 10;
    static constexpr int StatusBarY = 0;
    static constexpr int PreviewX = 32;           ///< Centered on 128px display
    static constexpr int PreviewY = 12;
    static constexpr int PreviewW = 64;
    static constexpr int PreviewH = 64;  
    static constexpr int HintBarY = 54;
    static constexpr int HintBarHeight = 10;
    static constexpr int IconSize = 8;
    static constexpr int IconSpacing = 10;
};

/// Initialize UI system (fonts, icons, etc.)
void initUI();

/// Draw a small monochrome icon (8x8)
/// @param x X position
/// @param y Y position
/// @param iconId Icon identifier
void drawIcon(int16_t x, int16_t y, IconId iconId);

/// Draw the top status bar
/// @param fps Current FPS
/// @param sdPresent SD card detected
/// @param batteryPct Battery percentage (0-100)
/// @param mode Current preview mode
void drawStatusBar(int fps, bool sdPresent, uint8_t batteryPct, PreviewMode mode);

/// Draw 1-bit preview bitmap (packed) into centered region
/// @param packedBitmap 1-bit packed bitmap (row-major, MSB-first)
/// @param w Width (typically 64)
/// @param h Height (typically 64)
void drawPreviewBitmap(const uint8_t* packedBitmap, int w, int h);

/// Draw preview frame border
void drawPreviewFrame();

/// Swap display buffers (if double buffering enabled)
void swapDisplayBuffers();

/// Draw small text at position
/// @param x X position
/// @param y Y position  
/// @param text Text to display
void drawSmallText(int16_t x, int16_t y, const char* text);

/// Draw bottom hint bar with current controls
/// @param hintText Text to display
void drawHintBar(const char* hintText);

/// Clear only the preview area (for fast refresh)
void clearPreviewArea();

/// Full UI refresh (status + preview frame + hints)
void refreshFullUI(int fps, bool sdPresent, uint8_t batteryPct, PreviewMode mode, const char* hint);

/// Get mode name string
const char* getModeName(PreviewMode mode);

}  // namespace pxlcam::display
