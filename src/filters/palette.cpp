/**
 * @file palette.cpp
 * @brief Implementation of the PXLcam Color Palette System
 * 
 * @details
 * This file implements the palette management system for PXLcam v1.3.0.
 * It provides:
 * - Storage for 8 built-in palettes
 * - 3 user-configurable custom palette slots
 * - Safe access functions with validation
 * - Tone mapping utilities for the dithering pipeline
 * 
 * @see palette.h for public API documentation
 * 
 * @author PXLcam Team
 * @version 1.3.0
 * @date 2024
 * 
 * @copyright MIT License
 */

#include "filters/palette.h"
#include <string.h>  // For memcpy, strncpy

namespace pxlcam {
namespace filters {

// =============================================================================
// Internal State
// =============================================================================

/**
 * @brief Initialization flag to track module state
 */
static bool s_initialized = false;

/**
 * @brief Internal storage for all palettes
 * 
 * Layout: [0-7] = Built-in, [8-10] = Custom
 */
static Palette s_palettes[TOTAL_PALETTE_COUNT];

/**
 * @brief Internal name buffer for custom palettes
 * 
 * Since custom palettes need mutable names, we store them here.
 */
static char s_custom_names[CUSTOM_PALETTE_COUNT][PALETTE_NAME_MAX_LEN];

// =============================================================================
// Built-in Palette Definitions (ROM Constants)
// =============================================================================

/**
 * @brief Default tone values for built-in palettes
 * 
 * Each palette has 4 tones ordered from darkest [0] to lightest [3].
 * These values are carefully tuned for aesthetic appeal on the
 * PXLcam's target resolution and dithering algorithms.
 */
namespace defaults {

// GameBoy Classic (DMG-01) - Iconic greenish tint
constexpr uint8_t GB_CLASSIC_TONES[PALETTE_TONE_COUNT] = { 0x0F, 0x56, 0x9B, 0xCF };
constexpr const char* GB_CLASSIC_NAME = "GB Classic";

// GameBoy Pocket - Pure grayscale
constexpr uint8_t GB_POCKET_TONES[PALETTE_TONE_COUNT] = { 0x00, 0x55, 0xAA, 0xFF };
constexpr const char* GB_POCKET_NAME = "GB Pocket";

// CGA Mode 1 - Cyan/Magenta inspired
constexpr uint8_t CGA_MODE1_TONES[PALETTE_TONE_COUNT] = { 0x00, 0x40, 0xAA, 0xFF };
constexpr const char* CGA_MODE1_NAME = "CGA Mode 1";

// CGA Mode 2 - Green/Red inspired
constexpr uint8_t CGA_MODE2_TONES[PALETTE_TONE_COUNT] = { 0x00, 0x60, 0xA0, 0xFF };
constexpr const char* CGA_MODE2_NAME = "CGA Mode 2";

// Sepia - Warm vintage tones
constexpr uint8_t SEPIA_TONES[PALETTE_TONE_COUNT] = { 0x1E, 0x64, 0xB4, 0xF0 };
constexpr const char* SEPIA_NAME = "Sepia";

// Night Vision - Enhanced low-light contrast
constexpr uint8_t NIGHT_TONES[PALETTE_TONE_COUNT] = { 0x00, 0x3C, 0x8C, 0xDC };
constexpr const char* NIGHT_NAME = "Night";

// Thermal - High-contrast thermal imaging simulation
constexpr uint8_t THERMAL_TONES[PALETTE_TONE_COUNT] = { 0x00, 0x46, 0x96, 0xFF };
constexpr const char* THERMAL_NAME = "Thermal";

// High Contrast - Maximum black/white separation (effectively 2-tone)
constexpr uint8_t HI_CONTRAST_TONES[PALETTE_TONE_COUNT] = { 0x00, 0x00, 0xFF, 0xFF };
constexpr const char* HI_CONTRAST_NAME = "Hi-Contrast";

// Custom palette defaults
constexpr uint8_t CUSTOM_1_TONES[PALETTE_TONE_COUNT] = { 0x0F, 0x56, 0x9B, 0xCF };  // Copy of GB Classic
constexpr uint8_t CUSTOM_2_TONES[PALETTE_TONE_COUNT] = { 0x00, 0x55, 0xAA, 0xFF };  // Copy of GB Pocket
constexpr uint8_t CUSTOM_3_TONES[PALETTE_TONE_COUNT] = { 0x00, 0x55, 0xAA, 0xFF };  // Linear grayscale

} // namespace defaults

// =============================================================================
// Internal Helper Functions
// =============================================================================

/**
 * @brief Convert PaletteType to array index
 * 
 * @param type Palette type enumeration
 * @return uint8_t Array index (0-10), or 0 if invalid
 */
static inline uint8_t type_to_index(PaletteType type) {
    const uint8_t idx = static_cast<uint8_t>(type);
    if (idx >= TOTAL_PALETTE_COUNT) {
        return 0;  // Fallback to GB_CLASSIC index
    }
    return idx;
}

/**
 * @brief Initialize a single palette entry
 * 
 * @param index Array index
 * @param tones Source tone array
 * @param name Palette name (pointer will be stored directly for built-in palettes)
 */
static void init_palette_entry(uint8_t index, const uint8_t tones[PALETTE_TONE_COUNT], const char* name) {
    if (index >= TOTAL_PALETTE_COUNT) return;
    
    memcpy(s_palettes[index].tones, tones, PALETTE_TONE_COUNT);
    s_palettes[index].name = name;
}

/**
 * @brief Initialize a custom palette entry
 * 
 * @param customIndex Custom palette index (0-2, maps to CUSTOM_1-3)
 * @param tones Source tone array
 * @param name Default name
 */
static void init_custom_palette_entry(uint8_t customIndex, const uint8_t tones[PALETTE_TONE_COUNT], const char* name) {
    if (customIndex >= CUSTOM_PALETTE_COUNT) return;
    
    const uint8_t arrayIndex = BUILTIN_PALETTE_COUNT + customIndex;
    
    memcpy(s_palettes[arrayIndex].tones, tones, PALETTE_TONE_COUNT);
    
    // Copy name to mutable buffer
    strncpy(s_custom_names[customIndex], name, PALETTE_NAME_MAX_LEN - 1);
    s_custom_names[customIndex][PALETTE_NAME_MAX_LEN - 1] = '\0';
    s_palettes[arrayIndex].name = s_custom_names[customIndex];
}

// =============================================================================
// Public API - Initialization
// =============================================================================

void palette_init() {
    // Guard against double initialization (idempotent)
    if (s_initialized) {
        return;
    }
    
    // -------------------------------------------------------------------------
    // Initialize Built-in Palettes
    // -------------------------------------------------------------------------
    
    init_palette_entry(
        static_cast<uint8_t>(PaletteType::GB_CLASSIC),
        defaults::GB_CLASSIC_TONES,
        defaults::GB_CLASSIC_NAME
    );
    
    init_palette_entry(
        static_cast<uint8_t>(PaletteType::GB_POCKET),
        defaults::GB_POCKET_TONES,
        defaults::GB_POCKET_NAME
    );
    
    init_palette_entry(
        static_cast<uint8_t>(PaletteType::CGA_MODE1),
        defaults::CGA_MODE1_TONES,
        defaults::CGA_MODE1_NAME
    );
    
    init_palette_entry(
        static_cast<uint8_t>(PaletteType::CGA_MODE2),
        defaults::CGA_MODE2_TONES,
        defaults::CGA_MODE2_NAME
    );
    
    init_palette_entry(
        static_cast<uint8_t>(PaletteType::SEPIA),
        defaults::SEPIA_TONES,
        defaults::SEPIA_NAME
    );
    
    init_palette_entry(
        static_cast<uint8_t>(PaletteType::NIGHT),
        defaults::NIGHT_TONES,
        defaults::NIGHT_NAME
    );
    
    init_palette_entry(
        static_cast<uint8_t>(PaletteType::THERMAL),
        defaults::THERMAL_TONES,
        defaults::THERMAL_NAME
    );
    
    init_palette_entry(
        static_cast<uint8_t>(PaletteType::HI_CONTRAST),
        defaults::HI_CONTRAST_TONES,
        defaults::HI_CONTRAST_NAME
    );
    
    // -------------------------------------------------------------------------
    // Initialize Custom Palettes
    // -------------------------------------------------------------------------
    
    init_custom_palette_entry(0, defaults::CUSTOM_1_TONES, "Custom 1");
    init_custom_palette_entry(1, defaults::CUSTOM_2_TONES, "Custom 2");
    init_custom_palette_entry(2, defaults::CUSTOM_3_TONES, "Custom 3");
    
    // -------------------------------------------------------------------------
    // Mark as Initialized
    // -------------------------------------------------------------------------
    
    s_initialized = true;
}

bool palette_is_initialized() {
    return s_initialized;
}

// =============================================================================
// Public API - Palette Access
// =============================================================================

const Palette& palette_get(PaletteType type) {
    const uint8_t idx = type_to_index(type);
    return s_palettes[idx];
}

const Palette& palette_get_by_index(uint8_t index) {
    if (index >= TOTAL_PALETTE_COUNT) {
        index = 0;  // Fallback to GB_CLASSIC
    }
    return s_palettes[index];
}

uint8_t palette_get_count() {
    return TOTAL_PALETTE_COUNT;
}

uint8_t palette_get_builtin_count() {
    return BUILTIN_PALETTE_COUNT;
}

uint8_t palette_get_custom_count() {
    return CUSTOM_PALETTE_COUNT;
}

// =============================================================================
// Public API - Validation
// =============================================================================

bool palette_is_custom(PaletteType type) {
    const uint8_t idx = static_cast<uint8_t>(type);
    return idx >= BUILTIN_PALETTE_COUNT && idx < TOTAL_PALETTE_COUNT;
}

bool palette_is_valid_type(PaletteType type) {
    return static_cast<uint8_t>(type) < TOTAL_PALETTE_COUNT;
}

PaletteType palette_get_default_type() {
    return PaletteType::GB_CLASSIC;
}

// =============================================================================
// Public API - Tone Mapping
// =============================================================================

uint8_t palette_map_value(uint8_t value, const Palette& palette) {
    // Compute distance to each tone and find the closest
    uint8_t bestIndex = 0;
    int16_t bestDist = 256;  // Maximum possible distance + 1
    
    for (uint8_t i = 0; i < PALETTE_TONE_COUNT; ++i) {
        int16_t dist = static_cast<int16_t>(value) - static_cast<int16_t>(palette.tones[i]);
        if (dist < 0) dist = -dist;  // abs()
        
        if (dist < bestDist) {
            bestDist = dist;
            bestIndex = i;
        }
    }
    
    return palette.tones[bestIndex];
}

uint8_t palette_map_index(uint8_t value, const Palette& palette) {
    // Same algorithm as palette_map_value, but returns index
    uint8_t bestIndex = 0;
    int16_t bestDist = 256;
    
    for (uint8_t i = 0; i < PALETTE_TONE_COUNT; ++i) {
        int16_t dist = static_cast<int16_t>(value) - static_cast<int16_t>(palette.tones[i]);
        if (dist < 0) dist = -dist;
        
        if (dist < bestDist) {
            bestDist = dist;
            bestIndex = i;
        }
    }
    
    return bestIndex;
}

// =============================================================================
// Public API - Custom Palette Management
// =============================================================================

bool palette_set_custom(PaletteType type, const uint8_t tones[PALETTE_TONE_COUNT], const char* name) {
    // Validate that this is a custom palette type
    if (!palette_is_custom(type)) {
        return false;
    }
    
    const uint8_t arrayIndex = static_cast<uint8_t>(type);
    const uint8_t customIndex = arrayIndex - BUILTIN_PALETTE_COUNT;
    
    // Copy tones
    memcpy(s_palettes[arrayIndex].tones, tones, PALETTE_TONE_COUNT);
    
    // Copy name if provided
    if (name != nullptr) {
        strncpy(s_custom_names[customIndex], name, PALETTE_NAME_MAX_LEN - 1);
        s_custom_names[customIndex][PALETTE_NAME_MAX_LEN - 1] = '\0';
        s_palettes[arrayIndex].name = s_custom_names[customIndex];
    }
    
    return true;
}

bool palette_reset_custom(PaletteType type) {
    // Validate custom palette type
    if (!palette_is_custom(type)) {
        return false;
    }
    
    const uint8_t customIndex = static_cast<uint8_t>(type) - BUILTIN_PALETTE_COUNT;
    
    // Reset to default values based on slot
    switch (customIndex) {
        case 0:  // CUSTOM_1
            init_custom_palette_entry(0, defaults::CUSTOM_1_TONES, "Custom 1");
            break;
        case 1:  // CUSTOM_2
            init_custom_palette_entry(1, defaults::CUSTOM_2_TONES, "Custom 2");
            break;
        case 2:  // CUSTOM_3
            init_custom_palette_entry(2, defaults::CUSTOM_3_TONES, "Custom 3");
            break;
        default:
            return false;
    }
    
    return true;
}

// =============================================================================
// Debug Utilities
// =============================================================================

#ifdef PXLCAM_DEBUG_PALETTE

#include <Arduino.h>  // For Serial

void palette_debug_print(PaletteType type) {
    if (!s_initialized) {
        Serial.println(F("[PALETTE] ERROR: Not initialized"));
        return;
    }
    
    const Palette& pal = palette_get(type);
    const uint8_t idx = static_cast<uint8_t>(type);
    
    Serial.printf("[PALETTE] [%u] %s\n", idx, pal.name);
    Serial.printf("  Tones: 0x%02X, 0x%02X, 0x%02X, 0x%02X\n",
        pal.tones[0], pal.tones[1], pal.tones[2], pal.tones[3]);
    Serial.printf("  Custom: %s\n", palette_is_custom(type) ? "Yes" : "No");
}

void palette_debug_print_all() {
    if (!s_initialized) {
        Serial.println(F("[PALETTE] ERROR: Not initialized"));
        return;
    }
    
    Serial.println(F("=== PALETTE REGISTRY ==="));
    Serial.printf("  Total: %u (Built-in: %u, Custom: %u)\n",
        TOTAL_PALETTE_COUNT, BUILTIN_PALETTE_COUNT, CUSTOM_PALETTE_COUNT);
    Serial.println();
    
    for (uint8_t i = 0; i < TOTAL_PALETTE_COUNT; ++i) {
        palette_debug_print(static_cast<PaletteType>(i));
    }
    
    Serial.println(F("========================"));
}

#endif // PXLCAM_DEBUG_PALETTE

} // namespace filters
} // namespace pxlcam
