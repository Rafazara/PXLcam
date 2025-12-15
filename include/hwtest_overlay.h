/**
 * @file hwtest_overlay.h
 * @brief Hardware Test Diagnostic Overlay for PXLcam v1.3.0
 * 
 * Displays real-time system metrics on OLED:
 * - Free RAM (heap + PSRAM)
 * - Live FPS counter
 * - WiFi status & IP
 * - SD card status
 * - Timelapse status
 * 
 * @version 1.3.0-HWTEST
 * @date 2024
 */

#pragma once

#include <stdint.h>

#ifndef PXLCAM_HWTEST
#define PXLCAM_HWTEST 0
#endif

#if PXLCAM_HWTEST

namespace pxlcam::hwtest {

//==============================================================================
// Diagnostic Overlay Configuration
//==============================================================================

struct DiagConfig {
    bool showRam = true;          ///< Show RAM usage
    bool showFps = true;          ///< Show FPS counter
    bool showWifi = true;         ///< Show WiFi status
    bool showSd = true;           ///< Show SD status
    bool showTimelapse = true;    ///< Show Timelapse status
    uint32_t updateIntervalMs = 500;  ///< Overlay refresh rate
};

//==============================================================================
// System Metrics Structure
//==============================================================================

struct SystemMetrics {
    // Memory
    uint32_t freeHeap;
    uint32_t freePsram;
    uint32_t minFreeHeap;
    uint8_t heapFragmentation;
    
    // Performance
    float currentFps;
    float avgFps;
    uint32_t frameCount;
    uint32_t captureTimeMs;
    uint32_t filterTimeMs;
    uint32_t saveTimeMs;
    
    // WiFi
    bool wifiActive;
    uint8_t wifiClients;
    uint32_t wifiFramesSent;
    char wifiIp[16];
    
    // Storage
    bool sdMounted;
    uint64_t sdTotalMb;
    uint64_t sdFreeMb;
    uint32_t filesWritten;
    
    // Timelapse
    bool timelapseActive;
    uint32_t timelapseFrames;
    uint32_t timelapseIntervalMs;
    
    // Power
    float batteryVoltage;
    uint32_t uptimeSeconds;
};

//==============================================================================
// Public API
//==============================================================================

/**
 * @brief Initialize diagnostic overlay system
 * @param config Configuration options
 */
void init(const DiagConfig* config = nullptr);

/**
 * @brief Update metrics and refresh overlay
 * 
 * Call this periodically (e.g., in tick loop)
 */
void update();

/**
 * @brief Draw full diagnostic overlay on OLED
 * 
 * Draws compact status bar at bottom of screen
 */
void drawOverlay();

/**
 * @brief Draw full-screen diagnostic view
 * 
 * Shows detailed metrics (for dedicated diag screen)
 */
void drawFullDiagScreen();

/**
 * @brief Get current system metrics
 * @return Metrics structure
 */
SystemMetrics getMetrics();

/**
 * @brief Record a capture timing
 * @param captureMs Time for capture
 * @param filterMs Time for filter processing
 * @param saveMs Time for SD save
 */
void recordCaptureTiming(uint32_t captureMs, uint32_t filterMs, uint32_t saveMs);

/**
 * @brief Increment FPS counter (call once per frame)
 */
void tickFps();

/**
 * @brief Log metrics to serial with prefix
 * @param prefix Log prefix string
 */
void logToSerial(const char* prefix = "DIAG");

/**
 * @brief Check if overlay is enabled
 * @return true if diagnostic mode active
 */
bool isEnabled();

/**
 * @brief Enable/disable overlay
 * @param enable true to enable
 */
void setEnabled(bool enable);

/**
 * @brief Set SD card status (call from storage module)
 * @param mounted Whether SD is mounted
 * @param totalMb Total capacity in MB
 * @param freeMb Free space in MB
 */
void setSdStatus(bool mounted, uint64_t totalMb, uint64_t freeMb);

/**
 * @brief Increment files written counter
 */
void incrementFilesWritten();

}  // namespace pxlcam::hwtest

#endif // PXLCAM_HWTEST
