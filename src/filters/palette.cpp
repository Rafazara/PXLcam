/**
 * @file palette.cpp
 * @brief Color Palette System implementation for PXLcam v1.3.0
 * 
 * @version 1.3.0
 * @date 2024
 */

#include "filters/palette.h"

#if PXLCAM_FEATURE_CUSTOM_PALETTES || PXLCAM_FEATURE_STYLIZED_CAPTURE

namespace pxlcam {
namespace filters {

// =============================================================================
// Built-in Palette Definitions
// =============================================================================

static const Palette s_builtinPalettes[] = {
    // GAMEBOY_CLASSIC - Original DMG greenish tones
    Palette("GB Classic", 15, 86, 168, 255),
    
    // GAMEBOY_POCKET - Pure grayscale
    Palette("GB Pocket", 0, 85, 170, 255),
    
    // CGA_MODE1 - Cyan/Magenta palette (mapped to grayscale)
    Palette("CGA Mode 1", 0, 64, 170, 255),
    
    // CGA_MODE2 - Green/Red palette (mapped to grayscale)
    Palette("CGA Mode 2", 0, 96, 160, 255),
    
    // SEPIA_VINTAGE - Warm sepia-like tones
    Palette("Sepia", 30, 100, 180, 240),
    
    // NIGHT_VISION - Green night vision
    Palette("Night", 0, 60, 140, 220),
    
    // THERMAL - Thermal camera style
    Palette("Thermal", 0, 70, 150, 255),
    
    // HIGH_CONTRAST - Pure black and white
    Palette("Hi-Contrast", 0, 0, 255, 255)
};

// Custom palette storage
static Palette s_customPalettes[CUSTOM_PALETTE_SLOTS];

// Active palette tracking
static PaletteType s_activePalette = PaletteType::GAMEBOY_CLASSIC;
static bool s_initialized = false;

// =============================================================================
// Implementation
// =============================================================================

bool palette_init() {
    if (s_initialized) return true;
    
    // Initialize custom slots with default grayscale
    for (uint8_t i = 0; i < CUSTOM_PALETTE_SLOTS; i++) {
        s_customPalettes[i] = Palette();
        snprintf(s_customPalettes[i].name, PALETTE_NAME_LEN, "Custom %d", i + 1);
        s_customPalettes[i].isCustom = true;
    }
    
    // TODO: Load custom palettes from NVS
    // TODO: Load last active palette from NVS
    
    s_initialized = true;
    return true;
}

const Palette* palette_get(PaletteType type) {
    uint8_t idx = static_cast<uint8_t>(type);
    
    // Built-in palette
    if (idx < BUILTIN_PALETTE_COUNT) {
        return &s_builtinPalettes[idx];
    }
    
    // Custom palette
    if (type >= PaletteType::CUSTOM_1 && type <= PaletteType::CUSTOM_3) {
        uint8_t customIdx = static_cast<uint8_t>(type) - static_cast<uint8_t>(PaletteType::CUSTOM_1);
        if (customIdx < CUSTOM_PALETTE_SLOTS) {
            return &s_customPalettes[customIdx];
        }
    }
    
    return nullptr;
}

const Palette* palette_get_active() {
    return palette_get(s_activePalette);
}

bool palette_set_active(PaletteType type) {
    if (palette_get(type) != nullptr) {
        s_activePalette = type;
        // TODO: Save to NVS
        return true;
    }
    return false;
}

PaletteType palette_cycle_next() {
    uint8_t current = static_cast<uint8_t>(s_activePalette);
    uint8_t next = (current + 1) % BUILTIN_PALETTE_COUNT;
    s_activePalette = static_cast<PaletteType>(next);
    return s_activePalette;
}

PaletteType palette_cycle_prev() {
    uint8_t current = static_cast<uint8_t>(s_activePalette);
    uint8_t prev = (current == 0) ? (BUILTIN_PALETTE_COUNT - 1) : (current - 1);
    s_activePalette = static_cast<PaletteType>(prev);
    return s_activePalette;
}

uint8_t palette_get_count() {
    return BUILTIN_PALETTE_COUNT + CUSTOM_PALETTE_SLOTS;
}

uint8_t palette_map_tone(uint8_t gray) {
    const Palette* pal = palette_get_active();
    if (!pal || pal->toneCount == 0) return gray;
    
    // Find nearest tone
    uint8_t bestIdx = 0;
    int bestDist = 256;
    
    for (uint8_t i = 0; i < pal->toneCount; i++) {
        int dist = abs((int)gray - (int)pal->tones[i]);
        if (dist < bestDist) {
            bestDist = dist;
            bestIdx = i;
        }
    }
    
    return pal->tones[bestIdx];
}

uint8_t palette_map_index(uint8_t gray) {
    const Palette* pal = palette_get_active();
    if (!pal || pal->toneCount == 0) return 0;
    
    // Map to index based on thresholds
    // For 4-tone: 0-63 -> 0, 64-127 -> 1, 128-191 -> 2, 192-255 -> 3
    uint8_t step = 256 / pal->toneCount;
    uint8_t idx = gray / step;
    if (idx >= pal->toneCount) idx = pal->toneCount - 1;
    
    return idx;
}

#if PXLCAM_FEATURE_CUSTOM_PALETTES

bool palette_save_custom(uint8_t slot, const Palette& palette) {
    if (slot >= CUSTOM_PALETTE_SLOTS) return false;
    
    s_customPalettes[slot] = palette;
    s_customPalettes[slot].isCustom = true;
    
    // TODO: Persist to NVS
    // nvs_store_palette(slot, &palette);
    
    return true;
}

bool palette_load_custom(uint8_t slot, Palette& palette) {
    if (slot >= CUSTOM_PALETTE_SLOTS) return false;
    
    palette = s_customPalettes[slot];
    return true;
}

bool palette_delete_custom(uint8_t slot) {
    if (slot >= CUSTOM_PALETTE_SLOTS) return false;
    
    // Reset to default
    s_customPalettes[slot] = Palette();
    snprintf(s_customPalettes[slot].name, PALETTE_NAME_LEN, "Custom %d", slot + 1);
    s_customPalettes[slot].isCustom = true;
    
    // TODO: Remove from NVS
    
    return true;
}

#endif // PXLCAM_FEATURE_CUSTOM_PALETTES

} // namespace filters
} // namespace pxlcam

#endif // PXLCAM_FEATURE_CUSTOM_PALETTES || PXLCAM_FEATURE_STYLIZED_CAPTURE
