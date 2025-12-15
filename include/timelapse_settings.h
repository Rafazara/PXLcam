/**
 * @file timelapse_settings.h
 * @brief Timelapse settings and NVS persistence for PXLcam v1.3.0
 * 
 * Provides:
 * - TimelapseInterval enum for preset intervals
 * - MaxFrames options
 * - NVS persistence for timelapse settings
 * - Name/value conversion functions
 * 
 * @version 1.3.0
 * @date 2024
 */

#ifndef PXLCAM_TIMELAPSE_SETTINGS_H
#define PXLCAM_TIMELAPSE_SETTINGS_H

#include <stdint.h>
#include "pxlcam_config.h"

#if PXLCAM_FEATURE_TIMELAPSE

namespace pxlcam::timelapse {

// =============================================================================
// Interval Presets
// =============================================================================

/**
 * @brief Timelapse interval presets
 */
enum class TimelapseInterval : uint8_t {
    FAST_1S = 0,        ///< 1 second
    NORMAL_5S = 1,      ///< 5 seconds (default)
    SLOW_30S = 2,       ///< 30 seconds
    MINUTE_1M = 3,      ///< 1 minute
    MINUTE_5M = 4,      ///< 5 minutes
    COUNT = 5           ///< Number of presets
};

/**
 * @brief Get interval in milliseconds
 * @param interval Interval enum value
 * @return Milliseconds for interval
 */
uint32_t intervalToMs(TimelapseInterval interval);

/**
 * @brief Get human-readable name for interval
 * @param interval Interval enum value
 * @return Display name string
 */
const char* intervalName(TimelapseInterval interval);

/**
 * @brief Get next interval (wraps around)
 * @param current Current interval
 * @return Next interval in cycle
 */
TimelapseInterval nextInterval(TimelapseInterval current);

/**
 * @brief Get previous interval (wraps around)
 * @param current Current interval
 * @return Previous interval in cycle
 */
TimelapseInterval prevInterval(TimelapseInterval current);

// =============================================================================
// Max Frames Options
// =============================================================================

/**
 * @brief Max frames presets
 */
enum class MaxFramesOption : uint8_t {
    FRAMES_10 = 0,      ///< 10 frames
    FRAMES_25 = 1,      ///< 25 frames
    FRAMES_50 = 2,      ///< 50 frames
    FRAMES_100 = 3,     ///< 100 frames
    UNLIMITED = 4,      ///< No limit (∞)
    COUNT = 5           ///< Number of options
};

/**
 * @brief Get frame count value
 * @param option MaxFrames option
 * @return Number of frames (0 = unlimited)
 */
uint32_t maxFramesToValue(MaxFramesOption option);

/**
 * @brief Get human-readable name for max frames
 * @param option MaxFrames option
 * @return Display name string
 */
const char* maxFramesName(MaxFramesOption option);

/**
 * @brief Get next max frames option (wraps around)
 * @param current Current option
 * @return Next option in cycle
 */
MaxFramesOption nextMaxFrames(MaxFramesOption current);

/**
 * @brief Get previous max frames option (wraps around)
 * @param current Current option
 * @return Previous option in cycle
 */
MaxFramesOption prevMaxFrames(MaxFramesOption current);

// =============================================================================
// NVS Persistence
// =============================================================================

/// NVS keys for timelapse settings
namespace keys {
    constexpr const char* kInterval = "tl_interval";
    constexpr const char* kMaxFrames = "tl_maxframes";
    constexpr const char* kEnabled = "tl_enabled";
}

/**
 * @brief Initialize timelapse settings from NVS
 * Loads saved interval and maxFrames preferences
 */
void settingsInit();

/**
 * @brief Save current interval to NVS
 * @param interval Interval to save
 * @return true if saved successfully
 */
bool saveInterval(TimelapseInterval interval);

/**
 * @brief Load interval from NVS
 * @return Saved interval or NORMAL_5S if not found
 */
TimelapseInterval loadInterval();

/**
 * @brief Save current max frames to NVS
 * @param option MaxFrames option to save
 * @return true if saved successfully
 */
bool saveMaxFrames(MaxFramesOption option);

/**
 * @brief Load max frames from NVS
 * @return Saved option or FRAMES_25 if not found
 */
MaxFramesOption loadMaxFrames();

/**
 * @brief Get current interval setting
 * @return Current TimelapseInterval
 */
TimelapseInterval getCurrentInterval();

/**
 * @brief Set current interval setting
 * @param interval New interval
 * @param persist Save to NVS if true
 */
void setCurrentInterval(TimelapseInterval interval, bool persist = true);

/**
 * @brief Get current max frames setting
 * @return Current MaxFramesOption
 */
MaxFramesOption getCurrentMaxFrames();

/**
 * @brief Set current max frames setting
 * @param option New max frames option
 * @param persist Save to NVS if true
 */
void setCurrentMaxFrames(MaxFramesOption option, bool persist = true);

/**
 * @brief Check if max frames limit has been reached
 * @param framesCaptured Current frame count
 * @return true if reached max (never true for UNLIMITED)
 */
bool hasReachedMaxFrames(uint32_t framesCaptured);

} // namespace pxlcam::timelapse

#endif // PXLCAM_FEATURE_TIMELAPSE

#endif // PXLCAM_TIMELAPSE_SETTINGS_H
