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
 * - NVS persistence for selected palette (v1.3.0)
 * - SD card JSON import/export for custom palettes (v1.3.0)
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
#include "pxlcam_config.h"
#include <string.h>  // For memcpy, strncpy

#if PXLCAM_FEATURE_CUSTOM_PALETTES
#include "nvs_store.h"
#include "logging.h"
#include <FS.h>
#include <SD_MMC.h>
#endif

namespace pxlcam {
namespace filters {

// =============================================================================
// Internal State
// =============================================================================

#if PXLCAM_FEATURE_CUSTOM_PALETTES
namespace {
constexpr const char* kLogTag = "palette";
constexpr const char* kNvsKeyPaletteId = "palette_id";
}
#endif

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

#if PXLCAM_FEATURE_CUSTOM_PALETTES
/**
 * @brief Custom palette slot metadata
 */
static CustomPaletteSlot s_custom_slots[CUSTOM_PALETTE_COUNT];

/**
 * @brief Currently selected palette type
 */
static PaletteType s_current_palette = PaletteType::GB_CLASSIC;
#endif

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
    
#if PXLCAM_FEATURE_CUSTOM_PALETTES
    // Initialize custom slot metadata
    for (uint8_t i = 0; i < CUSTOM_PALETTE_COUNT; ++i) {
        s_custom_slots[i].loaded = false;
        s_custom_slots[i].data = s_palettes[BUILTIN_PALETTE_COUNT + i];
    }
    
    // Load selected palette from NVS
    palette_load_selection_from_nvs();
#endif
    
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
    
#if PXLCAM_FEATURE_CUSTOM_PALETTES
    // Mark slot as not loaded (reset to default)
    s_custom_slots[customIndex].loaded = false;
#endif
    
