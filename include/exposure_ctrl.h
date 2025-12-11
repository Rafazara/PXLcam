#pragma once
/**
 * @file exposure_ctrl.h
 * @brief Automatic exposure quick-tuning for OV2640 camera
 * 
 * Non-blocking exposure adjustment at boot and during preview
 */

#include <stdint.h>
#include <esp_camera.h>

namespace pxlcam::exposure {

/// Exposure tuning state
enum class TuneState : uint8_t {
    Idle = 0,
    Sampling = 1,
    Adjusting = 2,
    Complete = 3
};

/// Exposure configuration
struct ExposureConfig {
    bool autoExposure = true;          ///< Enable AEC
    bool autoGain = true;              ///< Enable AGC
    int8_t exposureLevel = 0;          ///< Manual exposure level (-2 to +2)
    uint8_t targetBrightness = 128;    ///< Target average brightness
    uint8_t tolerance = 20;            ///< Acceptable deviation from target
    uint16_t sampleIntervalMs = 500;   ///< Time between samples
    uint8_t maxIterations = 10;        ///< Max tuning iterations
};

/// Initialize exposure control
/// @param config Initial configuration
void init(const ExposureConfig& config = ExposureConfig{});

/// Start non-blocking auto-tune sequence
void startAutoTune();

/// Cancel current auto-tune
void cancelAutoTune();

/// Process one tick of auto-tune (call in main loop)
/// @return true if tuning is complete or idle
bool tick();

/// Get current tuning state
TuneState getState();

/// Check if tuning is in progress
bool isTuning();

/// Set manual exposure level (-2 to +2)
/// @param level Exposure compensation level
void setExposureLevel(int8_t level);

/// Enable/disable auto exposure
void setAutoExposure(bool enable);

/// Enable/disable auto gain
void setAutoGain(bool enable);

/// Get current average brightness from last sample
uint8_t getLastBrightness();

/// Calculate average brightness of a frame
/// @param fb Frame buffer
/// @return Average brightness (0-255)
uint8_t calculateBrightness(const camera_fb_t* fb);

/// Quick one-shot exposure adjustment based on frame
/// @param fb Frame buffer to analyze
void quickAdjust(const camera_fb_t* fb);

/// Apply night mode exposure settings
void applyNightMode();

/// Apply standard exposure settings
void applyStandardMode();

}  // namespace pxlcam::exposure
