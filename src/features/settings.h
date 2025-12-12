/**
 * @file settings.h
 * @brief NVS Settings Persistence for PXLcam v1.2.0
 * 
 * Provides persistent settings storage using ESP32 NVS.
 * Includes robust fallback to defaults and comprehensive logging.
 * 
 * @author PXLcam Team
 * @version 1.2.0
 * @date 2024
 */

#ifndef PXLCAM_SETTINGS_H
#define PXLCAM_SETTINGS_H

#include <cstdint>
#include "../core/app_context.h"

namespace pxlcam {
namespace features {

/**
 * @brief NVS namespace for PXLcam settings
 */
namespace NvsConfig {
    constexpr const char* NAMESPACE = "pxlcam";      ///< NVS namespace
    constexpr uint8_t SETTINGS_VERSION = 2;          ///< Settings format version
}

/**
 * @brief NVS key names for persisted fields
 */
namespace NvsKey {
    constexpr const char* CURRENT_MODE = "mode";           ///< CameraMode
    constexpr const char* PALETTE_ID = "palette";          ///< Palette ID
    constexpr const char* BRIGHTNESS = "brightness";       ///< Display brightness
    constexpr const char* CAPTURE_STYLE = "capStyle";      ///< Capture style/effect
    constexpr const char* LAST_EXPOSURE = "lastExp";       ///< Last exposure value
    constexpr const char* SETTINGS_VER = "version";        ///< Settings version marker
    constexpr const char* INITIALIZED = "init";            ///< Initialization marker
}

/**
 * @brief Capture style enumeration
 */
enum class CaptureStyle : uint8_t {
    NORMAL = 0,         ///< Standard capture
    DITHERED,           ///< Dithered pixel art
    OUTLINE,            ///< Edge detection
    POSTERIZED,         ///< Reduced colors
    STYLE_COUNT
};

/**
 * @brief Persisted settings data structure
 * 
 * Contains all fields that are saved to NVS.
 */
struct PersistedSettings {
    core::CameraMode currentMode;   ///< Current camera mode
    core::Palette paletteId;        ///< Selected color palette
    uint8_t brightness;             ///< Display brightness (0-255)
    CaptureStyle captureStyle;      ///< Capture style/effect
    int8_t lastExposure;            ///< Last exposure compensation (-2 to +2)

    /**
     * @brief Get default settings values
     * @return PersistedSettings Default values
     */
    static PersistedSettings defaults() {
        return {
            .currentMode = core::CameraMode::STANDARD,
            .paletteId = core::Palette::FULL_COLOR,
            .brightness = 200,
            .captureStyle = CaptureStyle::NORMAL,
            .lastExposure = 0
        };
    }
};

/**
 * @brief Settings persistence namespace
 * 
 * Provides synchronous, thread-safe NVS operations with
 * robust fallback and comprehensive logging.
 * 
 * @code
 * // Load settings into context
 * auto& ctx = core::AppContext::instance();
 * settings::load(ctx);
 * 
 * // Modify and save
 * ctx.setMode(core::CameraMode::PIXEL_ART);
 * settings::save(ctx);
 * @endcode
 */
namespace settings {

    /**
     * @brief Initialize NVS settings system
     * 
     * Must be called once at startup before load/save operations.
     * Initializes NVS flash if not already done.
     * 
     * @return bool True if initialization successful
     */
    bool init();

    /**
     * @brief Load settings from NVS into AppContext
     * 
     * Loads all persisted fields from NVS and applies them
     * to the provided AppContext. Falls back to defaults
     * on any read error.
     * 
     * @param ctx AppContext to populate with loaded settings
     */
    void load(core::AppContext& ctx);

    /**
     * @brief Save settings from AppContext to NVS
     * 
     * Persists all relevant fields from AppContext to NVS.
     * All operations are logged for debugging.
     * 
     * @param ctx AppContext containing settings to save
     */
    void save(const core::AppContext& ctx);

    /**
     * @brief Load default values into AppContext
     * 
     * Applies factory default settings. Called automatically
     * on first boot or when NVS read fails.
     * 
     * @param ctx AppContext to populate with defaults
     */
    void loadDefaultValues(core::AppContext& ctx);

    /**
     * @brief Reset all settings to factory defaults
     * 
     * Clears NVS storage and applies default values.
     * 
     * @param ctx AppContext to reset
     * @return bool True if reset successful
     */
    bool resetToDefaults(core::AppContext& ctx);

    /**
     * @brief Check if this is first boot (no saved settings)
     * @return bool True if first boot
     */
    bool isFirstBoot();

    /**
     * @brief Get current persisted settings snapshot
     * @return PersistedSettings Current persisted values
     */
    PersistedSettings getPersistedSettings();

    /**
     * @brief Save individual field: currentMode
     * @param mode Camera mode to save
     * @return bool True if saved successfully
     */
    bool saveCurrentMode(core::CameraMode mode);

    /**
     * @brief Save individual field: paletteId
     * @param palette Palette to save
     * @return bool True if saved successfully
     */
    bool savePaletteId(core::Palette palette);

    /**
     * @brief Save individual field: brightness
     * @param brightness Display brightness (0-255)
     * @return bool True if saved successfully
     */
    bool saveBrightness(uint8_t brightness);

    /**
     * @brief Save individual field: captureStyle
     * @param style Capture style to save
     * @return bool True if saved successfully
     */
    bool saveCaptureStyle(CaptureStyle style);

    /**
     * @brief Save individual field: lastExposure
     * @param exposure Exposure compensation (-2 to +2)
     * @return bool True if saved successfully
     */
    bool saveLastExposure(int8_t exposure);

    /**
     * @brief Check if NVS is available and ready
     * @return bool True if NVS is operational
     */
    bool isAvailable();

    /**
     * @brief Get NVS free entries count
     * @return size_t Number of free NVS entries
     */
    size_t getFreeEntries();

} // namespace settings

} // namespace features
} // namespace pxlcam

#endif // PXLCAM_SETTINGS_H