    return true;
}

// =============================================================================
// Public API - Palette Selection & Persistence (v1.3.0)
// =============================================================================

#if PXLCAM_FEATURE_CUSTOM_PALETTES

bool palette_select(PaletteType type) {
    if (!palette_is_valid_type(type)) {
        PXLCAM_LOGW_TAG(kLogTag, "Invalid palette type: %d", static_cast<int>(type));
        return false;
    }
    
    s_current_palette = type;
    
    // Persist to NVS
    bool nvs_ok = pxlcam::nvs::writeU8(kNvsKeyPaletteId, static_cast<uint8_t>(type));
    
    if (nvs_ok) {
        PXLCAM_LOGI_TAG(kLogTag, "Palette selected: %s (id=%d)", 
                        palette_get(type).name, static_cast<int>(type));
    } else {
        PXLCAM_LOGW_TAG(kLogTag, "Palette selected but NVS write failed: %d", 
                        static_cast<int>(type));
    }
    
    return nvs_ok;
}

const Palette& palette_current() {
    return palette_get(s_current_palette);
}

PaletteType palette_current_type() {
    return s_current_palette;
}

bool palette_load_selection_from_nvs() {
    if (!pxlcam::nvs::isInitialized()) {
        pxlcam::nvs::nvsStoreInit();
    }
    
    uint8_t stored = pxlcam::nvs::readU8(kNvsKeyPaletteId, 
                                          static_cast<uint8_t>(PaletteType::GB_CLASSIC));
    
    if (stored >= TOTAL_PALETTE_COUNT) {
        PXLCAM_LOGW_TAG(kLogTag, "Invalid palette ID in NVS: %d, using default", stored);
        s_current_palette = PaletteType::GB_CLASSIC;
        return false;
    }
    
    s_current_palette = static_cast<PaletteType>(stored);
    PXLCAM_LOGI_TAG(kLogTag, "Palette loaded from NVS: %s (id=%d)", 
                    palette_get(s_current_palette).name, stored);
    return true;
}

bool palette_set_custom_slot(PaletteType type, const Palette& palette) {
    if (!palette_is_custom(type)) {
        return false;
    }
    
    const uint8_t arrayIndex = static_cast<uint8_t>(type);
    const uint8_t customIndex = arrayIndex - BUILTIN_PALETTE_COUNT;
    
    // Copy tones
    memcpy(s_palettes[arrayIndex].tones, palette.tones, PALETTE_TONE_COUNT);
    
    // Copy name
    if (palette.name != nullptr) {
        strncpy(s_custom_names[customIndex], palette.name, PALETTE_NAME_MAX_LEN - 1);
        s_custom_names[customIndex][PALETTE_NAME_MAX_LEN - 1] = '\0';
        s_palettes[arrayIndex].name = s_custom_names[customIndex];
    }
    
    // Update slot metadata
    s_custom_slots[customIndex].loaded = true;
    s_custom_slots[customIndex].data = s_palettes[arrayIndex];
    
    PXLCAM_LOGI_TAG(kLogTag, "Custom slot %d set: %s [%d,%d,%d,%d]",
                    customIndex, s_palettes[arrayIndex].name,
                    palette.tones[0], palette.tones[1], 
                    palette.tones[2], palette.tones[3]);
    
    return true;
}

const CustomPaletteSlot* palette_custom_slots() {
    return s_custom_slots;
}

bool palette_custom_is_loaded(PaletteType type) {
    if (!palette_is_custom(type)) {
        return false;
    }
    const uint8_t customIndex = static_cast<uint8_t>(type) - BUILTIN_PALETTE_COUNT;
    return s_custom_slots[customIndex].loaded;
}

uint8_t palette_list_all(PaletteInfo* outList) {
    if (outList == nullptr) {
        return 0;
    }
    
    for (uint8_t i = 0; i < TOTAL_PALETTE_COUNT; ++i) {
        outList[i].type = static_cast<PaletteType>(i);
        outList[i].palette = &s_palettes[i];
        
        if (i < BUILTIN_PALETTE_COUNT) {
            outList[i].source = PaletteSource::BUILTIN;
            outList[i].loaded = true;  // Built-ins are always loaded
        } else {
            outList[i].source = PaletteSource::CUSTOM;
            outList[i].loaded = s_custom_slots[i - BUILTIN_PALETTE_COUNT].loaded;
        }
    }
    
    return TOTAL_PALETTE_COUNT;
}

// =============================================================================
// SD Card JSON Parsing Helpers
// =============================================================================

namespace {

/**
 * @brief Skip whitespace in JSON string
 */
const char* skip_whitespace(const char* p) {
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
        ++p;
    }
    return p;
}

/**
 * @brief Parse a JSON string value (simple: no escapes)
 * @return Pointer to character after closing quote, or nullptr on error
 */
const char* parse_json_string(const char* p, char* out, size_t maxLen) {
    p = skip_whitespace(p);
    if (*p != '"') return nullptr;
    ++p;  // Skip opening quote
    
    size_t i = 0;
    while (*p && *p != '"' && i < maxLen - 1) {
        out[i++] = *p++;
    }
    out[i] = '\0';
    
    if (*p != '"') return nullptr;
    return p + 1;  // Skip closing quote
}

/**
 * @brief Parse a JSON integer value
 */
const char* parse_json_int(const char* p, int* out) {
    p = skip_whitespace(p);
    
    int val = 0;
    bool negative = false;
    
    if (*p == '-') {
        negative = true;
        ++p;
    }
    
    if (*p < '0' || *p > '9') return nullptr;
    
    while (*p >= '0' && *p <= '9') {
        val = val * 10 + (*p - '0');
        ++p;
    }
    
    *out = negative ? -val : val;
    return p;
}

/**
 * @brief Find key in JSON object (simple: assumes well-formed JSON)
 * @return Pointer to value after colon, or nullptr if not found
 */
const char* find_json_key(const char* json, const char* key) {
    const char* p = json;
    size_t keyLen = strlen(key);
    
    while ((p = strstr(p, key)) != nullptr) {
        // Check if this is actually the key (preceded by quote)
        if (p > json && *(p - 1) == '"') {
            const char* afterKey = p + keyLen;
            if (*afterKey == '"') {
                // Found key, skip to colon and value
                p = afterKey + 1;
                p = skip_whitespace(p);
                if (*p == ':') {
                    return skip_whitespace(p + 1);
                }
            }
        }
        ++p;
    }
    return nullptr;
}

/**
 * @brief Parse a single palette from JSON object
 * @param json Pointer to start of palette object (after '{')
 * @param outPalette Output palette structure
 * @return true if parsed successfully
 */
bool parse_palette_object(const char* json, Palette& outPalette, char* nameBuffer) {
    // Find "name" key
    const char* nameVal = find_json_key(json, "name");
    if (nameVal) {
        parse_json_string(nameVal, nameBuffer, PALETTE_NAME_MAX_LEN);
        outPalette.name = nameBuffer;
    } else {
        strncpy(nameBuffer, "Unnamed", PALETTE_NAME_MAX_LEN);
        outPalette.name = nameBuffer;
    }
    
    // Find "tones" array
    const char* tonesVal = find_json_key(json, "tones");
    if (!tonesVal || *tonesVal != '[') {
        return false;
    }
    
    const char* p = tonesVal + 1;  // Skip '['
    
    for (int i = 0; i < PALETTE_TONE_COUNT; ++i) {
        p = skip_whitespace(p);
        
        int val = 0;
        p = parse_json_int(p, &val);
        if (!p) return false;
        
        // Clamp to valid range
        if (val < 0) val = 0;
        if (val > 255) val = 255;
        outPalette.tones[i] = static_cast<uint8_t>(val);
        
        p = skip_whitespace(p);
        if (*p == ',') ++p;
    }
    
    return true;
}

} // anonymous namespace

