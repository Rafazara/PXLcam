/**
 * @file settings.h
 * @brief Settings Management for PXLcam v1.2.0
 * 
 * Provides persistent settings storage and retrieval.
 * Works with HAL storage abstraction for portability.
 * 
 * @author PXLcam Team
 * @version 1.2.0
 * @date 2024
 */

#ifndef PXLCAM_SETTINGS_H
#define PXLCAM_SETTINGS_H

#include <cstdint>
#include "../core/app_context.h"
#include "../hal/hal_storage.h"

namespace pxlcam {
namespace features {

/**
 * @brief Settings storage keys
 */
namespace SettingsKey {
    constexpr const char* CAMERA_MODE = "cam_mode";
    constexpr const char* PALETTE = "palette";
    constexpr const char* EXPOSURE = "exposure";
    constexpr const char* SYSTEM_CONFIG = "sys_cfg";
    constexpr const char* FIRST_BOOT = "first_boot";
}

/**
 * @brief Settings version for migration support
 */
constexpr uint8_t SETTINGS_VERSION = 1;

/**
 * @brief Settings header for stored data
 */
struct SettingsHeader {
    uint8_t version;        ///< Settings format version
    uint8_t checksum;       ///< Simple checksum for validation
    uint16_t reserved;      ///< Reserved for future use
};

/**
 * @brief Settings Manager class
 * 
 * Handles loading and saving of application settings.
 * Uses HAL storage interface for persistence.
 * 
 * @code
 * Settings settings(&storage);
 * settings.init();
 * settings.load();
 * // Modify context...
 * settings.save();
 * @endcode
 */
class Settings {
public:
    /**
     * @brief Constructor
     * @param storage Pointer to storage HAL implementation
     */
    explicit Settings(hal::IStorage* storage);

    /**
     * @brief Destructor
     */
    ~Settings() = default;

    /**
     * @brief Initialize settings manager
     * @return bool True if successful
     */
    bool init();

    /**
     * @brief Load all settings into AppContext
     * @return bool True if successful
     */
    bool load();

    /**
     * @brief Save all settings from AppContext
     * @return bool True if successful
     */
    bool save();

    /**
     * @brief Reset settings to defaults
     * @return bool True if successful
     */
    bool resetToDefaults();

    /**
     * @brief Check if this is first boot
     * @return bool True if first boot
     */
    bool isFirstBoot() const { return firstBoot_; }

    /**
     * @brief Mark first boot as complete
     */
    void markFirstBootComplete();

    /**
     * @brief Save camera mode
     * @param mode Camera mode
     * @return bool True if successful
     */
    bool saveCameraMode(core::CameraMode mode);

    /**
     * @brief Save palette
     * @param palette Color palette
     * @return bool True if successful
     */
    bool savePalette(core::Palette palette);

    /**
     * @brief Save exposure settings
     * @param exposure Exposure settings
     * @return bool True if successful
     */
    bool saveExposure(const core::ExposureSettings& exposure);

    /**
     * @brief Save system config
     * @param config System configuration
     * @return bool True if successful
     */
    bool saveSystemConfig(const core::SystemConfig& config);

    /**
     * @brief Check if settings storage is available
     * @return bool True if storage is ready
     */
    bool isAvailable() const;

private:
    /**
     * @brief Calculate simple checksum
     * @param data Data buffer
     * @param size Data size
     * @return uint8_t Checksum value
     */
    uint8_t calculateChecksum(const void* data, size_t size);

    hal::IStorage* storage_;    ///< Storage HAL
    bool initialized_;          ///< Initialization flag
    bool firstBoot_;            ///< First boot flag
};

} // namespace features
} // namespace pxlcam

#endif // PXLCAM_SETTINGS_H
