/**
 * @file ui_theme.h
 * @brief UI Theme Definitions for PXLcam v1.2.0
 * 
 * Defines visual styling for the user interface:
 * - Font sizes and styles
 * - Layout dimensions
 * - Color schemes
 * - Status bar and hint bar configurations
 * 
 * @author PXLcam Team
 * @version 1.2.0
 * @date 2024
 */

#ifndef PXLCAM_UI_THEME_H
#define PXLCAM_UI_THEME_H

#include <cstdint>

namespace pxlcam {
namespace ui {

/**
 * @brief Display dimensions (SSD1306 128x64)
 */
namespace Display {
    constexpr uint16_t WIDTH = 128;     ///< Display width in pixels
    constexpr uint16_t HEIGHT = 64;     ///< Display height in pixels
    constexpr uint8_t I2C_ADDR = 0x3C;  ///< I2C address
}

/**
 * @brief Font size enumeration
 */
enum class FontSize : uint8_t {
    SMALL = 0,      ///< 6x8 pixels (default system font)
    MEDIUM,         ///< 8x16 pixels (2x scale or custom)
    LARGE,          ///< 12x24 pixels (3x scale or custom)
    FONT_COUNT
};

/**
 * @brief Font configuration structure
 */
struct FontConfig {
    uint8_t width;          ///< Character width in pixels
    uint8_t height;         ///< Character height in pixels
    uint8_t spacing;        ///< Character spacing
    uint8_t lineSpacing;    ///< Line spacing
    uint8_t scale;          ///< Text scale factor (for Adafruit GFX)
};

/**
 * @brief Predefined font configurations
 */
namespace Fonts {
    /// Small font (6x8, scale 1) - For status bars and hints
    constexpr FontConfig SMALL = {
        .width = 6,
        .height = 8,
        .spacing = 1,
        .lineSpacing = 2,
        .scale = 1
    };

    /// Medium font (12x16, scale 2) - For menu items
    constexpr FontConfig MEDIUM = {
        .width = 12,
        .height = 16,
        .spacing = 2,
        .lineSpacing = 4,
        .scale = 2
    };

    /// Large font (18x24, scale 3) - For titles and important info
    constexpr FontConfig LARGE = {
        .width = 18,
        .height = 24,
        .spacing = 3,
        .lineSpacing = 6,
        .scale = 3
    };

    /**
     * @brief Get font config by size enum
     * @param size Font size
     * @return const FontConfig& Font configuration
     */
    inline const FontConfig& getFont(FontSize size) {
        switch (size) {
            case FontSize::MEDIUM: return MEDIUM;
            case FontSize::LARGE:  return LARGE;
            default:               return SMALL;
        }
    }
}

/**
 * @brief Status bar layout configuration
 */
struct StatusBarLayout {
    uint8_t x;              ///< X position
    uint8_t y;              ///< Y position
    uint8_t width;          ///< Width
    uint8_t height;         ///< Height
    uint8_t padding;        ///< Internal padding
    uint8_t iconSize;       ///< Icon size
    uint8_t iconSpacing;    ///< Space between icons
    bool showBattery;       ///< Show battery indicator
    bool showMode;          ///< Show current mode
    bool showStorage;       ///< Show storage indicator
};

/**
 * @brief Hint bar layout configuration
 */
struct HintBarLayout {
    uint8_t x;              ///< X position
    uint8_t y;              ///< Y position
    uint8_t width;          ///< Width
    uint8_t height;         ///< Height
    uint8_t padding;        ///< Internal padding
    uint8_t maxHints;       ///< Maximum hints to display
    bool showSeparator;     ///< Show separator line
};

/**
 * @brief Menu layout configuration
 */
struct MenuLayout {
    uint8_t x;              ///< X position
    uint8_t y;              ///< Y position
    uint8_t width;          ///< Width
    uint8_t height;         ///< Height
    uint8_t itemHeight;     ///< Height per menu item
    uint8_t visibleItems;   ///< Number of visible items
    uint8_t padding;        ///< Internal padding
    uint8_t scrollbarWidth; ///< Scrollbar width (0 to hide)
    bool showTitle;         ///< Show menu title
    bool showScrollbar;     ///< Show scrollbar
};

/**
 * @brief UI Theme class
 * 
 * Central theme configuration for the UI.
 * Provides consistent styling across all screens.
 */
class UiTheme {
public:
    /**
     * @brief Get singleton instance
     * @return UiTheme& Reference to theme
     */
    static UiTheme& instance() {
        static UiTheme theme;
        return theme;
    }

    // Delete copy/move for singleton
    UiTheme(const UiTheme&) = delete;
    UiTheme& operator=(const UiTheme&) = delete;

    /**
     * @brief Initialize theme with defaults
     */
    void init();

    // Accessors
    const StatusBarLayout& getStatusBar() const { return statusBar_; }
    const HintBarLayout& getHintBar() const { return hintBar_; }
    const MenuLayout& getMenuLayout() const { return menuLayout_; }

