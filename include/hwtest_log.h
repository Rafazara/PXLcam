/**
 * @file hwtest_log.h
 * @brief Hardware Test SD Logging for PXLcam v1.3.0
 * 
 * Logs diagnostic data to /PXL/hwtest.log on SD card:
 * - System metrics at intervals
 * - Event markers (boot, capture, wifi, etc.)
 * - Error conditions
 * 
 * @version 1.3.0-HWTEST
 * @date 2024
 */

#pragma once

#include "pxlcam_config.h"
#include <stdint.h>

#ifndef PXLCAM_HWTEST
#define PXLCAM_HWTEST 0
#endif

#if PXLCAM_HWTEST

namespace pxlcam::hwtest {

//==============================================================================
// Log Level
//==============================================================================

enum class LogLevel : uint8_t {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3,
    EVENT = 4
};

//==============================================================================
// Log Configuration
//==============================================================================

struct LogConfig {
    const char* logPath = "/PXL/hwtest.log";  ///< Log file path on SD
    uint32_t flushIntervalMs = 5000;          ///< Auto-flush interval
    uint32_t maxFileSizeKb = 1024;            ///< Max log size before rotate
    LogLevel minLevel = LogLevel::INFO;       ///< Minimum level to log
    bool logToSerial = true;                  ///< Also echo to serial
};

//==============================================================================
// Public API
//==============================================================================

/**
 * @brief Initialize logging system
 * @param config Configuration options
 * @return true if log file opened successfully
 */
bool logInit(const LogConfig* config = nullptr);

/**
 * @brief Shut down logging system
 * 
 * Flushes and closes log file
 */
void logShutdown();

/**
 * @brief Log a formatted message
 * @param level Log level
 * @param tag Module tag
 * @param fmt Printf-style format string
 * @param ... Format arguments
 */
void logMsg(LogLevel level, const char* tag, const char* fmt, ...);

/**
 * @brief Log system metrics snapshot
 * 
 * Writes current memory, FPS, WiFi, SD, timelapse stats
 */
void logMetrics();

/**
 * @brief Log an event marker
 * @param eventName Name of event
 * @param detail Optional detail string
 */
void logEvent(const char* eventName, const char* detail = nullptr);

/**
 * @brief Flush log buffer to SD
 */
void logFlush();

/**
 * @brief Check if logging is active
 * @return true if log file open
 */
bool logIsActive();

/**
 * @brief Get log file size
 * @return File size in bytes
 */
uint32_t logGetSize();

/**
 * @brief Periodic update (for auto-flush)
 * 
 * Call from main tick loop
 */
void logUpdate();

//==============================================================================
// Convenience Macros
//==============================================================================

#define HWTEST_LOGD(tag, fmt, ...) \
    pxlcam::hwtest::logMsg(pxlcam::hwtest::LogLevel::DEBUG, tag, fmt, ##__VA_ARGS__)

#define HWTEST_LOGI(tag, fmt, ...) \
    pxlcam::hwtest::logMsg(pxlcam::hwtest::LogLevel::INFO, tag, fmt, ##__VA_ARGS__)

#define HWTEST_LOGW(tag, fmt, ...) \
    pxlcam::hwtest::logMsg(pxlcam::hwtest::LogLevel::WARN, tag, fmt, ##__VA_ARGS__)

#define HWTEST_LOGE(tag, fmt, ...) \
    pxlcam::hwtest::logMsg(pxlcam::hwtest::LogLevel::ERROR, tag, fmt, ##__VA_ARGS__)

#define HWTEST_EVENT(name, detail) \
    pxlcam::hwtest::logEvent(name, detail)

}  // namespace pxlcam::hwtest

#else  // !PXLCAM_HWTEST

// Stub macros when HWTEST disabled
#define HWTEST_LOGD(tag, fmt, ...) ((void)0)
#define HWTEST_LOGI(tag, fmt, ...) ((void)0)
#define HWTEST_LOGW(tag, fmt, ...) ((void)0)
#define HWTEST_LOGE(tag, fmt, ...) ((void)0)
#define HWTEST_EVENT(name, detail) ((void)0)

#endif // PXLCAM_HWTEST
