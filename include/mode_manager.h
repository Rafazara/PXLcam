#pragma once
/**
 * @file mode_manager.h
 * @brief Centralized mode management for PXLcam v1.2.0
 * 
 * Manages capture modes (Normal, GameBoy, Night) with NVS persistence
 * and provides unified access for preview, capture, and UI modules.
 */

#include <stdint.h>

namespace pxlcam::mode {

/// Available capture/preview modes
enum class CaptureMode : uint8_t {
    Normal = 0,     ///< Standard capture, no post-processing
    GameBoy = 1,    ///< GameBoy-style 4-tone dithering
    Night = 2,      ///< Night vision with gamma boost
    COUNT           ///< Sentinel for iteration
};

/// Mode change callback signature
using ModeChangeCallback = void (*)(CaptureMode newMode);

/// Initialize mode manager and load saved mode from NVS
/// @return true if initialization successful
bool init();

/// Get current capture mode
CaptureMode getCurrentMode();

/// Set capture mode and persist to NVS
/// @param mode New capture mode
/// @param persist If true, save to NVS (default: true)
void setMode(CaptureMode mode, bool persist = true);

/// Cycle to next mode (Normal → GameBoy → Night → Normal)
/// @param persist If true, save to NVS
/// @return New mode after cycling
CaptureMode cycleMode(bool persist = true);

/// Get human-readable mode name
/// @param mode Mode to get name for
/// @return Mode name string (e.g., "Normal", "GameBoy", "Night")
const char* getModeName(CaptureMode mode);

/// Get short mode identifier (single character)
/// @param mode Mode to get identifier for
/// @return Single character: 'N', 'G', or 'X' (night)
char getModeChar(CaptureMode mode);

/// Register callback for mode changes
/// @param callback Function to call when mode changes
void setModeChangeCallback(ModeChangeCallback callback);

/// Check if current mode requires post-processing
bool requiresPostProcess();

/// Check if current mode uses night exposure settings
bool isNightMode();

/// Check if current mode uses dithering
bool usesDithering();

/// Reset to default mode (Normal) and clear NVS
void resetToDefault();

}  // namespace pxlcam::mode