    // Font helpers
    const FontConfig& getSmallFont() const { return Fonts::SMALL; }
    const FontConfig& getMediumFont() const { return Fonts::MEDIUM; }
    const FontConfig& getLargeFont() const { return Fonts::LARGE; }
    const FontConfig& getFont(FontSize size) const { return Fonts::getFont(size); }

    // Layout calculation helpers
    
    /**
     * @brief Calculate content area (between status and hint bars)
     * @param x Output X position
     * @param y Output Y position
     * @param w Output width
     * @param h Output height
     */
    void getContentArea(uint8_t& x, uint8_t& y, uint8_t& w, uint8_t& h) const;

    /**
     * @brief Calculate text width
     * @param text Text string
     * @param font Font configuration
     * @return uint16_t Width in pixels
     */
    uint16_t calculateTextWidth(const char* text, const FontConfig& font) const;

    /**
     * @brief Calculate centered X position for text
     * @param text Text string
     * @param font Font configuration
     * @param areaWidth Area width
     * @return uint8_t X position for centered text
     */
    uint8_t centerTextX(const char* text, const FontConfig& font, uint8_t areaWidth) const;

private:
    UiTheme();

    StatusBarLayout statusBar_;
    HintBarLayout hintBar_;
    MenuLayout menuLayout_;
};

/**
 * @brief Common UI colors (for OLED: 0=black, 1=white)
 */
namespace Colors {
    constexpr uint8_t BLACK = 0;
    constexpr uint8_t WHITE = 1;
    constexpr uint8_t INVERSE = 2;  // Invert pixels
}

/**
 * @brief UI spacing constants
 */
namespace Spacing {
    constexpr uint8_t TINY = 1;
    constexpr uint8_t SMALL = 2;
    constexpr uint8_t MEDIUM = 4;
    constexpr uint8_t LARGE = 8;
    constexpr uint8_t XLARGE = 16;
}

/**
 * @brief UI animation timing (milliseconds)
 */
namespace Timing {
    constexpr uint16_t CURSOR_BLINK = 500;
    constexpr uint16_t MENU_SCROLL = 100;
    constexpr uint16_t SPLASH_DURATION = 2500;
    constexpr uint16_t HINT_FADE = 300;
    constexpr uint16_t HINT_AUTO_HIDE = 3000;    ///< Auto-hide hint bar after inactivity
    constexpr uint16_t DEBOUNCE = 50;
    constexpr uint16_t FADE_DURATION = 200;      ///< Screen fade transition duration
    constexpr uint16_t SHUTTER_ANIMATION = 150;  ///< Shutter animation frame duration
    constexpr uint16_t MODE_INDICATOR_BLINK = 800; ///< Mode indicator blink rate
}

/**
 * @brief Animation types for screen transitions
 */
enum class TransitionType : uint8_t {
    NONE = 0,       ///< No transition
    FADE,           ///< Fade in/out
    SLIDE_LEFT,     ///< Slide from right to left
    SLIDE_UP,       ///< Slide from bottom to top
    SHUTTER         ///< Camera shutter effect
};

/**
 * @brief Animation state for UI transitions
 */
struct AnimationState {
    TransitionType type;
    uint32_t startTime;
    uint16_t duration;
    bool active;
    uint8_t progress;   ///< 0-255 progress
    
    void start(TransitionType t, uint16_t dur = Timing::FADE_DURATION) {
        type = t;
        startTime = 0;  // Set by caller with millis()
        duration = dur;
        active = true;
        progress = 0;
    }
    
    void stop() {
        active = false;
        progress = 255;
    }
    
    void reset() {
        type = TransitionType::NONE;
        startTime = 0;
        duration = 0;
        active = false;
        progress = 0;
    }
};

/**
 * @brief Shutter animation frames (camera capture effect)
 */
namespace ShutterAnim {
    constexpr uint8_t FRAME_COUNT = 4;
    constexpr uint8_t FRAME_DURATION = Timing::SHUTTER_ANIMATION;
    
    // Shutter closing pattern (percentage of screen covered)
    constexpr uint8_t FRAMES[FRAME_COUNT] = {25, 50, 75, 100};
}

/**
 * @brief Status bar icons (ASCII representations for mock display)
 */
namespace Icons {
    constexpr const char* BATTERY_FULL = "[###]";
    constexpr const char* BATTERY_MID = "[## ]";
    constexpr const char* BATTERY_LOW = "[#  ]";
    constexpr const char* BATTERY_EMPTY = "[   ]";
    constexpr const char* STORAGE_OK = "SD";
    constexpr const char* STORAGE_NONE = "--";
    constexpr const char* WIFI_ON = "W";
    constexpr const char* WIFI_OFF = " ";
}

} // namespace ui
} // namespace pxlcam

#endif // PXLCAM_UI_THEME_H
