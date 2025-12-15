/**
 * @file timelapse_menu.h
 * @brief Timelapse menu interface for PXLcam v1.3.0
 * 
 * Provides modal submenu for timelapse control:
 * - Start/Stop timelapse
 * - Interval selection
 * - Max frames selection
 * - Progress display during capture
 * 
 * @version 1.3.0
 * @date 2024
 */

#ifndef PXLCAM_TIMELAPSE_MENU_H
#define PXLCAM_TIMELAPSE_MENU_H

#include <stddef.h>
#include <stdint.h>
#include "pxlcam_config.h"

#if PXLCAM_FEATURE_TIMELAPSE

namespace pxlcam::timelapse {

// =============================================================================
// Menu Result
// =============================================================================

/**
 * @brief Timelapse menu result
 */
enum class MenuResult : uint8_t {
    START = 0,          ///< Start timelapse
    STOP = 1,           ///< Stop timelapse
    INTERVAL = 2,       ///< Interval changed
    MAX_FRAMES = 3,     ///< Max frames changed
    BACK = 4,           ///< Return to main menu
    CANCELLED = 5       ///< Menu cancelled/timeout
};

// =============================================================================
// Public API
// =============================================================================

/**
 * @brief Initialize timelapse menu system
 */
void menuInit();

/**
 * @brief Show timelapse submenu (modal)
 * 
 * Displays the timelapse configuration menu:
 * - Start/Stop Timelapse
 * - Interval: [current]
 * - Max Frames: [current]
 * - Back
 * 
 * @return MenuResult indicating action taken
 */
MenuResult showMenu();

/**
 * @brief Show interval selection screen
 * Cycles through interval presets
 */
void showIntervalSelect();

/**
 * @brief Show max frames selection screen
 * Cycles through max frames options
 */
void showMaxFramesSelect();

/**
 * @brief Draw timelapse active screen on OLED
 * Shows: frame counter, progress bar, next capture countdown
 * Call this periodically during timelapse
 */
void drawActiveScreen();

/**
 * @brief Draw timelapse start confirmation
 * @param intervalMs Interval in milliseconds
 * @param maxFrames Max frames (0 = unlimited)
 */
void drawStartScreen(uint32_t intervalMs, uint32_t maxFrames);

/**
 * @brief Draw timelapse stopped summary
 * @param framesCaptured Number of frames captured
 */
void drawStoppedScreen(uint32_t framesCaptured);

/**
 * @brief Format time in human-readable form
 * @param ms Milliseconds
 * @param buf Output buffer
 * @param bufSize Buffer size
 * @return Pointer to buf
 */
char* formatTime(uint32_t ms, char* buf, size_t bufSize);

/**
 * @brief Check if timelapse menu is active
 * @return true if menu is showing
 */
bool isMenuActive();

} // namespace pxlcam::timelapse

#endif // PXLCAM_FEATURE_TIMELAPSE

#endif // PXLCAM_TIMELAPSE_MENU_H
