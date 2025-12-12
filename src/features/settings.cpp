/**
 * @file settings.cpp
 * @brief NVS Settings Persistence Implementation for PXLcam v1.2.0
 * 
 * Implements ESP32 NVS-based persistent storage with:
 * - Robust fallback to defaults on any error
 * - Comprehensive logging of all operations
 * - Thread-safe synchronous API
 * 
 * @author PXLcam Team
 * @version 1.2.0
 * @date 2024
 */

#include "settings.h"
#include <Arduino.h>
#include <nvs_flash.h>
#include <nvs.h>

namespace pxlcam {
namespace features {
namespace settings {

// ============================================================================
// Internal State
// ============================================================================

namespace {
    bool g_initialized = false;         ///< NVS initialization flag
    bool g_firstBoot = true;            ///< First boot detection
    nvs_handle_t g_nvsHandle = 0;       ///< NVS handle for operations
    CaptureStyle g_captureStyle = CaptureStyle::NORMAL;  ///< Cached capture style
    
    /**
     * @brief Log tag for settings operations
     */
    constexpr const char* TAG = "[Settings]";
    
    /**
     * @brief Open NVS namespace with read-write access
     * @return bool True if opened successfully
     */
    bool openNvs() {
        if (g_nvsHandle != 0) {
            return true;  // Already open
        }
        
        esp_err_t err = nvs_open(NvsConfig::NAMESPACE, NVS_READWRITE, &g_nvsHandle);
        if (err != ESP_OK) {
            Serial.printf("%s ERROR: nvs_open failed: %s\n", TAG, esp_err_to_name(err));
            g_nvsHandle = 0;
            return false;
        }
        return true;
    }
    
    /**
     * @brief Close NVS handle
     */
    void closeNvs() {
        if (g_nvsHandle != 0) {
            nvs_close(g_nvsHandle);
            g_nvsHandle = 0;
        }
    }
    
    /**
     * @brief Commit pending NVS writes
     * @return bool True if commit successful
     */
    bool commitNvs() {
        if (g_nvsHandle == 0) return false;
        
        esp_err_t err = nvs_commit(g_nvsHandle);
        if (err != ESP_OK) {
            Serial.printf("%s ERROR: nvs_commit failed: %s\n", TAG, esp_err_to_name(err));
            return false;
        }
        return true;
    }
    
    /**
     * @brief Read uint8_t value from NVS
     * @param key NVS key
     * @param value Output value
     * @return bool True if read successful
     */
    bool readU8(const char* key, uint8_t& value) {
        if (g_nvsHandle == 0) return false;
        
        esp_err_t err = nvs_get_u8(g_nvsHandle, key, &value);
        if (err == ESP_OK) {
            Serial.printf("%s Load '%s' = %u\n", TAG, key, value);
            return true;
        } else if (err == ESP_ERR_NVS_NOT_FOUND) {
            Serial.printf("%s Key '%s' not found (using default)\n", TAG, key);
        } else {
            Serial.printf("%s ERROR reading '%s': %s\n", TAG, key, esp_err_to_name(err));
        }
        return false;
    }
    
    /**
     * @brief Read int8_t value from NVS
     * @param key NVS key
     * @param value Output value
     * @return bool True if read successful
     */
    bool readI8(const char* key, int8_t& value) {
        if (g_nvsHandle == 0) return false;
        
        esp_err_t err = nvs_get_i8(g_nvsHandle, key, &value);
        if (err == ESP_OK) {
            Serial.printf("%s Load '%s' = %d\n", TAG, key, value);
            return true;
        } else if (err == ESP_ERR_NVS_NOT_FOUND) {
            Serial.printf("%s Key '%s' not found (using default)\n", TAG, key);
        } else {
            Serial.printf("%s ERROR reading '%s': %s\n", TAG, key, esp_err_to_name(err));
        }
        return false;
    }
    
    /**
     * @brief Write uint8_t value to NVS
     * @param key NVS key
     * @param value Value to write
     * @return bool True if write successful
     */
    bool writeU8(const char* key, uint8_t value) {
        if (g_nvsHandle == 0) return false;
        
        esp_err_t err = nvs_set_u8(g_nvsHandle, key, value);
        if (err == ESP_OK) {
            Serial.printf("%s Save '%s' = %u\n", TAG, key, value);
            return true;
        }
        Serial.printf("%s ERROR writing '%s': %s\n", TAG, key, esp_err_to_name(err));
        return false;
    }
    
