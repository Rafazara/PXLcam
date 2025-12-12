/**
 * @file timelapse.cpp
 * @brief Timelapse Controller implementation for PXLcam v1.3.0
 * 
 * @version 1.3.0
 * @date 2024
 */

#include "timelapse.h"

#if PXLCAM_FEATURE_TIMELAPSE

namespace pxlcam {

// =============================================================================
// Singleton
// =============================================================================

TimelapseController& TimelapseController::instance() {
    static TimelapseController instance;
    return instance;
}

// =============================================================================
// Constructor/Destructor
// =============================================================================

TimelapseController::TimelapseController()
    : m_startTime(0)
    , m_lastCaptureTime(0)
    , m_captureCallback(nullptr)
{
    m_config = TimelapseConfig();
    memset(&m_status, 0, sizeof(m_status));
}

TimelapseController::~TimelapseController() {
    stop();
}

// =============================================================================
// Public Methods
// =============================================================================

bool TimelapseController::init() {
    m_config = TimelapseConfig();
    memset(&m_status, 0, sizeof(m_status));
    return true;
}

void TimelapseController::setInterval(uint32_t ms) {
    // Minimum 1 second interval
    m_config.intervalMs = max(ms, (uint32_t)1000);
}

uint32_t TimelapseController::getInterval() const {
    return m_config.intervalMs;
}

void TimelapseController::setMaxFrames(uint32_t frames) {
    m_config.maxFrames = frames;
}

void TimelapseController::setConfig(const TimelapseConfig& config) {
    m_config = config;
    // Enforce minimum interval
    if (m_config.intervalMs < 1000) {
        m_config.intervalMs = 1000;
    }
}

const TimelapseConfig& TimelapseController::getConfig() const {
    return m_config;
}

bool TimelapseController::begin() {
    if (m_status.running) {
        stop();
    }
    
    m_startTime = millis();
    m_lastCaptureTime = 0;
    
    m_status.running = true;
    m_status.paused = false;
    m_status.framesCaptured = 0;
    m_status.elapsedMs = 0;
    m_status.lastCaptureOk = true;
    
    if (m_config.maxFrames > 0) {
        m_status.framesRemaining = m_config.maxFrames;
    } else {
        m_status.framesRemaining = 0xFFFFFFFF;
    }
    
    // First capture is immediate
    if (m_captureCallback) {
        m_captureCallback();
    }
    
    return true;
}

void TimelapseController::stop() {
    m_status.running = false;
    m_status.paused = false;
}

void TimelapseController::pause() {
    if (m_status.running) {
        m_status.paused = true;
    }
}

void TimelapseController::resume() {
    if (m_status.running && m_status.paused) {
        m_status.paused = false;
        // Adjust timing so next capture is relative to resume time
        m_lastCaptureTime = millis();
    }
}

bool TimelapseController::isRunning() const {
    return m_status.running;
}

bool TimelapseController::isPaused() const {
    return m_status.paused;
}

void TimelapseController::tick() {
    if (!m_status.running || m_status.paused) return;
    
    uint32_t now = millis();
    m_status.elapsedMs = now - m_startTime;
    
    // Check duration limit
    if (m_config.durationMs > 0 && m_status.elapsedMs >= m_config.durationMs) {
        stop();
        return;
    }
    
    // Check frame limit
    if (m_config.maxFrames > 0 && m_status.framesCaptured >= m_config.maxFrames) {
        stop();
        return;
    }
    
    // Calculate time to next capture
    uint32_t timeSinceLast = now - m_lastCaptureTime;
    if (timeSinceLast >= m_config.intervalMs) {
        m_status.nextCaptureMs = 0;
    } else {
        m_status.nextCaptureMs = m_config.intervalMs - timeSinceLast;
    }
    
    // Update remaining time estimate
    if (m_config.maxFrames > 0) {
        uint32_t remaining = m_config.maxFrames - m_status.framesCaptured;
        m_status.remainingMs = remaining * m_config.intervalMs;
    }
    
    // Check if should capture
    if (shouldCapture() && m_captureCallback) {
        m_captureCallback();
    }
}

bool TimelapseController::shouldCapture() const {
    if (!m_status.running || m_status.paused) return false;
    
    uint32_t now = millis();
    
    // First frame (lastCaptureTime == 0) was already captured in begin()
    if (m_lastCaptureTime == 0) return false;
    
    return (now - m_lastCaptureTime) >= m_config.intervalMs;
}

void TimelapseController::onCaptureComplete(bool success) {
    m_lastCaptureTime = millis();
    m_status.lastCaptureOk = success;
    
    if (success) {
        m_status.framesCaptured++;
        if (m_config.maxFrames > 0 && m_status.framesRemaining > 0) {
            m_status.framesRemaining--;
        }
    }
    
    m_status.lastCaptureMs = 0;
}

TimelapseStatus TimelapseController::getStatus() const {
    return m_status;
}

uint32_t TimelapseController::getFramesCaptured() const {
    return m_status.framesCaptured;
}

uint32_t TimelapseController::getTimeToNextCapture() const {
    return m_status.nextCaptureMs;
}

uint8_t TimelapseController::getProgress() const {
    if (m_config.maxFrames == 0) return 0;
    
    return (uint8_t)((m_status.framesCaptured * 100) / m_config.maxFrames);
}

void TimelapseController::onCapture(CaptureCallback callback) {
    m_captureCallback = callback;
}

// =============================================================================
// Convenience Functions
// =============================================================================

bool timelapse_start(uint32_t intervalSec, uint32_t maxFrames) {
    TimelapseConfig config;
    config.intervalMs = intervalSec * 1000;
    config.maxFrames = maxFrames;
    
    TimelapseController::instance().setConfig(config);
    return TimelapseController::instance().begin();
}

void timelapse_stop() {
    TimelapseController::instance().stop();
}

bool timelapse_is_running() {
    return TimelapseController::instance().isRunning();
}

uint32_t timelapse_get_frames() {
    return TimelapseController::instance().getFramesCaptured();
}

} // namespace pxlcam

#endif // PXLCAM_FEATURE_TIMELAPSE
