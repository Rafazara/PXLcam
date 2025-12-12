/**
 * @file app_context.h
 * @brief Application Context for PXLcam v1.2.0
 * 
 * Central application state container holding:
 * - Current operational mode
 * - Exposure settings
 * - Color palette selection
 * - System configuration
 * 
 * @author PXLcam Team
 * @version 1.2.0
 * @date 2024
 */

#ifndef PXLCAM_APP_CONTEXT_H
#define PXLCAM_APP_CONTEXT_H

#include <cstdint>
#include <cstring>

namespace pxlcam {
namespace core {

/**
 * @brief Camera operational modes
 */
enum class CameraMode : uint8_t {
    STANDARD = 0,   ///< Standard photo mode
    PIXEL_ART,      ///< Pixel art mode with dithering
    RETRO,          ///< Retro/vintage effect
    MONOCHROME,     ///< Black and white mode
    MODE_COUNT      ///< Number of modes (sentinel)
};

/**
 * @brief Color palette options
 */
enum class Palette : uint8_t {
    FULL_COLOR = 0, ///< Full RGB color
    GAMEBOY,        ///< 4-color GameBoy palette
    CGA,            ///< CGA 4-color palette
    EGA,            ///< EGA 16-color palette
    SEPIA,          ///< Sepia tone
    CUSTOM,         ///< User-defined palette
    PALETTE_COUNT   ///< Number of palettes (sentinel)
};

/**
 * @brief Exposure settings structure
 */
struct ExposureSettings {
    int8_t brightness;      ///< Brightness adjustment (-2 to +2)
    int8_t contrast;        ///< Contrast adjustment (-2 to +2)
    int8_t saturation;      ///< Saturation adjustment (-2 to +2)
    uint8_t gainCeiling;    ///< Auto gain ceiling (0-6)
    bool autoExposure;      ///< Auto exposure enable
    bool autoWhiteBalance;  ///< Auto white balance enable

    /**
     * @brief Default exposure settings
     */
    static ExposureSettings defaults() {
        return {
            .brightness = 0,
            .contrast = 0,
            .saturation = 0,
            .gainCeiling = 2,
            .autoExposure = true,
            .autoWhiteBalance = true
        };
    }
};

/**
 * @brief System configuration structure
 */
struct SystemConfig {
    uint8_t displayBrightness;  ///< Display brightness (0-255)
    uint8_t previewFps;         ///< Preview frames per second
    bool soundEnabled;          ///< Sound feedback enable
    bool debugMode;             ///< Debug output enable
    char deviceName[16];        ///< Device name for identification

    /**
     * @brief Default system configuration
     */
    static SystemConfig defaults() {
        SystemConfig cfg;
        cfg.displayBrightness = 200;
        cfg.previewFps = 15;
        cfg.soundEnabled = true;
        cfg.debugMode = false;
        strncpy(cfg.deviceName, "PXLcam", sizeof(cfg.deviceName) - 1);
        cfg.deviceName[sizeof(cfg.deviceName) - 1] = '\0';
        return cfg;
    }
};

/**
 * @brief Application Context class
 * 
 * Singleton-like context holding all application state.
 * Use AppContext::instance() to access.
 * 
 * @code
 * auto& ctx = AppContext::instance();
 * ctx.setMode(CameraMode::PIXEL_ART);
 * ctx.setPalette(Palette::GAMEBOY);
 * @endcode
 */
class AppContext {
public:
    /**
     * @brief Get singleton instance
     * @return AppContext& Reference to singleton
     */
    static AppContext& instance() {
        static AppContext ctx;
        return ctx;
    }

    // Delete copy/move constructors for singleton
    AppContext(const AppContext&) = delete;
    AppContext& operator=(const AppContext&) = delete;
    AppContext(AppContext&&) = delete;
    AppContext& operator=(AppContext&&) = delete;

    /**
     * @brief Initialize context with defaults
     */
    void init() {
        mode_ = CameraMode::STANDARD;
        palette_ = Palette::FULL_COLOR;
        exposure_ = ExposureSettings::defaults();
        config_ = SystemConfig::defaults();
        initialized_ = true;
    }

    /**
     * @brief Check if context is initialized
     * @return bool True if initialized
     */
    bool isInitialized() const { return initialized_; }

    // Mode accessors
    CameraMode getMode() const { return mode_; }
    void setMode(CameraMode mode) { mode_ = mode; }

    // Palette accessors
    Palette getPalette() const { return palette_; }
    void setPalette(Palette palette) { palette_ = palette; }

    // Exposure accessors
    const ExposureSettings& getExposure() const { return exposure_; }
    ExposureSettings& getExposureMutable() { return exposure_; }
    void setExposure(const ExposureSettings& exposure) { exposure_ = exposure; }

    // Config accessors
    const SystemConfig& getConfig() const { return config_; }
    SystemConfig& getConfigMutable() { return config_; }
    void setConfig(const SystemConfig& config) { config_ = config; }

    /**
     * @brief Get mode as string
     * @return const char* Mode name
     */
    const char* getModeString() const {
        switch (mode_) {
            case CameraMode::STANDARD:   return "Standard";
            case CameraMode::PIXEL_ART:  return "Pixel Art";
            case CameraMode::RETRO:      return "Retro";
            case CameraMode::MONOCHROME: return "Mono";
            default:                     return "Unknown";
        }
    }

    /**
     * @brief Get palette as string
     * @return const char* Palette name
     */
    const char* getPaletteString() const {
        switch (palette_) {
            case Palette::FULL_COLOR: return "Full Color";
            case Palette::GAMEBOY:    return "GameBoy";
            case Palette::CGA:        return "CGA";
            case Palette::EGA:        return "EGA";
            case Palette::SEPIA:      return "Sepia";
            case Palette::CUSTOM:     return "Custom";
            default:                  return "Unknown";
        }
    }

private:
    /**
     * @brief Private constructor for singleton
     */
    AppContext() : mode_(CameraMode::STANDARD)
                 , palette_(Palette::FULL_COLOR)
                 , initialized_(false)
    {}

    CameraMode mode_;           ///< Current camera mode
    Palette palette_;           ///< Current color palette
    ExposureSettings exposure_; ///< Exposure settings
    SystemConfig config_;       ///< System configuration
    bool initialized_;          ///< Initialization flag
};

} // namespace core
} // namespace pxlcam

#endif // PXLCAM_APP_CONTEXT_H
