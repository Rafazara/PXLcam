/**
 * @file timelapse_power.cpp
 * @brief Power management implementation for timelapse mode
 * 
 * @version 1.3.0
 * @date 2024
 */

#include "timelapse_power.h"

#if PXLCAM_FEATURE_TIMELAPSE

#include "logging.h"

#include <Arduino.h>
#include <esp_sleep.h>
#include <esp_pm.h>

namespace pxlcam::timelapse {

namespace {

constexpr const char* kLogTag = "tl_power";

bool g_sleepEnabled = true;
bool g_wasTimerWakeup = false;

} // anonymous namespace

//==============================================================================
// Public API Implementation
//==============================================================================

void powerInit() {
    g_sleepEnabled = true;
    g_wasTimerWakeup = false;
    
    // Check if we just woke from sleep
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    
    if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
        g_wasTimerWakeup = true;
        PXLCAM_LOGI_TAG(kLogTag, "Woke from timer sleep");
    }
    
    PXLCAM_LOGI_TAG(kLogTag, "Power management initialized (sleep %s)",
                    g_sleepEnabled ? "enabled" : "disabled");
}

bool shouldUseSleep(uint32_t intervalMs) {
    if (!g_sleepEnabled) return false;
    return intervalMs >= kMinSleepIntervalMs;
}

void enterLightSleep(uint32_t sleepMs) {
    if (!g_sleepEnabled) {
        PXLCAM_LOGW_TAG(kLogTag, "Sleep disabled, skipping");
        return;
    }
    
    // Subtract wake margin for camera warmup
    if (sleepMs <= kWakeMarginMs) {
        PXLCAM_LOGD_TAG(kLogTag, "Sleep duration too short, skipping");
        return;
    }
    
    uint32_t actualSleepMs = sleepMs - kWakeMarginMs;
    uint64_t sleepUs = (uint64_t)actualSleepMs * 1000ULL;
    
    PXLCAM_LOGI_TAG(kLogTag, "Entering light sleep for %lu ms", actualSleepMs);
    
    // Configure timer wakeup
    esp_err_t err = esp_sleep_enable_timer_wakeup(sleepUs);
    if (err != ESP_OK) {
        PXLCAM_LOGE_TAG(kLogTag, "Failed to set timer wakeup: %d", err);
        return;
    }
    
    // Flush logs before sleep
    Serial.flush();
    delay(10);
    
    // Enter light sleep
    esp_light_sleep_start();
    
    // We wake up here
    g_wasTimerWakeup = (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER);
    
    PXLCAM_LOGI_TAG(kLogTag, "Woke from light sleep (timer: %s)",
                    g_wasTimerWakeup ? "yes" : "no");
}

void handleWakeup() {
    // Re-initialize peripherals if needed
    // For light sleep, most peripherals remain active
    // Camera may need brief warmup
    
    PXLCAM_LOGD_TAG(kLogTag, "Handling wakeup, reason: %s", getWakeupReason());
    
    // Small delay for peripheral stabilization
    delay(100);
}

bool wasTimerWakeup() {
    return g_wasTimerWakeup;
}

const char* getWakeupReason() {
    esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();
    
    switch (reason) {
        case ESP_SLEEP_WAKEUP_UNDEFINED:
            return "undefined";
        case ESP_SLEEP_WAKEUP_ALL:
            return "all";
        case ESP_SLEEP_WAKEUP_EXT0:
            return "ext0";
        case ESP_SLEEP_WAKEUP_EXT1:
            return "ext1";
        case ESP_SLEEP_WAKEUP_TIMER:
            return "timer";
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            return "touchpad";
        case ESP_SLEEP_WAKEUP_ULP:
            return "ulp";
        case ESP_SLEEP_WAKEUP_GPIO:
            return "gpio";
        case ESP_SLEEP_WAKEUP_UART:
            return "uart";
        default:
            return "unknown";
    }
}

void disableSleep() {
    g_sleepEnabled = false;
    PXLCAM_LOGI_TAG(kLogTag, "Sleep mode disabled");
}

void enableSleep() {
    g_sleepEnabled = true;
    PXLCAM_LOGI_TAG(kLogTag, "Sleep mode enabled");
}

bool isSleepEnabled() {
    return g_sleepEnabled;
}

} // namespace pxlcam::timelapse

#endif // PXLCAM_FEATURE_TIMELAPSE
