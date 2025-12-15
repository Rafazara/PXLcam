/**
 * @file palette_manager.cpp
 * @brief Color Palette Management implementation for PXLcam v1.3.0
 * 
 * @version 1.3.0
 * @date 2024
 */

#include "palettes/palette_manager.h"

#ifdef PXL_V13_EXPERIMENTAL

namespace pxlcam {
namespace palettes {

// ============================================================================
// Built-in Palette Definitions
// ============================================================================

static const Palette s_builtinPalettes[] = {
    // GAMEBOY_CLASSIC - Original DMG green
    {
        "GB Classic",
        {
            Color(15, 56, 15),      // Darkest
            Color(48, 98, 48),      // Dark
            Color(139, 172, 15),    // Light
            Color(155, 188, 15)     // Lightest
        },
        4, false
    },
    // GAMEBOY_POCKET - Grayscale
    {
        "GB Pocket",
        {
            Color(0, 0, 0),         // Black
            Color(85, 85, 85),      // Dark gray
            Color(170, 170, 170),   // Light gray
            Color(255, 255, 255)    // White
        },
        4, false
    },
    // GAMEBOY_LIGHT - Yellowish
    {
        "GB Light",
        {
            Color(0, 60, 0),
            Color(51, 102, 51),
            Color(136, 192, 112),
            Color(224, 248, 208)
        },
        4, false
    },
    // CGA_MODE4_CYAN
    {
        "CGA Cyan",
        {
            Color(0, 0, 0),         // Black
            Color(0, 170, 170),     // Cyan
            Color(170, 0, 170),     // Magenta
            Color(170, 170, 170)    // White
        },
        4, false
    },
    // CGA_MODE4_GREEN
    {
        "CGA Green",
        {
            Color(0, 0, 0),         // Black
            Color(0, 170, 0),       // Green
            Color(170, 0, 0),       // Red
            Color(170, 85, 0)       // Brown/Yellow
        },
        4, false
    },
    // SEPIA_VINTAGE
    {
        "Sepia",
        {
            Color(30, 20, 10),
            Color(100, 70, 40),
            Color(180, 140, 90),
            Color(240, 220, 180)
        },
        4, false
    },
    // THERMAL_VISION
    {
        "Thermal",
        {
            Color(0, 0, 64),        // Cold (dark blue)
            Color(0, 128, 0),       // Cool (green)
            Color(255, 255, 0),     // Warm (yellow)
            Color(255, 0, 0)        // Hot (red)
        },
        4, false
    },
    // NEON_SYNTHWAVE
    {
        "Synthwave",
        {
            Color(10, 0, 20),       // Deep purple
            Color(255, 0, 128),     // Hot pink
            Color(0, 255, 255),     // Cyan
            Color(255, 255, 0)      // Yellow
        },
        4, false
    },
    // MONOCHROME_BW
    {
        "Mono B&W",
        {
            Color(0, 0, 0),
            Color(85, 85, 85),
            Color(170, 170, 170),
            Color(255, 255, 255)
        },
        4, false
    }
};

// Custom palette storage
static Palette s_customPalettes[3];
static PaletteId s_activePaletteId = PaletteId::GAMEBOY_CLASSIC;
static bool s_initialized = false;

// ============================================================================
// Color Methods
// ============================================================================

uint16_t Color::toRGB565() const {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

uint32_t Color::toRGB888() const {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

// ============================================================================
// Palette Manager Implementation
// ============================================================================

bool init() {
    if (s_initialized) return true;
    
    // Initialize custom palette slots with defaults
    for (int i = 0; i < 3; i++) {
        snprintf(s_customPalettes[i].name, sizeof(s_customPalettes[i].name), 
                 "Custom %d", i + 1);
        s_customPalettes[i].colorCount = 4;
        s_customPalettes[i].isCustom = true;
        // Default to grayscale
        s_customPalettes[i].colors[0] = Color(0, 0, 0);
        s_customPalettes[i].colors[1] = Color(85, 85, 85);
        s_customPalettes[i].colors[2] = Color(170, 170, 170);
        s_customPalettes[i].colors[3] = Color(255, 255, 255);
    }
    
    // TODO: Load custom palettes from NVS
    
    s_initialized = true;
    return true;
}

const Palette* getPalette(PaletteId id) {
    uint8_t idx = static_cast<uint8_t>(id);
    
    if (idx < static_cast<uint8_t>(PaletteId::CUSTOM_SLOT_1)) {
        return &s_builtinPalettes[idx];
    }
    
    if (idx >= static_cast<uint8_t>(PaletteId::CUSTOM_SLOT_1) &&
        idx <= static_cast<uint8_t>(PaletteId::CUSTOM_SLOT_3)) {
        return &s_customPalettes[idx - static_cast<uint8_t>(PaletteId::CUSTOM_SLOT_1)];
    }
    
    return nullptr;
}

const Palette* getActivePalette() {
    return getPalette(s_activePaletteId);
}

bool setActivePalette(PaletteId id) {
    if (static_cast<uint8_t>(id) >= static_cast<uint8_t>(PaletteId::PALETTE_COUNT)) {
        return false;
    }
    s_activePaletteId = id;
    return true;
}

PaletteId cycleNextPalette() {
    uint8_t next = (static_cast<uint8_t>(s_activePaletteId) + 1) % 
                   static_cast<uint8_t>(PaletteId::PALETTE_COUNT);
    s_activePaletteId = static_cast<PaletteId>(next);
    return s_activePaletteId;
}

PaletteId cyclePrevPalette() {
    uint8_t prev = static_cast<uint8_t>(s_activePaletteId);
    if (prev == 0) {
        prev = static_cast<uint8_t>(PaletteId::PALETTE_COUNT) - 1;
    } else {
        prev--;
    }
    s_activePaletteId = static_cast<PaletteId>(prev);
    return s_activePaletteId;
}

bool saveCustomPalette(uint8_t slot, const Palette& palette) {
    if (slot >= 3) return false;
    s_customPalettes[slot] = palette;
    s_customPalettes[slot].isCustom = true;
    // TODO: Persist to NVS
    return true;
}

bool loadCustomPalette(uint8_t slot, Palette& palette) {
    if (slot >= 3) return false;
    palette = s_customPalettes[slot];
    return true;
}

uint16_t mapToPalette(uint8_t gray) {
    const Palette* pal = getActivePalette();
    if (!pal || pal->colorCount == 0) {
        return 0;
    }
    
    // Map grayscale to palette index
    uint8_t idx = (gray * (pal->colorCount - 1)) / 255;
    if (idx >= pal->colorCount) idx = pal->colorCount - 1;
    
    return pal->colors[idx].toRGB565();
}

uint8_t getPaletteCount() {
    return static_cast<uint8_t>(PaletteId::PALETTE_COUNT);
}

} // namespace palettes
} // namespace pxlcam

#endif // PXL_V13_EXPERIMENTAL
