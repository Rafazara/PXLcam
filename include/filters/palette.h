/**
 * @file palette.h
 * @brief Official Color Palette System for PXLcam v1.3.0
 * 
 * @details
 * This module provides the core palette infrastructure for the PXLcam
 * stylized capture pipeline. It manages a collection of 4-tone grayscale
 * palettes used for retro-style image processing.
 * 
 * The palette system supports:
 * - 8 built-in palettes (GameBoy, CGA, Sepia, etc.)
 * - 3 custom palette slots for user-defined palettes
 * - Safe access with automatic fallback to GB_CLASSIC
 * - Future expandability for JSON import/export
 * 
 * @section usage Usage Example
 * @code
 * #include "filters/palette.h"
 * 
 * void setup() {
 *     pxlcam::filters::palette_init();
 *     
 *     const auto& pal = pxlcam::filters::palette_get(PaletteType::GB_CLASSIC);
 *     Serial.printf("Palette: %s\n", pal.name);
 *     Serial.printf("Tones: %d, %d, %d, %d\n", 
 *         pal.tones[0], pal.tones[1], pal.tones[2], pal.tones[3]);
 * }
 * @endcode
 * 
 * @author PXLcam Team
 * @version 1.3.0
 * @date 2024
 * 
 * @copyright MIT License
 */

#ifndef PXLCAM_FILTERS_PALETTE_H
#define PXLCAM_FILTERS_PALETTE_H

#include <stdint.h>
#include <stddef.h>

namespace pxlcam {
namespace filters {

// =============================================================================
// Constants
// =============================================================================

/**
 * @brief Number of tones per palette (fixed at 4 for retro aesthetics)
 * 
 * The 4-tone palette is a deliberate design choice to emulate classic
 * hardware limitations like the GameBoy's 4-shade display.
 */
constexpr uint8_t PALETTE_TONE_COUNT = 4;

/**
 * @brief Total number of built-in palettes
 */
constexpr uint8_t BUILTIN_PALETTE_COUNT = 8;

/**
 * @brief Number of custom palette slots available
 */
constexpr uint8_t CUSTOM_PALETTE_COUNT = 3;

/**
 * @brief Total number of palettes (built-in + custom)
 */
constexpr uint8_t TOTAL_PALETTE_COUNT = BUILTIN_PALETTE_COUNT + CUSTOM_PALETTE_COUNT;

/**
 * @brief Maximum length for palette names (including null terminator)
 */
constexpr uint8_t PALETTE_NAME_MAX_LEN = 16;

// =============================================================================
// Palette Type Enumeration
// =============================================================================

/**
 * @brief Enumeration of all available palette types
 * 
 * @details
 * Palettes are divided into two categories:
 * 
 * **Built-in Palettes (0-7):**
 * Predefined, read-only palettes optimized for specific visual styles.
 * 
 * **Custom Palettes (8-10):**
 * User-configurable slots that can be modified at runtime and
 * persisted to NVS flash storage.
 * 
 * @note The enum values are designed to be used as array indices.
 */
enum class PaletteType : uint8_t {
    // =========================================================================
    // Built-in Palettes (Read-Only)
    // =========================================================================
    
    /**
     * @brief Original GameBoy DMG palette
     * 
     * The iconic green-tinted 4-tone palette from the 1989 Nintendo GameBoy.
     * Tones: 0x0F (darkest) → 0x56 → 0x9B → 0xCF (lightest)
     */
    GB_CLASSIC = 0,
    
    /**
     * @brief GameBoy Pocket palette
     * 
     * Pure grayscale palette from the 1996 GameBoy Pocket.
     * Tones: 0x00 (black) → 0x55 → 0xAA → 0xFF (white)
     */
    GB_POCKET = 1,
    
    /**
     * @brief CGA Mode 1 palette (Cyan/Magenta)
     * 
     * Inspired by IBM CGA graphics adapter palette 1.
     * Grayscale representation of the cyan-magenta-white scheme.
     * Tones: 0x00 → 0x40 → 0xAA → 0xFF
     */
    CGA_MODE1 = 2,
    
    /**
     * @brief CGA Mode 2 palette (Green/Red)
     * 
     * Inspired by IBM CGA graphics adapter palette 2.
     * Grayscale representation of the green-red-yellow scheme.
     * Tones: 0x00 → 0x60 → 0xA0 → 0xFF
     */
    CGA_MODE2 = 3,
    
    /**
     * @brief Sepia vintage palette
     * 
     * Warm tones reminiscent of aged photographs.
     * Tones: 0x1E → 0x64 → 0xB4 → 0xF0
     */
    SEPIA = 4,
    
