/**
 * @file hwtest_overlay.cpp
 * @brief Hardware Test Diagnostic Overlay implementation
 * 
 * @version 1.3.0-HWTEST
 * @date 2024
 */

#include "hwtest_overlay.h"

#if PXLCAM_HWTEST

#include "display.h"
#include "logging.h"

#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <esp_heap_caps.h>
#include <esp32-hal-psram.h>

#if PXLCAM_FEATURE_WIFI_PREVIEW
#include "wifi_preview.h"
#endif

#if PXLCAM_FEATURE_TIMELAPSE
#include "timelapse.h"
#endif

namespace pxlcam::hwtest {

namespace {

constexpr const char* kLogTag = "hwtest";

//==============================================================================
// State
//==============================================================================

DiagConfig g_config;
SystemMetrics g_metrics;
bool g_enabled = true;

// FPS calculation
uint32_t g_fpsFrameCount = 0;
uint32_t g_fpsLastCalcTime = 0;
float g_currentFps = 0.0f;
float g_fpsAccum = 0.0f;
uint32_t g_fpsSamples = 0;

// Timing
uint32_t g_lastUpdateTime = 0;
uint32_t g_startTime = 0;

//==============================================================================
// Metric Collection
//==============================================================================

void collectMemoryMetrics() {
    g_metrics.freeHeap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    g_metrics.minFreeHeap = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    
    if (psramFound()) {
        g_metrics.freePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    } else {
        g_metrics.freePsram = 0;
    }
    
    // Calculate fragmentation estimate
    uint32_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    if (g_metrics.freeHeap > 0 && largest > 0) {
        g_metrics.heapFragmentation = 100 - ((largest * 100) / g_metrics.freeHeap);
    } else {
        g_metrics.heapFragmentation = 0;
    }
}

void collectWifiMetrics() {
#if PXLCAM_FEATURE_WIFI_PREVIEW
    g_metrics.wifiActive = pxlcam::WifiPreview::instance().isActive();
    if (g_metrics.wifiActive) {
        auto status = pxlcam::WifiPreview::instance().getStatus();
        g_metrics.wifiClients = status.clientCount;
        g_metrics.wifiFramesSent = status.framesServed;
        strncpy(g_metrics.wifiIp, status.ipAddress, sizeof(g_metrics.wifiIp));
    } else {
        g_metrics.wifiClients = 0;
        g_metrics.wifiFramesSent = 0;
        strcpy(g_metrics.wifiIp, "N/A");
    }
#else
    g_metrics.wifiActive = false;
    strcpy(g_metrics.wifiIp, "disabled");
#endif
}

void collectTimelapseMetrics() {
#if PXLCAM_FEATURE_TIMELAPSE
    auto& tl = pxlcam::TimelapseController::instance();
    g_metrics.timelapseActive = tl.isRunning();
    g_metrics.timelapseFrames = tl.getFramesCaptured();
    g_metrics.timelapseIntervalMs = tl.getInterval();
#else
    g_metrics.timelapseActive = false;
    g_metrics.timelapseFrames = 0;
    g_metrics.timelapseIntervalMs = 0;
#endif
}

void collectSdMetrics() {
    // SD metrics are set externally via storage module
    // Just update uptime
    g_metrics.uptimeSeconds = (millis() - g_startTime) / 1000;
}

void calculateFps() {
    uint32_t now = millis();
    uint32_t elapsed = now - g_fpsLastCalcTime;
    
    // ==========================================================================
    // FIX: Prevent FPS overflow and divide-by-zero
    // ==========================================================================
    if (elapsed == 0) {
        return;  // Avoid division by zero
    }
    
    if (elapsed >= 1000) {
        // Sanity check: FPS should be reasonable (0-120)
        float rawFps = (g_fpsFrameCount * 1000.0f) / elapsed;
        g_currentFps = (rawFps > 0.0f && rawFps < 1000.0f) ? rawFps : 0.0f;
        
        g_fpsFrameCount = 0;
        g_fpsLastCalcTime = now;
        
        // Update average (with overflow protection)
        if (g_fpsSamples < 10000) {  // Prevent overflow after long runtime
            g_fpsAccum += g_currentFps;
            g_fpsSamples++;
            g_metrics.avgFps = (g_fpsSamples > 0) ? (g_fpsAccum / g_fpsSamples) : 0.0f;
        }
    }
    
    g_metrics.currentFps = g_currentFps;
}

}  // anonymous namespace

//==============================================================================
// Public API
//==============================================================================

void init(const DiagConfig* config) {
    if (config) {
        g_config = *config;
    }
    
    memset(&g_metrics, 0, sizeof(g_metrics));
    g_startTime = millis();
    g_fpsLastCalcTime = millis();
    g_enabled = true;
    
    PXLCAM_LOGI_TAG(kLogTag, "=== HWTEST DIAGNOSTIC MODE ENABLED ===");
    PXLCAM_LOGI_TAG(kLogTag, "Overlay update interval: %lu ms", g_config.updateIntervalMs);
}

void update() {
    if (!g_enabled) return;
    
    uint32_t now = millis();
    if (now - g_lastUpdateTime < g_config.updateIntervalMs) return;
    g_lastUpdateTime = now;
    
    collectMemoryMetrics();
    collectWifiMetrics();
    collectTimelapseMetrics();
    collectSdMetrics();
    calculateFps();
}

void drawOverlay() {
    if (!g_enabled) return;
    
    Adafruit_SSD1306* disp = pxlcam::display::getDisplayPtr();
    if (!disp) return;
    
    // Draw status bar at bottom (8 pixels high)
    int16_t y = 56;
    disp->fillRect(0, y, 128, 8, SSD1306_BLACK);
    disp->setTextSize(1);
    disp->setTextColor(SSD1306_WHITE);
    
    // Format: "H:32K P:2M F:12 W:* S:*"
    char buf[32];
    snprintf(buf, sizeof(buf), "H:%luK F:%.0f %s%s",
             g_metrics.freeHeap / 1024,
             g_metrics.currentFps,
             g_metrics.wifiActive ? "W" : "-",
             g_metrics.sdMounted ? "S" : "-");
    
    disp->setCursor(0, y);
    disp->print(buf);
    
    disp->display();
}

void drawFullDiagScreen() {
    Adafruit_SSD1306* disp = pxlcam::display::getDisplayPtr();
    if (!disp) return;
    
    disp->clearDisplay();
    disp->setTextSize(1);
    disp->setTextColor(SSD1306_WHITE);
    
    // Title
    disp->setCursor(15, 0);
    disp->print("HWTEST v1.3.0");
    disp->drawFastHLine(0, 9, 128, SSD1306_WHITE);
    
    // Memory
    disp->setCursor(0, 12);
    disp->printf("Heap: %luK/%luK", 
                 g_metrics.freeHeap / 1024,
                 g_metrics.minFreeHeap / 1024);
    
    disp->setCursor(0, 21);
    disp->printf("PSRAM: %luK Frag:%d%%",
                 g_metrics.freePsram / 1024,
                 g_metrics.heapFragmentation);
    
    // Performance
    disp->setCursor(0, 30);
    disp->printf("FPS: %.1f (avg:%.1f)",
                 g_metrics.currentFps, g_metrics.avgFps);
    
    // WiFi
    disp->setCursor(0, 39);
    if (g_metrics.wifiActive) {
        disp->printf("WiFi: %s (%d)", g_metrics.wifiIp, g_metrics.wifiClients);
    } else {
        disp->print("WiFi: OFF");
    }
    
    // Storage & Timelapse
    disp->setCursor(0, 48);
    disp->printf("SD:%s TL:%s(%lu)",
                 g_metrics.sdMounted ? "OK" : "NO",
                 g_metrics.timelapseActive ? "RUN" : "OFF",
                 g_metrics.timelapseFrames);
    
    // Uptime
    disp->setCursor(0, 57);
    disp->printf("Up:%lus Cap:%lu", 
                 g_metrics.uptimeSeconds,
                 g_metrics.frameCount);
    
    disp->display();
}

SystemMetrics getMetrics() {
    return g_metrics;
}

void recordCaptureTiming(uint32_t captureMs, uint32_t filterMs, uint32_t saveMs) {
    g_metrics.captureTimeMs = captureMs;
    g_metrics.filterTimeMs = filterMs;
    g_metrics.saveTimeMs = saveMs;
    g_metrics.frameCount++;
}

void tickFps() {
    g_fpsFrameCount++;
}

void logToSerial(const char* prefix) {
    PXLCAM_LOGI_TAG(kLogTag, "[%s] heap=%luK psram=%luK frag=%d%% fps=%.1f",
                    prefix,
                    g_metrics.freeHeap / 1024,
                    g_metrics.freePsram / 1024,
                    g_metrics.heapFragmentation,
                    g_metrics.currentFps);
    
    PXLCAM_LOGI_TAG(kLogTag, "[%s] wifi=%s clients=%d frames=%lu",
                    prefix,
                    g_metrics.wifiActive ? g_metrics.wifiIp : "OFF",
                    g_metrics.wifiClients,
                    g_metrics.wifiFramesSent);
    
    PXLCAM_LOGI_TAG(kLogTag, "[%s] sd=%s tl=%s(%lu) up=%lus cap=%lu",
                    prefix,
                    g_metrics.sdMounted ? "OK" : "NO",
                    g_metrics.timelapseActive ? "RUN" : "OFF",
                    g_metrics.timelapseFrames,
                    g_metrics.uptimeSeconds,
                    g_metrics.frameCount);
    
    PXLCAM_LOGI_TAG(kLogTag, "[%s] timing: cap=%lums flt=%lums save=%lums",
                    prefix,
                    g_metrics.captureTimeMs,
                    g_metrics.filterTimeMs,
                    g_metrics.saveTimeMs);
}

bool isEnabled() {
    return g_enabled;
}

void setEnabled(bool enable) {
    g_enabled = enable;
    PXLCAM_LOGI_TAG(kLogTag, "Diagnostic overlay %s", enable ? "enabled" : "disabled");
}

// External setters for metrics that come from other modules
void setSdStatus(bool mounted, uint64_t totalMb, uint64_t freeMb) {
    g_metrics.sdMounted = mounted;
    g_metrics.sdTotalMb = totalMb;
    g_metrics.sdFreeMb = freeMb;
}

void incrementFilesWritten() {
    g_metrics.filesWritten++;
}

}  // namespace pxlcam::hwtest

#endif // PXLCAM_HWTEST