    /**
     * @brief Write int8_t value to NVS
     * @param key NVS key
     * @param value Value to write
     * @return bool True if write successful
     */
    bool writeI8(const char* key, int8_t value) {
        if (g_nvsHandle == 0) return false;
        
        esp_err_t err = nvs_set_i8(g_nvsHandle, key, value);
        if (err == ESP_OK) {
            Serial.printf("%s Save '%s' = %d\n", TAG, key, value);
            return true;
        }
        Serial.printf("%s ERROR writing '%s': %s\n", TAG, key, esp_err_to_name(err));
        return false;
    }
    
} // anonymous namespace

// ============================================================================
// Public API Implementation
// ============================================================================

bool init() {
    Serial.printf("%s Initializing NVS (namespace: '%s')...\n", TAG, NvsConfig::NAMESPACE);
    
    // Initialize NVS flash partition
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        Serial.printf("%s NVS partition needs erase, reformatting...\n", TAG);
        err = nvs_flash_erase();
        if (err != ESP_OK) {
            Serial.printf("%s ERROR: nvs_flash_erase failed: %s\n", TAG, esp_err_to_name(err));
            return false;
        }
        err = nvs_flash_init();
    }
    
    if (err != ESP_OK) {
        Serial.printf("%s ERROR: nvs_flash_init failed: %s\n", TAG, esp_err_to_name(err));
        return false;
    }
    
    // Open namespace
    if (!openNvs()) {
        return false;
    }
    
    // Check for initialization marker to detect first boot
    uint8_t initMarker = 0;
    if (readU8(NvsKey::INITIALIZED, initMarker) && initMarker == 0xAA) {
        g_firstBoot = false;
        Serial.printf("%s Previous settings detected\n", TAG);
    } else {
        g_firstBoot = true;
        Serial.printf("%s First boot detected\n", TAG);
    }
    
    // Check settings version for migrations
    uint8_t version = 0;
    if (readU8(NvsKey::SETTINGS_VER, version)) {
        if (version != NvsConfig::SETTINGS_VERSION) {
            Serial.printf("%s Settings version mismatch (stored: %u, current: %u)\n", 
                          TAG, version, NvsConfig::SETTINGS_VERSION);
            // Future: handle migration here
        }
    }
    