// =============================================================================
// SD Card Load/Save Implementation
// =============================================================================

bool palette_load_from_sd() {
    // Check if SD is mounted
    if (SD_MMC.cardType() == CARD_NONE) {
        PXLCAM_LOGW_TAG(kLogTag, "SD not mounted, cannot load palettes");
        return false;
    }
    
    // Check if file exists
    if (!SD_MMC.exists(PALETTE_SD_PATH)) {
        PXLCAM_LOGI_TAG(kLogTag, "No palettes.json found at %s (using defaults)", PALETTE_SD_PATH);
        return true;  // Not an error - custom palettes are optional
    }
    
    // Open file
    File file = SD_MMC.open(PALETTE_SD_PATH, FILE_READ);
    if (!file) {
        PXLCAM_LOGE_TAG(kLogTag, "Failed to open %s", PALETTE_SD_PATH);
        return false;
    }
    
    // Read entire file (limit: 2KB should be plenty for 3 palettes)
    constexpr size_t kMaxJsonSize = 2048;
    char* jsonBuffer = static_cast<char*>(malloc(kMaxJsonSize));
    if (!jsonBuffer) {
        file.close();
        PXLCAM_LOGE_TAG(kLogTag, "Failed to allocate JSON buffer");
        return false;
    }
    
    size_t bytesRead = file.readBytes(jsonBuffer, kMaxJsonSize - 1);
    jsonBuffer[bytesRead] = '\0';
    file.close();
    
    PXLCAM_LOGI_TAG(kLogTag, "Read %u bytes from %s", bytesRead, PALETTE_SD_PATH);
    
    // Find "custom_palettes" array
    const char* arrayStart = find_json_key(jsonBuffer, "custom_palettes");
    if (!arrayStart || *arrayStart != '[') {
        PXLCAM_LOGW_TAG(kLogTag, "No 'custom_palettes' array found in JSON");
        free(jsonBuffer);
        return true;  // Empty/invalid JSON is not fatal
    }
    
    // Parse palette objects
    const char* p = arrayStart + 1;  // Skip '['
    uint8_t slotIndex = 0;
    
    while (slotIndex < CUSTOM_PALETTE_COUNT) {
        // Find next object
        p = skip_whitespace(p);
        if (*p == ']') break;  // End of array
        if (*p != '{') {
            if (*p == ',') { ++p; continue; }
            break;
        }
        
        // Find end of object (simple: count braces)
        const char* objStart = p + 1;
        int braceCount = 1;
        const char* objEnd = objStart;
        while (*objEnd && braceCount > 0) {
            if (*objEnd == '{') ++braceCount;
            else if (*objEnd == '}') --braceCount;
            ++objEnd;
        }
        
        // Parse this palette
        Palette tempPalette;
        if (parse_palette_object(objStart, tempPalette, s_custom_names[slotIndex])) {
            // Copy to appropriate slot
            const uint8_t arrayIndex = BUILTIN_PALETTE_COUNT + slotIndex;
            memcpy(s_palettes[arrayIndex].tones, tempPalette.tones, PALETTE_TONE_COUNT);
            s_palettes[arrayIndex].name = s_custom_names[slotIndex];
            
            // Mark as loaded
            s_custom_slots[slotIndex].loaded = true;
            s_custom_slots[slotIndex].data = s_palettes[arrayIndex];
            
            PXLCAM_LOGI_TAG(kLogTag, "Loaded CUSTOM_%d: %s [%d,%d,%d,%d]",
                            slotIndex + 1, s_palettes[arrayIndex].name,
                            s_palettes[arrayIndex].tones[0],
                            s_palettes[arrayIndex].tones[1],
                            s_palettes[arrayIndex].tones[2],
                            s_palettes[arrayIndex].tones[3]);
            
            ++slotIndex;
        } else {
            PXLCAM_LOGW_TAG(kLogTag, "Failed to parse palette object %d", slotIndex);
        }
        
        p = objEnd;
    }
    
    free(jsonBuffer);
    PXLCAM_LOGI_TAG(kLogTag, "Loaded %d custom palettes from SD", slotIndex);
    return true;
}