    /**
     * @brief Night vision palette
     * 
     * Enhanced contrast palette optimized for low-light conditions.
     * Boosted shadows and compressed highlights.
     * Tones: 0x00 → 0x3C → 0x8C → 0xDC
     */
    NIGHT = 5,
    
    /**
     * @brief Thermal camera palette
     * 
     * High-contrast palette simulating thermal imaging.
     * Distinct separation between intensity levels.
     * Tones: 0x00 → 0x46 → 0x96 → 0xFF
     */
    THERMAL = 6,
    
    /**
     * @brief High contrast black & white
     * 
     * Maximum contrast palette for stark, graphic results.
     * Tones: 0x00 → 0x00 → 0xFF → 0xFF (effectively 2-tone)
     */
    HI_CONTRAST = 7,
    
    // =========================================================================
    // Custom Palettes (User-Configurable)
    // =========================================================================
    
    /**
     * @brief Custom palette slot 1
     * 
     * User-defined palette, persisted to NVS.
     * Default: Copy of GB_CLASSIC until modified.
     */
    CUSTOM_1 = 8,
    
    /**
     * @brief Custom palette slot 2
     * 
     * User-defined palette, persisted to NVS.
     * Default: Copy of GB_POCKET until modified.
     */
    CUSTOM_2 = 9,
    
    /**
     * @brief Custom palette slot 3
     * 
     * User-defined palette, persisted to NVS.
     * Default: Linear grayscale until modified.
     */
    CUSTOM_3 = 10,
    
    // =========================================================================
    // Sentinel
    // =========================================================================
    
    /**
     * @brief Total count of palette types (do not use as palette index)
     */
    COUNT = 11
};

// =============================================================================
// Palette Structure
// =============================================================================

/**
 * @brief Color palette definition
 * 
 * @details
 * A palette consists of 4 grayscale tones arranged from darkest to lightest.
 * The tones are used by the dithering pipeline to quantize continuous
 * grayscale values into discrete levels.
 * 
 * **Tone Ordering:**
 * - `tones[0]` = Darkest (shadows)
 * - `tones[1]` = Dark midtone
 * - `tones[2]` = Light midtone
 * - `tones[3]` = Lightest (highlights)
 * 
 * **Example:**
 * @code
 * Palette myPalette = {
 *     .tones = { 0x00, 0x55, 0xAA, 0xFF },
 *     .name = "Linear Gray"
 * };
 * @endcode
 */
struct Palette {
    /**
     * @brief Grayscale tone values (0-255)
     * 
     * Array of 4 tones ordered from darkest [0] to lightest [3].
     * Values should be monotonically increasing for proper dithering.
     */
    uint8_t tones[PALETTE_TONE_COUNT];
    
