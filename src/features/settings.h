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
    // v1.2.0 new keys
    constexpr const char* STYLE_MODE = "styleMode";        ///< StyleMode (Normal/GameBoy/Night)
    constexpr const char* NIGHT_MODE = "nightMode";        ///< Night mode flag
    constexpr const char* AUTO_EXPOSURE = "autoExp";       ///< Auto exposure flag
}

/**
 * @brief Capture style enumeration (v1.2.0)
 */
enum class CaptureStyle : uint8_t {
    NORMAL = 0,         ///< Standard capture
    DITHERED,           ///< Dithered pixel art
    OUTLINE,            ///< Edge detection
    POSTERIZED,         ///< Reduced colors
    STYLE_COUNT
};

/**
 * @brief Style mode enumeration (v1.2.0)
 * Maps to user-visible style selection
 */
enum class StyleMode : uint8_t {
    NORMAL = 0,         ///< Standard capture, no effects
    GAMEBOY = 1,        ///< GameBoy 4-tone dithering
    NIGHT = 2,          ///< Night mode with gamma boost
    STYLE_MODE_COUNT
};

/**
 * @brief Simplified settings struct (v1.2.0 user request)
 * 
 * Contains the 3 user-configurable settings:
 * - styleMode: Normal/GameBoy/Night
 * - nightMode: Enable night boost
 * - autoExposure: Enable auto exposure
 */
struct PxlcamSettings {
    uint8_t styleMode;      ///< StyleMode enum value
    uint8_t nightMode;      ///< 1 = enabled, 0 = disabled
    uint8_t autoExposure;   ///< 1 = enabled, 0 = disabled
    
    /**
     * @brief Get default settings
     */
    static PxlcamSettings defaults() {
        return {
            .styleMode = static_cast<uint8_t>(StyleMode::NORMAL),
            .nightMode = 0,
            .autoExposure = 1
        };
    }
    
    /**
     * @brief Serialize to byte buffer
     * @param buf Output buffer (minimum 3 bytes)
     * @param maxLen Buffer size
     * @return Number of bytes written (3 on success, 0 on failure)
     */
    size_t serialize(uint8_t* buf, size_t maxLen) const {
        if (maxLen < 3) return 0;
        buf[0] = styleMode;
        buf[1] = nightMode;
        buf[2] = autoExposure;
        return 3;
    }
    
    /**
     * @brief Deserialize from byte buffer
     * @param buf Input buffer
     * @param len Buffer length
     * @return Deserialized settings (defaults if invalid)
     */
    static PxlcamSettings deserialize(const uint8_t* buf, size_t len) {
        if (len < 3) return defaults();
        return {
            .styleMode = buf[0],
            .nightMode = buf[1],
            .autoExposure = buf[2]
        };
    }
    
    /**
     * @brief Equality operator
     */
    bool operator==(const PxlcamSettings& other) const {
        return styleMode == other.styleMode &&
               nightMode == other.nightMode &&
               autoExposure == other.autoExposure;
    }
};

/**
 * @brief Persisted settings data structure (extended)
 * 
 * Contains all fields that are saved to NVS.
 */
struct PersistedSettings {
    core::CameraMode currentMode;   ///< Current camera mode
    core::Palette paletteId;        ///< Selected color palette
    uint8_t brightness;             ///< Display brightness (0-255)
    CaptureStyle captureStyle;      ///< Capture style/effect
    int8_t lastExposure;            ///< Last exposure compensation (-2 to +2)
    StyleMode styleMode;            ///< v1.2.0: Style mode selection
    bool nightModeEnabled;          ///< v1.2.0: Night mode flag
    bool autoExposureEnabled;       ///< v1.2.0: Auto exposure flag

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
            .lastExposure = 0,
            .styleMode = StyleMode::NORMAL,
            .nightModeEnabled = false,
            .autoExposureEnabled = true
        };
    }
    
    /**
     * @brief Convert to PxlcamSettings (v1.2.0 simplified struct)
     */
    PxlcamSettings toPxlcamSettings() const {
        return {
            .styleMode = static_cast<uint8_t>(styleMode),
            .nightMode = static_cast<uint8_t>(nightModeEnabled ? 1 : 0),
            .autoExposure = static_cast<uint8_t>(autoExposureEnabled ? 1 : 0)
        };
    }
    
    /**
     * @brief Apply PxlcamSettings values
     */
    void fromPxlcamSettings(const PxlcamSettings& s) {
        styleMode = static_cast<StyleMode>(s.styleMode);
        nightModeEnabled = s.nightMode != 0;
        autoExposureEnabled = s.autoExposure != 0;
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

    //==========================================================================
    // v1.2.0 PxlcamSettings API (simplified 3-field struct)
    //==========================================================================

    /**
     * @brief Load PxlcamSettings from NVS
     * @return PxlcamSettings Loaded settings or defaults
     */
    PxlcamSettings loadPxlcamSettings();

    /**
     * @brief Save PxlcamSettings to NVS
     * @param settings Settings to save
     * @return bool True if saved successfully
     */
    bool savePxlcamSettings(const PxlcamSettings& settings);

    /**
     * @brief Save individual field: styleMode
     * @param mode StyleMode value (0=Normal, 1=GameBoy, 2=Night)
     * @return bool True if saved successfully
     */
    bool saveStyleMode(StyleMode mode);

    /**
     * @brief Save individual field: nightMode
     * @param enabled Night mode enabled flag
     * @return bool True if saved successfully
     */
    bool saveNightMode(bool enabled);

    /**
     * @brief Save individual field: autoExposure
     * @param enabled Auto exposure enabled flag
     * @return bool True if saved successfully
     */
    bool saveAutoExposure(bool enabled);

    /**
     * @brief Get current StyleMode from NVS
     * @return StyleMode Current style mode or NORMAL
     */
    StyleMode getStyleMode();

    /**
     * @brief Get night mode state from NVS
     * @return bool Night mode enabled state
     */
    bool getNightMode();

    /**
     * @brief Get auto exposure state from NVS
     * @return bool Auto exposure enabled state
     */
    bool getAutoExposure();

} // namespace settings

} // namespace features
} // namespace pxlcam

#endif // PXLCAM_SETTINGS_H
