/**
 * @file hwtest_log.cpp
 * @brief Hardware Test SD Logging implementation
 * 
 * @version 1.3.0-HWTEST
 * @date 2024
 */

#include "hwtest_log.h"

#if PXLCAM_HWTEST

#include "hwtest_overlay.h"
#include "logging.h"

#include <Arduino.h>
#include <SD_MMC.h>
#include <stdarg.h>

namespace pxlcam::hwtest {

namespace {

constexpr const char* kLogTag = "hwlog";

//==============================================================================
// State
//==============================================================================

LogConfig g_config;
File g_logFile;
bool g_active = false;
uint32_t g_lastFlushTime = 0;
char g_lineBuf[256];

//==============================================================================
// Helpers
//==============================================================================

const char* levelToStr(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "D";
        case LogLevel::INFO:  return "I";
        case LogLevel::WARN:  return "W";
        case LogLevel::ERROR: return "E";
        case LogLevel::EVENT: return "*";
        default:              return "?";
    }
}

void writeHeader() {
    g_logFile.println("================================================================================");
    g_logFile.println("PXLcam v1.3.0-HWTEST Diagnostic Log");
    g_logFile.printf("Boot Time: %lu ms\n", millis());
    g_logFile.println("================================================================================");
    g_logFile.println();
    g_logFile.flush();
}

void checkRotate() {
    if (!g_active) return;
    
    uint32_t size = g_logFile.size();
    if (size < g_config.maxFileSizeKb * 1024) return;
    
    // Close current log
    g_logFile.close();
    
    // Rotate: remove old backup, rename current to backup
    SD_MMC.remove("/PXL/hwtest.log.bak");
    SD_MMC.rename(g_config.logPath, "/PXL/hwtest.log.bak");
    
    // Open new log
    g_logFile = SD_MMC.open(g_config.logPath, FILE_WRITE);
    if (g_logFile) {
        writeHeader();
        PXLCAM_LOGI_TAG(kLogTag, "Log rotated, old size: %lu bytes", size);
    } else {
        g_active = false;
        PXLCAM_LOGE_TAG(kLogTag, "Failed to create new log after rotate");
    }
}

}  // anonymous namespace

//==============================================================================
// Public API
//==============================================================================

bool logInit(const LogConfig* config) {
    if (config) {
        g_config = *config;
    }
    
    // Ensure directory exists
    if (!SD_MMC.exists("/PXL")) {
        SD_MMC.mkdir("/PXL");
    }
    
    // Open log file for append
    g_logFile = SD_MMC.open(g_config.logPath, FILE_APPEND);
    if (!g_logFile) {
        PXLCAM_LOGE_TAG(kLogTag, "Failed to open log file: %s", g_config.logPath);
        return false;
    }
    
    g_active = true;
    g_lastFlushTime = millis();
    
    writeHeader();
    
    PXLCAM_LOGI_TAG(kLogTag, "Logging to %s", g_config.logPath);
    logEvent("BOOT", "HWTEST diagnostic mode started");
    
    return true;
}

void logShutdown() {
    if (!g_active) return;
    
    logEvent("SHUTDOWN", "HWTEST session ending");
    g_logFile.flush();
    g_logFile.close();
    g_active = false;
    
    PXLCAM_LOGI_TAG(kLogTag, "Log file closed");
}

void logMsg(LogLevel level, const char* tag, const char* fmt, ...) {
    if (!g_active) return;
    if (level < g_config.minLevel) return;
    
    // Format timestamp
    uint32_t ms = millis();
    uint32_t s = ms / 1000;
    uint32_t m = s / 60;
    uint32_t h = m / 60;
    
    // Build message
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_lineBuf, sizeof(g_lineBuf), fmt, args);
    va_end(args);
    
    // Write to file
    g_logFile.printf("[%02lu:%02lu:%02lu.%03lu] [%s] [%s] %s\n",
                     h % 24, m % 60, s % 60, ms % 1000,
                     levelToStr(level), tag, g_lineBuf);
    
    // Echo to serial if configured
    if (g_config.logToSerial) {
        Serial.printf("[HWLOG] [%s] [%s] %s\n", levelToStr(level), tag, g_lineBuf);
    }
}

void logMetrics() {
    if (!g_active) return;
    
    SystemMetrics m = getMetrics();
    
    g_logFile.println("--- METRICS SNAPSHOT ---");
    g_logFile.printf("  Memory: heap=%luK psram=%luK minHeap=%luK frag=%d%%\n",
                     m.freeHeap / 1024, m.freePsram / 1024, 
                     m.minFreeHeap / 1024, m.heapFragmentation);
    g_logFile.printf("  Performance: fps=%.1f avg=%.1f frames=%lu\n",
                     m.currentFps, m.avgFps, m.frameCount);
    g_logFile.printf("  Timing: capture=%lums filter=%lums save=%lums\n",
                     m.captureTimeMs, m.filterTimeMs, m.saveTimeMs);
    g_logFile.printf("  WiFi: active=%d clients=%d ip=%s frames=%lu\n",
                     m.wifiActive, m.wifiClients, m.wifiIp, m.wifiFramesSent);
    g_logFile.printf("  Storage: sd=%d total=%lluMB free=%lluMB files=%lu\n",
                     m.sdMounted, m.sdTotalMb, m.sdFreeMb, m.filesWritten);
    g_logFile.printf("  Timelapse: active=%d frames=%lu interval=%lums\n",
                     m.timelapseActive, m.timelapseFrames, m.timelapseIntervalMs);
    g_logFile.printf("  Uptime: %lu seconds\n", m.uptimeSeconds);
    g_logFile.println("------------------------");
}

void logEvent(const char* eventName, const char* detail) {
    if (!g_active) return;
    
    uint32_t ms = millis();
    
    if (detail) {
        g_logFile.printf("[%lu] EVENT: %s - %s\n", ms, eventName, detail);
    } else {
        g_logFile.printf("[%lu] EVENT: %s\n", ms, eventName);
    }
    
    if (g_config.logToSerial) {
        Serial.printf("[HWLOG] EVENT: %s %s\n", eventName, detail ? detail : "");
    }
}

void logFlush() {
    if (!g_active) return;
    g_logFile.flush();
    g_lastFlushTime = millis();
}

bool logIsActive() {
    return g_active;
}

uint32_t logGetSize() {
    if (!g_active) return 0;
    return g_logFile.size();
}

void logUpdate() {
    if (!g_active) return;
    
    uint32_t now = millis();
    
    // Auto-flush at interval
    if (now - g_lastFlushTime >= g_config.flushIntervalMs) {
        logFlush();
    }
    
    // Check if rotation needed
    checkRotate();
}

}  // namespace pxlcam::hwtest

#endif // PXLCAM_HWTEST
