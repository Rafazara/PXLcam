/**
 * @file palette.h
 * @brief Color Palette System for PXLcam v1.3.0
 * 
 * Provides the core palette infrastructure for stylized captures.
 * Supports built-in palettes (GameBoy, CGA, Sepia, NightVision) and
 * user-defined custom palettes with NVS persistence.
 * 
 * @version 1.3.0
 * @date 2024
 * 
 * Feature Flag: PXLCAM_FEATURE_CUSTOM_PALETTES
 */

#ifndef PXLCAM_FILTERS_PALETTE_H
#define PXLCAM_FILTERS_PALETTE_H

#include <Arduino.h>
#include <stdint.h>

// Feature gate
#ifndef PXLCAM_FEATURE_CUSTOM_PALETTES
#define PXLCAM_FEATURE_CUSTOM_PALETTES 1
#endif

namespace pxlcam {
namespace filters {

// =============================================================================
// Constants
// =============================================================================

/// Maximum number of tones per palette (retro style = 4)
constexpr uint8_t PALETTE_MAX_TONES = 4;

/// Maximum palette name length
constexpr uint8_t PALETTE_NAME_LEN = 16;

/// Number of built-in palettes
constexpr uint8_t BUILTIN_PALETTE_COUNT = 8;

/// Number of custom palette slots
constexpr uint8_t CUSTOM_PALETTE_SLOTS = 3;

// =============================================================================
// Palette Structure
// =============================================================================

/**
 * @brief Color palette definition for stylized output
 * 
 * Each palette contains up to 4 grayscale tones that define
 * the output levels for dithered/quantized images.
 */
struct Palette {
    uint8_t tones[PALETTE_MAX_TONES];   ///< Grayscale tone values (0-255)
    char name[PALETTE_NAME_LEN];         ///< Display name (null-terminated)
    uint8_t toneCount;                   ///< Active tones (2-4)
    bool isCustom;                       ///< true if user-defined
    
    /// Default constructor - GameBoy style
    Palette() : toneCount(4), isCustom(false) {
        tones[0] = 15;   // Darkest
        tones[1] = 86;   // Dark
        tones[2] = 168;  // Light  
        tones[3] = 255;  // Lightest
        strncpy(name, "Default", PALETTE_NAME_LEN);
    }
    
    /// Custom constructor
    Palette(const char* paletteName, uint8_t t0, uint8_t t1, uint8_t t2, uint8_t t3)
        : toneCount(4), isCustom(false) {
        tones[0] = t0;
        tones[1] = t1;
        tones[2] = t2;
        tones[3] = t3;
        strncpy(name, paletteName, PALETTE_NAME_LEN - 1);
        name[PALETTE_NAME_LEN - 1] = '\0';
    }
};

// =============================================================================
// Built-in Palette IDs
// =============================================================================

/**
 * @brief Enumeration of built-in palette types
 */
enum class PaletteType : uint8_t {
    GAMEBOY_CLASSIC = 0,    ///< Original DMG green-ish tones
    GAMEBOY_POCKET,         ///< GB Pocket grayscale
    CGA_MODE1,              ///< CGA 4-color mode 1
    CGA_MODE2,              ///< CGA 4-color mode 2
    SEPIA_VINTAGE,          ///< Warm sepia tones
    NIGHT_VISION,           ///< Green night vision style
    THERMAL,                ///< Thermal camera style
    HIGH_CONTRAST,          ///< Pure B&W high contrast
    
    // Custom slots
    CUSTOM_1 = 100,
    CUSTOM_2 = 101,
    CUSTOM_3 = 102,
    
    COUNT = BUILTIN_PALETTE_COUNT
};

// =============================================================================
// Palette Manager Interface
// =============================================================================

/**
 * @brief Initialize palette system
 * 
 * Loads built-in palettes and restores custom palettes from NVS.
 * 
 * @return true on success
 */
bool palette_init();

/**
 * @brief Get palette by type
 * 
 * @param type Palette type enum
 * @return Pointer to palette, or nullptr if invalid
 */
const Palette* palette_get(PaletteType type);

/**
 * @brief Get currently active palette
 * 
 * @return Pointer to active palette
 */
const Palette* palette_get_active();

/**
 * @brief Set active palette
 * 
 * @param type Palette type to activate
 * @return true on success
 */
bool palette_set_active(PaletteType type);

/**
 * @brief Cycle to next palette
 * 
 * @return New active palette type
 */
PaletteType palette_cycle_next();

/**
 * @brief Cycle to previous palette
 * 
 * @return New active palette type
 */
PaletteType palette_cycle_prev();

/**
 * @brief Get palette count (built-in + custom)
 * 
 * @return Total number of available palettes
 */
uint8_t palette_get_count();

/**
 * @brief Map grayscale value to nearest palette tone
 * 
 * @param gray Input grayscale value (0-255)
 * @return Mapped tone from active palette
 */
uint8_t palette_map_tone(uint8_t gray);

/**
 * @brief Map grayscale to palette tone index
 * 
 * @param gray Input grayscale value (0-255)
 * @return Index (0 to toneCount-1) of nearest tone
 */
uint8_t palette_map_index(uint8_t gray);

// =============================================================================
// Custom Palette Management
// =============================================================================

#if PXLCAM_FEATURE_CUSTOM_PALETTES

/**
 * @brief Save custom palette to NVS
 * 
 * @param slot Custom slot index (0-2)
 * @param palette Palette data to save
 * @return true on success
 */
bool palette_save_custom(uint8_t slot, const Palette& palette);

/**
 * @brief Load custom palette from NVS
 * 
 * @param slot Custom slot index (0-2)
 * @param palette Output palette data
 * @return true on success
 */
bool palette_load_custom(uint8_t slot, Palette& palette);

/**
 * @brief Delete custom palette
 * 
 * @param slot Custom slot index (0-2)
 * @return true on success
 */
bool palette_delete_custom(uint8_t slot);

#endif // PXLCAM_FEATURE_CUSTOM_PALETTES

// =============================================================================
// TODOs for v1.3.0 Implementation
// =============================================================================

// TODO: Implement palette_init() with NVS loading
// TODO: Implement palette cycling with wraparound
// TODO: Add RGB palette support for color output
// TODO: Implement palette preview in menu
// TODO: Add palette import/export via serial

} // namespace filters
} // namespace pxlcam

#endif // PXLCAM_FILTERS_PALETTE_H
