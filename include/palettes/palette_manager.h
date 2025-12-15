/**
 * @file palette_manager.h
 * @brief Color Palette Management for PXLcam v1.3.0
 * 
 * Manages multiple color palettes for stylized image output.
 * Supports built-in palettes (GameBoy, CGA, Sepia, etc.) and
 * user-defined custom palettes stored in NVS.
 * 
 * @version 1.3.0
 * @date 2024
 */

#ifndef PALETTE_MANAGER_H
#define PALETTE_MANAGER_H

#include <Arduino.h>

namespace pxlcam {
namespace palettes {

/**
 * @brief Built-in palette identifiers
 */
enum class PaletteId : uint8_t {
    GAMEBOY_CLASSIC = 0,    ///< Original GameBoy green
    GAMEBOY_POCKET,         ///< GameBoy Pocket grayscale
    GAMEBOY_LIGHT,          ///< GameBoy Light yellowish
    CGA_MODE4_CYAN,         ///< CGA cyan/magenta/white
    CGA_MODE4_GREEN,        ///< CGA green/red/yellow
    SEPIA_VINTAGE,          ///< Vintage sepia tone
    THERMAL_VISION,         ///< Thermal camera style
    NEON_SYNTHWAVE,         ///< 80s synthwave neon
    MONOCHROME_BW,          ///< Pure black & white
    CUSTOM_SLOT_1,          ///< User-defined slot 1
    CUSTOM_SLOT_2,          ///< User-defined slot 2
    CUSTOM_SLOT_3,          ///< User-defined slot 3
    PALETTE_COUNT
};

/**
 * @brief RGBA color structure
 */
struct Color {
    uint8_t r, g, b, a;
    
    Color() : r(0), g(0), b(0), a(255) {}
    Color(uint8_t _r, uint8_t _g, uint8_t _b, uint8_t _a = 255)
        : r(_r), g(_g), b(_b), a(_a) {}
    
    uint16_t toRGB565() const;
    uint32_t toRGB888() const;
};

/**
 * @brief Palette definition (4-color for retro styles)
 */
struct Palette {
    static constexpr uint8_t MAX_COLORS = 4;
    
    char name[16];                      ///< Palette display name
    Color colors[MAX_COLORS];           ///< Color entries
    uint8_t colorCount;                 ///< Number of colors (2-4)
    bool isCustom;                      ///< true if user-defined
};

/**
 * @brief Initialize palette manager
 * @return true on success
 */
bool init();

/**
 * @brief Get palette by ID
 * @param id Palette identifier
 * @return Pointer to palette or nullptr
 */
const Palette* getPalette(PaletteId id);

/**
 * @brief Get current active palette
 * @return Pointer to active palette
 */
const Palette* getActivePalette();

/**
 * @brief Set active palette
 * @param id Palette identifier
 * @return true on success
 */
bool setActivePalette(PaletteId id);

/**
 * @brief Cycle to next palette
 * @return New active palette ID
 */
PaletteId cycleNextPalette();

/**
 * @brief Cycle to previous palette
 * @return New active palette ID
 */
PaletteId cyclePrevPalette();

/**
 * @brief Save custom palette to NVS
 * @param slot Custom slot (0-2)
 * @param palette Palette data
 * @return true on success
 */
bool saveCustomPalette(uint8_t slot, const Palette& palette);

/**
 * @brief Load custom palette from NVS
 * @param slot Custom slot (0-2)
 * @param palette Output palette data
 * @return true on success
 */
bool loadCustomPalette(uint8_t slot, Palette& palette);

/**
 * @brief Map grayscale value to palette color
 * @param gray Grayscale value (0-255)
 * @return RGB565 color from active palette
 */
uint16_t mapToPalette(uint8_t gray);

/**
 * @brief Get palette count (including custom)
 * @return Number of available palettes
 */
uint8_t getPaletteCount();

} // namespace palettes
} // namespace pxlcam

#endif // PALETTE_MANAGER_H