    /**
     * @brief Human-readable palette name
     * 
     * Null-terminated string for display in menus and debugging.
     * Should be kept short (max 15 chars) for OLED display compatibility.
     */
    const char* name;
};

// =============================================================================
// Public API - Initialization
// =============================================================================

/**
 * @brief Initialize the palette system
 * 
 * @details
 * Must be called once during system startup before any other palette
 * functions. This function:
 * 
 * 1. Populates all built-in palettes with default values
 * 2. Initializes custom palette slots with placeholder values
 * 3. Validates internal data structures
 * 
 * Safe to call multiple times (idempotent).
 * 
 * @note This function does NOT load custom palettes from NVS.
 *       Use `palette_load_custom()` after init if persistence is needed.
 * 
 * @code
 * void setup() {
 *     pxlcam::filters::palette_init();  // Initialize palette system
 * }
 * @endcode
 */
void palette_init();

/**
 * @brief Check if palette system is initialized
 * 
 * @return true if palette_init() has been called successfully
 * @return false if palette system is not yet initialized
 */
bool palette_is_initialized();

// =============================================================================
// Public API - Palette Access
// =============================================================================

/**
 * @brief Get palette by type
 * 
 * @details
 * Returns a const reference to the requested palette. This function
 * performs bounds checking and returns the default palette (GB_CLASSIC)
 * if an invalid type is requested.
 * 
 * The returned reference is valid for the lifetime of the program.
 * 
 * @param type Palette type to retrieve
 * @return const Palette& Reference to the requested palette
 * 
 * @warning If called before palette_init(), behavior is undefined.
 *          Always call palette_init() first.
 * 
 * @code
 * const Palette& pal = palette_get(PaletteType::SEPIA);
 * uint8_t darkest = pal.tones[0];
 * @endcode
 */
const Palette& palette_get(PaletteType type);

/**
 * @brief Get palette by numeric index
 * 
 * @details
 * Alternative to palette_get() when working with dynamic indices.
 * Performs the same bounds checking and fallback behavior.
 * 
 * @param index Numeric index (0 to TOTAL_PALETTE_COUNT-1)
 * @return const Palette& Reference to the palette
 */
const Palette& palette_get_by_index(uint8_t index);

/**
 * @brief Get total number of available palettes
 * 
 * @return uint8_t Total palette count (built-in + custom)
 */
uint8_t palette_get_count();

/**
 * @brief Get number of built-in palettes
 * 
 * @return uint8_t Built-in palette count
 */
uint8_t palette_get_builtin_count();

/**
 * @brief Get number of custom palette slots
 * 
 * @return uint8_t Custom palette slot count
 */
uint8_t palette_get_custom_count();

// =============================================================================
// Public API - Validation
// =============================================================================

/**
 * @brief Check if a palette type is a custom (user-editable) palette
 * 
 * @param type Palette type to check
 * @return true if custom palette (CUSTOM_1, CUSTOM_2, CUSTOM_3)
 * @return false if built-in palette
 */
bool palette_is_custom(PaletteType type);

/**
 * @brief Validate that palette type is within valid range
 * 
 * @param type Palette type to validate
 * @return true if valid palette type
 * @return false if invalid (out of range)
 */
bool palette_is_valid_type(PaletteType type);

/**
 * @brief Get the default/fallback palette type
 * 
 * @return PaletteType Default palette (GB_CLASSIC)
 */
PaletteType palette_get_default_type();

// =============================================================================
// Public API - Tone Mapping
// =============================================================================

/**
 * @brief Map a grayscale value to the nearest palette tone
 * 
 * @details
 * Given a grayscale input value (0-255), this function returns the
 * closest matching tone from the specified palette.
 * 
 * @param value Input grayscale value (0-255)
 * @param palette Palette to map against
 * @return uint8_t Nearest palette tone value
 */
uint8_t palette_map_value(uint8_t value, const Palette& palette);

/**
 * @brief Map a grayscale value to palette tone index
 * 
 * @details
 * Returns the index (0-3) of the nearest tone rather than the tone value.
 * Useful for indexed color operations.
 * 
 * @param value Input grayscale value (0-255)
 * @param palette Palette to map against
 * @return uint8_t Tone index (0-3)
 */
uint8_t palette_map_index(uint8_t value, const Palette& palette);

// =============================================================================
// Public API - Custom Palette Management
// =============================================================================

/**
 * @brief Set a custom palette's tones
 * 
 * @details
 * Modifies one of the custom palette slots (CUSTOM_1, CUSTOM_2, CUSTOM_3).
 * This function only updates the in-memory palette; use palette_save_custom()
 * to persist changes to NVS.
 * 
 * @param type Must be CUSTOM_1, CUSTOM_2, or CUSTOM_3
 * @param tones Array of 4 tone values (copied internally)
 * @param name Optional custom name (max 15 chars, copied internally)
 * @return true on success
 * @return false if type is not a custom palette
 * 
 * @code
 * uint8_t myTones[4] = { 0x00, 0x40, 0xB0, 0xFF };
 * palette_set_custom(PaletteType::CUSTOM_1, myTones, "My Palette");
 * @endcode
 */
bool palette_set_custom(PaletteType type, const uint8_t tones[PALETTE_TONE_COUNT], const char* name = nullptr);

/**
 * @brief Reset a custom palette to default values
 * 
 * @param type Must be CUSTOM_1, CUSTOM_2, or CUSTOM_3
 * @return true on success
 * @return false if type is not a custom palette
 */
bool palette_reset_custom(PaletteType type);

// =============================================================================
// Future Expansion (TODO - Not Implemented Yet)
// =============================================================================

// TODO v1.3.1: Implement NVS persistence for custom palettes
// bool palette_save_custom(PaletteType type);
// bool palette_load_custom(PaletteType type);
// bool palette_load_all_custom();

// TODO v1.4.0: Implement JSON import/export for palette sharing
// bool palette_export_json(PaletteType type, char* buffer, size_t bufferSize);
// bool palette_import_json(PaletteType type, const char* json);

// TODO v1.3.1: Implement palette cycling for menu navigation
// PaletteType palette_cycle_next(PaletteType current);
// PaletteType palette_cycle_prev(PaletteType current);

// TODO v1.4.0: RGB palette support for color output
// struct RGBPalette { uint32_t colors[4]; const char* name; };
// const RGBPalette& palette_get_rgb(PaletteType type);

// =============================================================================
// Debug Utilities
// =============================================================================

#ifdef PXLCAM_DEBUG_PALETTE

/**
 * @brief Print palette information to Serial
 * 
 * @param type Palette type to print
 */
void palette_debug_print(PaletteType type);

/**
 * @brief Print all palettes to Serial
 */
void palette_debug_print_all();

#endif // PXLCAM_DEBUG_PALETTE

} // namespace filters
} // namespace pxlcam

#endif // PXLCAM_FILTERS_PALETTE_H