    g_initialized = true;
    Serial.printf("%s Initialized successfully\n", TAG);
    return true;
}

void load(core::AppContext& ctx) {
    Serial.printf("%s Loading settings from NVS...\n", TAG);
    
    if (!g_initialized) {
        Serial.printf("%s WARNING: Not initialized, calling init()\n", TAG);
        if (!init()) {
            Serial.printf("%s FALLBACK: Using defaults due to init failure\n", TAG);
            loadDefaultValues(ctx);
            return;
        }
    }
    
    if (!openNvs()) {
        Serial.printf("%s FALLBACK: Using defaults due to NVS open failure\n", TAG);
        loadDefaultValues(ctx);
        return;
    }
    
    PersistedSettings defaults = PersistedSettings::defaults();
    bool anyError = false;
    
    // Load currentMode
    uint8_t modeValue = static_cast<uint8_t>(defaults.currentMode);
    if (readU8(NvsKey::CURRENT_MODE, modeValue)) {
        if (modeValue < static_cast<uint8_t>(core::CameraMode::MODE_COUNT)) {
            ctx.setMode(static_cast<core::CameraMode>(modeValue));
        } else {
            Serial.printf("%s WARNING: Invalid mode %u, using default\n", TAG, modeValue);
            ctx.setMode(defaults.currentMode);
            anyError = true;
        }
    } else {
        ctx.setMode(defaults.currentMode);
    }
    
    // Load paletteId
    uint8_t paletteValue = static_cast<uint8_t>(defaults.paletteId);
    if (readU8(NvsKey::PALETTE_ID, paletteValue)) {
        if (paletteValue < static_cast<uint8_t>(core::Palette::PALETTE_COUNT)) {
            ctx.setPalette(static_cast<core::Palette>(paletteValue));
        } else {
            Serial.printf("%s WARNING: Invalid palette %u, using default\n", TAG, paletteValue);
            ctx.setPalette(defaults.paletteId);
            anyError = true;
        }
    } else {
        ctx.setPalette(defaults.paletteId);
    }
    
    // Load brightness
    uint8_t brightness = defaults.brightness;
    if (readU8(NvsKey::BRIGHTNESS, brightness)) {
        auto& config = ctx.getConfigMutable();
        config.displayBrightness = brightness;
    } else {
        auto& config = ctx.getConfigMutable();
        config.displayBrightness = defaults.brightness;
    }
    
    // Load captureStyle (cached internally)
    uint8_t styleValue = static_cast<uint8_t>(defaults.captureStyle);
    if (readU8(NvsKey::CAPTURE_STYLE, styleValue)) {
        if (styleValue < static_cast<uint8_t>(CaptureStyle::STYLE_COUNT)) {
            g_captureStyle = static_cast<CaptureStyle>(styleValue);
        } else {
            Serial.printf("%s WARNING: Invalid capture style %u, using default\n", TAG, styleValue);
            g_captureStyle = defaults.captureStyle;
            anyError = true;
        }
    } else {
        g_captureStyle = defaults.captureStyle;
    }
    
    // Load lastExposure
    int8_t exposure = defaults.lastExposure;
    if (readI8(NvsKey::LAST_EXPOSURE, exposure)) {
        // Clamp to valid range
        if (exposure < -2) exposure = -2;
        if (exposure > 2) exposure = 2;
        auto& expSettings = ctx.getExposureMutable();
        expSettings.brightness = exposure;
    } else {
        auto& expSettings = ctx.getExposureMutable();
        expSettings.brightness = defaults.lastExposure;
    }
    
    if (anyError) {
        Serial.printf("%s Load completed with warnings\n", TAG);
    } else {
        Serial.printf("%s Load completed successfully\n", TAG);
    }
    
    Serial.printf("%s Loaded: mode=%s, palette=%s, brightness=%u, style=%u, exposure=%d\n",
                  TAG,
                  ctx.getModeString(),
                  ctx.getPaletteString(),
                  ctx.getConfig().displayBrightness,
                  static_cast<uint8_t>(g_captureStyle),
                  ctx.getExposure().brightness);
}

void save(const core::AppContext& ctx) {
    Serial.printf("%s Saving settings to NVS...\n", TAG);
    
    if (!g_initialized) {
        Serial.printf("%s ERROR: Not initialized, cannot save\n", TAG);
        return;
    }
    
    if (!openNvs()) {
        Serial.printf("%s ERROR: Failed to open NVS for writing\n", TAG);
        return;
    }
    
    bool allSuccess = true;
    
    // Save currentMode
    if (!writeU8(NvsKey::CURRENT_MODE, static_cast<uint8_t>(ctx.getMode()))) {
        allSuccess = false;
    }
    
    // Save paletteId
    if (!writeU8(NvsKey::PALETTE_ID, static_cast<uint8_t>(ctx.getPalette()))) {
        allSuccess = false;
    }
    
    // Save brightness
    if (!writeU8(NvsKey::BRIGHTNESS, ctx.getConfig().displayBrightness)) {
        allSuccess = false;
    }
    
    // Save captureStyle
    if (!writeU8(NvsKey::CAPTURE_STYLE, static_cast<uint8_t>(g_captureStyle))) {
        allSuccess = false;
    }
    
    // Save lastExposure
    if (!writeI8(NvsKey::LAST_EXPOSURE, ctx.getExposure().brightness)) {
        allSuccess = false;
    }
    
    // Mark as initialized and set version
    if (!writeU8(NvsKey::INITIALIZED, 0xAA)) {
        allSuccess = false;
    }
    if (!writeU8(NvsKey::SETTINGS_VER, NvsConfig::SETTINGS_VERSION)) {
        allSuccess = false;
    }
    
    // Commit changes
    if (!commitNvs()) {
        allSuccess = false;
    }
    
    if (allSuccess) {
        g_firstBoot = false;
        Serial.printf("%s Save completed successfully\n", TAG);
    } else {
        Serial.printf("%s Save completed with errors\n", TAG);
    }
    
    Serial.printf("%s Saved: mode=%u, palette=%u, brightness=%u, style=%u, exposure=%d\n",
                  TAG,
                  static_cast<uint8_t>(ctx.getMode()),
                  static_cast<uint8_t>(ctx.getPalette()),
                  ctx.getConfig().displayBrightness,
                  static_cast<uint8_t>(g_captureStyle),
                  ctx.getExposure().brightness);
}

void loadDefaultValues(core::AppContext& ctx) {
    Serial.printf("%s Loading default values...\n", TAG);
    
    PersistedSettings defaults = PersistedSettings::defaults();
    
    ctx.setMode(defaults.currentMode);
    ctx.setPalette(defaults.paletteId);
    
    auto& config = ctx.getConfigMutable();
    config.displayBrightness = defaults.brightness;
    
    auto& exposure = ctx.getExposureMutable();
    exposure.brightness = defaults.lastExposure;
    
    g_captureStyle = defaults.captureStyle;
    
    Serial.printf("%s Defaults applied: mode=%s, palette=%s, brightness=%u, style=%u, exposure=%d\n",
                  TAG,
                  ctx.getModeString(),
                  ctx.getPaletteString(),
                  defaults.brightness,
                  static_cast<uint8_t>(defaults.captureStyle),
                  defaults.lastExposure);
}

bool resetToDefaults(core::AppContext& ctx) {
    Serial.printf("%s Resetting to factory defaults...\n", TAG);
    
    if (!g_initialized) {
        Serial.printf("%s WARNING: Not initialized\n", TAG);
    }
    
    if (openNvs()) {
        // Erase all keys in namespace
        esp_err_t err = nvs_erase_all(g_nvsHandle);
        if (err != ESP_OK) {
            Serial.printf("%s ERROR: nvs_erase_all failed: %s\n", TAG, esp_err_to_name(err));
        } else {
            commitNvs();
            Serial.printf("%s NVS namespace erased\n", TAG);
        }
    }
    
    // Apply defaults to context
    loadDefaultValues(ctx);
    g_firstBoot = true;
    
    Serial.printf("%s Reset complete\n", TAG);
    return true;
}

bool isFirstBoot() {
    return g_firstBoot;
}

PersistedSettings getPersistedSettings() {
    PersistedSettings settings = PersistedSettings::defaults();
    
    if (!g_initialized || !openNvs()) {
        return settings;
    }
    
    uint8_t u8Val;
    int8_t i8Val;
    
    if (readU8(NvsKey::CURRENT_MODE, u8Val)) {
        settings.currentMode = static_cast<core::CameraMode>(u8Val);
    }
    if (readU8(NvsKey::PALETTE_ID, u8Val)) {
        settings.paletteId = static_cast<core::Palette>(u8Val);
    }
    if (readU8(NvsKey::BRIGHTNESS, u8Val)) {
        settings.brightness = u8Val;
    }
    if (readU8(NvsKey::CAPTURE_STYLE, u8Val)) {
        settings.captureStyle = static_cast<CaptureStyle>(u8Val);
    }
    if (readI8(NvsKey::LAST_EXPOSURE, i8Val)) {
        settings.lastExposure = i8Val;
    }
    
    return settings;
}

bool saveCurrentMode(core::CameraMode mode) {
    Serial.printf("%s Saving currentMode...\n", TAG);
    
    if (!g_initialized || !openNvs()) {
        Serial.printf("%s ERROR: Cannot save - not initialized\n", TAG);
        return false;
    }
    
    bool success = writeU8(NvsKey::CURRENT_MODE, static_cast<uint8_t>(mode));
    if (success) {
        commitNvs();
    }
    return success;
}

bool savePaletteId(core::Palette palette) {
    Serial.printf("%s Saving paletteId...\n", TAG);
    
    if (!g_initialized || !openNvs()) {
        Serial.printf("%s ERROR: Cannot save - not initialized\n", TAG);
        return false;
    }
    
    bool success = writeU8(NvsKey::PALETTE_ID, static_cast<uint8_t>(palette));
    if (success) {
        commitNvs();
    }
    return success;
}

bool saveBrightness(uint8_t brightness) {
    Serial.printf("%s Saving brightness...\n", TAG);
    
    if (!g_initialized || !openNvs()) {
        Serial.printf("%s ERROR: Cannot save - not initialized\n", TAG);
        return false;
    }
    
    bool success = writeU8(NvsKey::BRIGHTNESS, brightness);
    if (success) {
        commitNvs();
    }
    return success;
}

bool saveCaptureStyle(CaptureStyle style) {
    Serial.printf("%s Saving captureStyle...\n", TAG);
    
    if (!g_initialized || !openNvs()) {
        Serial.printf("%s ERROR: Cannot save - not initialized\n", TAG);
        return false;
    }
    
    bool success = writeU8(NvsKey::CAPTURE_STYLE, static_cast<uint8_t>(style));
    if (success) {
        g_captureStyle = style;
        commitNvs();
    }
    return success;
}

bool saveLastExposure(int8_t exposure) {
    Serial.printf("%s Saving lastExposure...\n", TAG);
    
    if (!g_initialized || !openNvs()) {
        Serial.printf("%s ERROR: Cannot save - not initialized\n", TAG);
        return false;
    }
    
    // Clamp to valid range
    if (exposure < -2) exposure = -2;
    if (exposure > 2) exposure = 2;
    
    bool success = writeI8(NvsKey::LAST_EXPOSURE, exposure);
    if (success) {
        commitNvs();
    }
    return success;
}

bool isAvailable() {
    return g_initialized;
}

size_t getFreeEntries() {
    if (!g_initialized || !openNvs()) {
        return 0;
    }
    
    nvs_stats_t stats;
    esp_err_t err = nvs_get_stats(NULL, &stats);
    if (err != ESP_OK) {
        Serial.printf("%s ERROR: nvs_get_stats failed: %s\n", TAG, esp_err_to_name(err));
        return 0;
    }
    
    return stats.free_entries;
}

} // namespace settings
} // namespace features
} // namespace pxlcam
