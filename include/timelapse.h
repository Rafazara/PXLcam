/**
 * @file timelapse.h
 * @brief Timelapse Controller for PXLcam v1.3.0
 * 
 * Automated timelapse capture system with configurable intervals.
 * Supports both preview-based timing and RTC-based scheduling.
 * 
 * Features:
 * - Configurable capture interval (1 second to 24 hours)
 * - Frame counter and estimated completion time
 * - Power-saving mode between captures
 * - Stylized capture integration
 * - Progress display on OLED
 * 
 * @version 1.3.0
 * @date 2024
 * 
 * Feature Flag: PXLCAM_FEATURE_TIMELAPSE
 */

#ifndef PXLCAM_TIMELAPSE_H
#define PXLCAM_TIMELAPSE_H

#include <Arduino.h>
#include <stdint.h>

// Feature gate - disabled by default
#ifndef PXLCAM_FEATURE_TIMELAPSE
#define PXLCAM_FEATURE_TIMELAPSE 0
#endif

#if PXLCAM_FEATURE_TIMELAPSE

namespace pxlcam {

// =============================================================================
// Timelapse Configuration
// =============================================================================

/**
 * @brief Timelapse mode
 */
enum class TimelapseMode : uint8_t {
    INTERVAL,           ///< Fixed interval between captures
    SUNRISE_SUNSET,     ///< Capture during daylight (requires RTC)
    BURST,              ///< Rapid burst capture
    MOTION_TRIGGERED    ///< Capture on motion detection
};

/**
 * @brief Power mode between captures
 */
enum class TimelapsePowerMode : uint8_t {
    ACTIVE,             ///< Keep camera active (faster capture)
    LIGHT_SLEEP,        ///< Light sleep (moderate power savings)
    DEEP_SLEEP          ///< Deep sleep (maximum power savings, slower wake)
};

/**
 * @brief Timelapse configuration
 */
struct TimelapseConfig {
    TimelapseMode mode;             ///< Capture mode
    uint32_t intervalMs;            ///< Interval between captures (ms)
    uint32_t maxFrames;             ///< Maximum frames (0 = unlimited)
    uint32_t durationMs;            ///< Total duration (0 = unlimited)
    TimelapsePowerMode powerMode;   ///< Power mode between captures
    bool applyStyleFilter;          ///< Apply stylized capture
    bool showCountdown;             ///< Show countdown on display
    bool beepOnCapture;             ///< Beep when capturing
    
    // Defaults: 5 second interval, unlimited frames
    TimelapseConfig()
        : mode(TimelapseMode::INTERVAL)
        , intervalMs(5000)
        , maxFrames(0)
        , durationMs(0)
        , powerMode(TimelapsePowerMode::ACTIVE)
        , applyStyleFilter(true)
        , showCountdown(true)
        , beepOnCapture(false) {}
};

/**
 * @brief Timelapse status
 */
struct TimelapseStatus {
    bool running;               ///< Timelapse active
    bool paused;                ///< Currently paused
    uint32_t framesCaptured;    ///< Frames captured so far
    uint32_t framesRemaining;   ///< Frames remaining (if maxFrames set)
    uint32_t elapsedMs;         ///< Elapsed time since start
    uint32_t remainingMs;       ///< Estimated remaining time
    uint32_t nextCaptureMs;     ///< Time until next capture
    uint32_t lastCaptureMs;     ///< Time since last capture
    bool lastCaptureOk;         ///< Last capture succeeded
};

// =============================================================================
// Timelapse Controller Class
// =============================================================================

/**
 * @brief Timelapse Controller
 * 
 * Manages automated timelapse capture sequences.
 */
class TimelapseController {
public:
    /**
     * @brief Get singleton instance
     */
    static TimelapseController& instance();
    
    /**
     * @brief Initialize timelapse system
     * 
     * @return true on success
     */
    bool init();
    
    /**
     * @brief Set capture interval
     * 
     * @param ms Interval in milliseconds (minimum 1000)
     */
    void setInterval(uint32_t ms);
    
    /**
     * @brief Get current interval
     * 
     * @return Interval in milliseconds
     */
    uint32_t getInterval() const;
    
    /**
     * @brief Set maximum frames
     * 
     * @param frames Maximum frames (0 = unlimited)
     */
    void setMaxFrames(uint32_t frames);
    
    /**
     * @brief Set configuration
     * 
     * @param config Configuration structure
     */
    void setConfig(const TimelapseConfig& config);
    