bool palette_save_to_sd() {
    // Check if SD is mounted
    if (SD_MMC.cardType() == CARD_NONE) {
        PXLCAM_LOGW_TAG(kLogTag, "SD not mounted, cannot save palettes");
        return false;
    }
    
    // Create /PXL directory if needed
    if (!SD_MMC.exists("/PXL")) {
        if (!SD_MMC.mkdir("/PXL")) {
            PXLCAM_LOGE_TAG(kLogTag, "Failed to create /PXL directory");
            return false;
        }
        PXLCAM_LOGI_TAG(kLogTag, "Created /PXL directory");
    }
    
    // Build JSON content
    constexpr size_t kMaxJsonSize = 1024;
    char* jsonBuffer = static_cast<char*>(malloc(kMaxJsonSize));
    if (!jsonBuffer) {
        PXLCAM_LOGE_TAG(kLogTag, "Failed to allocate JSON buffer");
        return false;
    }
    
    char* p = jsonBuffer;
    p += snprintf(p, kMaxJsonSize, "{\n  \"custom_palettes\": [\n");
    
    bool first = true;
    for (uint8_t i = 0; i < CUSTOM_PALETTE_COUNT; ++i) {
        if (!s_custom_slots[i].loaded) continue;
        
        const Palette& pal = s_palettes[BUILTIN_PALETTE_COUNT + i];
        
        if (!first) {
            p += snprintf(p, jsonBuffer + kMaxJsonSize - p, ",\n");
        }
        first = false;
        
        p += snprintf(p, jsonBuffer + kMaxJsonSize - p,
                      "    { \"name\": \"%s\", \"tones\": [%d, %d, %d, %d] }",
                      pal.name,
                      pal.tones[0], pal.tones[1], 
                      pal.tones[2], pal.tones[3]);
    }
    
    snprintf(p, jsonBuffer + kMaxJsonSize - p, "\n  ]\n}\n");
    
    // Write to temp file first (atomic write)
    const char* tempPath = "/PXL/palettes.tmp";
    File file = SD_MMC.open(tempPath, FILE_WRITE);
    if (!file) {
        PXLCAM_LOGE_TAG(kLogTag, "Failed to open temp file for writing");
        free(jsonBuffer);
        return false;
    }
    
    size_t written = file.print(jsonBuffer);
    file.close();
    free(jsonBuffer);
    
    if (written == 0) {
        PXLCAM_LOGE_TAG(kLogTag, "Failed to write palette JSON");
        return false;
    }
    
    // Rename temp to final (atomic on most filesystems)
    if (SD_MMC.exists(PALETTE_SD_PATH)) {
        SD_MMC.remove(PALETTE_SD_PATH);
    }
    
    if (!SD_MMC.rename(tempPath, PALETTE_SD_PATH)) {
        PXLCAM_LOGE_TAG(kLogTag, "Failed to rename temp file to %s", PALETTE_SD_PATH);
        return false;
    }
    
    PXLCAM_LOGI_TAG(kLogTag, "Saved custom palettes to %s", PALETTE_SD_PATH);
    return true;
}

#endif // PXLCAM_FEATURE_CUSTOM_PALETTES

// =============================================================================
// Palette Cycling
// =============================================================================

PaletteType palette_cycle_next(PaletteType current, bool includeCustom) {
    uint8_t idx = static_cast<uint8_t>(current);
    uint8_t maxIdx = includeCustom ? (TOTAL_PALETTE_COUNT - 1) : (BUILTIN_PALETTE_COUNT - 1);
    
    ++idx;
    if (idx > maxIdx) {
        idx = 0;
    }
    
#if PXLCAM_FEATURE_CUSTOM_PALETTES
    // Skip unloaded custom slots
    if (includeCustom && idx >= BUILTIN_PALETTE_COUNT) {
        const uint8_t customIdx = idx - BUILTIN_PALETTE_COUNT;
        if (!s_custom_slots[customIdx].loaded) {
            // Try next slot or wrap
            return palette_cycle_next(static_cast<PaletteType>(idx), includeCustom);
        }
    }
#endif
    
    return static_cast<PaletteType>(idx);
}

PaletteType palette_cycle_prev(PaletteType current, bool includeCustom) {
    uint8_t idx = static_cast<uint8_t>(current);
    uint8_t maxIdx = includeCustom ? (TOTAL_PALETTE_COUNT - 1) : (BUILTIN_PALETTE_COUNT - 1);
    
    if (idx == 0) {
        idx = maxIdx;
    } else {
        --idx;
    }
    
#if PXLCAM_FEATURE_CUSTOM_PALETTES
    // Skip unloaded custom slots
    if (includeCustom && idx >= BUILTIN_PALETTE_COUNT) {
        const uint8_t customIdx = idx - BUILTIN_PALETTE_COUNT;
        if (!s_custom_slots[customIdx].loaded) {
            return palette_cycle_prev(static_cast<PaletteType>(idx), includeCustom);
        }
    }
#endif
    
    return static_cast<PaletteType>(idx);
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
