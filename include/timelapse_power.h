/**
 * @file timelapse_power.h
 * @brief Power management for timelapse mode
 * 
 * Provides light sleep support for long intervals to conserve power.
 * Uses ESP32 timer wakeup for scheduled captures.
 * 
 * @version 1.3.0
 * @date 2024
 */

#ifndef PXLCAM_TIMELAPSE_POWER_H
#define PXLCAM_TIMELAPSE_POWER_H

#include <stdint.h>
#include "pxlcam_config.h"

#if PXLCAM_FEATURE_TIMELAPSE

namespace pxlcam::timelapse {

// =============================================================================
// Power Management Constants
// =============================================================================

/// Minimum interval for light sleep (30 seconds)
constexpr uint32_t kMinSleepIntervalMs = 30000;

/// Wake margin before capture (2 seconds for camera warmup)
constexpr uint32_t kWakeMarginMs = 2000;

// =============================================================================
// Public API
// =============================================================================

/**
 * @brief Initialize power management
 */
void powerInit();

/**
 * @brief Check if sleep is appropriate for current interval
 * @param intervalMs Current timelapse interval
 * @return true if interval is long enough for sleep
 */
bool shouldUseSleep(uint32_t intervalMs);

/**
 * @brief Prepare and enter light sleep
 * 
 * Configures timer wakeup and enters light sleep.
 * Camera and peripherals remain powered but CPU sleeps.
 * 
 * @param sleepMs Duration to sleep (will subtract wake margin)
 */
void enterLightSleep(uint32_t sleepMs);

/**
 * @brief Handle wakeup from sleep
 * 
 * Called after waking from light sleep.
 * Re-initializes any peripherals that need it.
 */
void handleWakeup();

/**
 * @brief Check if we just woke from sleep
 * @return true if last wake was from timer
 */
bool wasTimerWakeup();

/**
 * @brief Get wakeup reason as string
 * @return Human-readable wakeup reason
 */
const char* getWakeupReason();

/**
 * @brief Disable sleep mode (for short intervals or debugging)
 */
void disableSleep();

/**
 * @brief Enable sleep mode
 */
void enableSleep();

/**
 * @brief Check if sleep mode is enabled
 * @return true if sleep is enabled
 */
bool isSleepEnabled();

} // namespace pxlcam::timelapse

#endif // PXLCAM_FEATURE_TIMELAPSE

#endif // PXLCAM_TIMELAPSE_POWER_H