    /**
     * @brief Get current configuration
     * 
     * @return Configuration structure
     */
    const TimelapseConfig& getConfig() const;
    
    /**
     * @brief Begin timelapse sequence
     * 
     * Starts the timelapse with current configuration.
     * First capture is immediate.
     * 
     * @return true on success
     */
    bool begin();
    
    /**
     * @brief Stop timelapse sequence
     */
    void stop();
    
    /**
     * @brief Pause timelapse
     */
    void pause();
    
    /**
     * @brief Resume paused timelapse
     */
    void resume();
    
    /**
     * @brief Check if timelapse is running
     * 
     * @return true if active
     */
    bool isRunning() const;
    
    /**
     * @brief Check if timelapse is paused
     * 
     * @return true if paused
     */
    bool isPaused() const;
    
    /**
     * @brief Process timelapse tick
     * 
     * Must be called regularly (e.g., in main loop).
     * Checks if it's time to capture and triggers capture if needed.
     */
    void tick();
    
    /**
     * @brief Check if capture should happen now
     * 
     * @return true if time to capture
     */
    bool shouldCapture() const;
    
    /**
     * @brief Notify that capture completed
     * 
     * Call this after capture is done to update timing.
     * 
     * @param success true if capture succeeded
     */
    void onCaptureComplete(bool success);
    
    /**
     * @brief Get current status
     * 
     * @return Status structure
     */
    TimelapseStatus getStatus() const;
    
    /**
     * @brief Get frames captured
     * 
     * @return Number of frames
     */
    uint32_t getFramesCaptured() const;
    
    /**
     * @brief Get time until next capture
     * 
     * @return Milliseconds until next capture
     */
    uint32_t getTimeToNextCapture() const;
    
    /**
     * @brief Get progress percentage
     * 
     * @return Progress 0-100 (or 0 if unlimited)
     */
    uint8_t getProgress() const;
    
    /**
     * @brief Register capture callback
     * 
     * Called when it's time to capture a frame.
     * 
     * @param callback Function to call for capture
     */
    using CaptureCallback = void(*)(void);
    void onCapture(CaptureCallback callback);
    
private:
    TimelapseController();
    ~TimelapseController();
    
    // Non-copyable
    TimelapseController(const TimelapseController&) = delete;
    TimelapseController& operator=(const TimelapseController&) = delete;
    
    // Private implementation
    TimelapseConfig m_config;
    TimelapseStatus m_status;
    uint32_t m_startTime;
    uint32_t m_lastCaptureTime;
    CaptureCallback m_captureCallback;
};

// =============================================================================
// Convenience Functions
// =============================================================================

/**
 * @brief Quick start timelapse with interval
 * 
 * @param intervalSec Interval in seconds
 * @param maxFrames Maximum frames (0 = unlimited)
 * @return true on success
 */
bool timelapse_start(uint32_t intervalSec, uint32_t maxFrames = 0);

/**
 * @brief Stop timelapse
 */
void timelapse_stop();

/**
 * @brief Check if timelapse is running
 * 
 * @return true if active
 */
bool timelapse_is_running();

/**
 * @brief Get frames captured
 * 
 * @return Number of frames
 */
uint32_t timelapse_get_frames();

// =============================================================================
// Preset Intervals
// =============================================================================

namespace TimelapsePresets {
    constexpr uint32_t FAST_1S     = 1000;      ///< 1 second
    constexpr uint32_t NORMAL_5S   = 5000;      ///< 5 seconds
    constexpr uint32_t SLOW_30S    = 30000;     ///< 30 seconds
    constexpr uint32_t MINUTE_1M   = 60000;     ///< 1 minute
    constexpr uint32_t MINUTE_5M   = 300000;    ///< 5 minutes
    constexpr uint32_t HOUR_1H     = 3600000;   ///< 1 hour
}

// =============================================================================
// TODOs for v1.3.0 Implementation
// =============================================================================

// TODO: Implement basic interval timelapse
// TODO: Add deep sleep support for long intervals
// TODO: Implement progress bar on OLED
// TODO: Add estimated completion time display
// TODO: Implement burst mode
// TODO: Add motion detection trigger
// TODO: Store timelapse progress in NVS for resume after power loss
// TODO: Add battery level monitoring and auto-stop

} // namespace pxlcam

#endif // PXLCAM_FEATURE_TIMELAPSE

#endif // PXLCAM_TIMELAPSE_H
