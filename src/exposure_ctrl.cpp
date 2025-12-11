/**
 * @file exposure_ctrl.cpp
 * @brief Automatic exposure quick-tuning for OV2640 camera
 */

#include "exposure_ctrl.h"
#include "pxlcam_config.h"
#include "logging.h"

#include <Arduino.h>
#include <esp_camera.h>

namespace pxlcam::exposure {

namespace {

constexpr const char* kLogTag = "pxlcam-exposure";

TuneState g_state = TuneState::Idle;
ExposureConfig g_config;
uint8_t g_lastBrightness = 128;
uint32_t g_lastSampleMs = 0;
uint8_t g_iterationCount = 0;

}  // anonymous namespace

void init(const ExposureConfig& config) {
    g_config = config;
    g_state = TuneState::Idle;
    g_lastBrightness = 128;
    g_lastSampleMs = 0;
    g_iterationCount = 0;
    
    sensor_t* sensor = esp_camera_sensor_get();
    if (sensor) {
        sensor->set_exposure_ctrl(sensor, g_config.autoExposure ? 1 : 0);
        sensor->set_gain_ctrl(sensor, g_config.autoGain ? 1 : 0);
        
        if (!g_config.autoExposure) {
            sensor->set_ae_level(sensor, g_config.exposureLevel);
        }
    }
    
    PXLCAM_LOGI_TAG(kLogTag, "Exposure control initialized, AEC=%d, AGC=%d", 
                    g_config.autoExposure, g_config.autoGain);
}

void startAutoTune() {
    g_state = TuneState::Sampling;
    g_iterationCount = 0;
    g_lastSampleMs = 0;
    PXLCAM_LOGI_TAG(kLogTag, "Auto-tune started");
}

void cancelAutoTune() {
    g_state = TuneState::Idle;
    PXLCAM_LOGI_TAG(kLogTag, "Auto-tune cancelled");
}

bool tick() {
    if (g_state == TuneState::Idle || g_state == TuneState::Complete) {
        return true;
    }
    
    uint32_t now = millis();
    if (now - g_lastSampleMs < g_config.sampleIntervalMs) {
        return false;  // Not time to sample yet
    }
    g_lastSampleMs = now;
    
    // Check iteration limit
    if (g_iterationCount >= g_config.maxIterations) {
        g_state = TuneState::Complete;
        PXLCAM_LOGI_TAG(kLogTag, "Auto-tune complete (max iterations)");
        return true;
    }
    
    // Get a frame to analyze
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        return false;
    }
    
    g_lastBrightness = calculateBrightness(fb);
    esp_camera_fb_return(fb);
    
    PXLCAM_LOGD_TAG(kLogTag, "Brightness sample: %d (target: %d)", 
                    g_lastBrightness, g_config.targetBrightness);
    
    // Check if within tolerance
    int diff = static_cast<int>(g_lastBrightness) - static_cast<int>(g_config.targetBrightness);
    if (diff < 0) diff = -diff;
    
    if (diff <= g_config.tolerance) {
        g_state = TuneState::Complete;
        PXLCAM_LOGI_TAG(kLogTag, "Auto-tune complete, brightness=%d", g_lastBrightness);
        return true;
    }
    
    // Need to adjust
    g_state = TuneState::Adjusting;
    ++g_iterationCount;
    
    sensor_t* sensor = esp_camera_sensor_get();
    if (!sensor) {
        return false;
    }
    
    // Determine adjustment direction
    int8_t adjust = (g_lastBrightness < g_config.targetBrightness) ? 1 : -1;
    
    // Get current AE level and adjust
    int8_t newLevel = g_config.exposureLevel + adjust;
    if (newLevel < -2) newLevel = -2;
    if (newLevel > 2) newLevel = 2;
    
    g_config.exposureLevel = newLevel;
    sensor->set_ae_level(sensor, newLevel);
    
    PXLCAM_LOGD_TAG(kLogTag, "Adjusted exposure level to %d", newLevel);
    
    g_state = TuneState::Sampling;
    return false;
}

TuneState getState() {
    return g_state;
}

bool isTuning() {
    return g_state == TuneState::Sampling || g_state == TuneState::Adjusting;
}

void setExposureLevel(int8_t level) {
    if (level < -2) level = -2;
    if (level > 2) level = 2;
    
    g_config.exposureLevel = level;
    
    sensor_t* sensor = esp_camera_sensor_get();
    if (sensor) {
        sensor->set_ae_level(sensor, level);
    }
}

void setAutoExposure(bool enable) {
    g_config.autoExposure = enable;
    
    sensor_t* sensor = esp_camera_sensor_get();
    if (sensor) {
        sensor->set_exposure_ctrl(sensor, enable ? 1 : 0);
    }
}

void setAutoGain(bool enable) {
    g_config.autoGain = enable;
    
    sensor_t* sensor = esp_camera_sensor_get();
    if (sensor) {
        sensor->set_gain_ctrl(sensor, enable ? 1 : 0);
    }
}

uint8_t getLastBrightness() {
    return g_lastBrightness;
}

uint8_t calculateBrightness(const camera_fb_t* fb) {
    if (!fb || !fb->buf || fb->len == 0) {
        return 128;
    }
    
    // Sample center region (for JPEG, use simple byte average as approximation)
    // For better accuracy with JPEG, we'd need to decode - this is a quick estimate
    uint32_t sum = 0;
    size_t samples = fb->len > 1024 ? 1024 : fb->len;
    size_t step = fb->len / samples;
    
    for (size_t i = 0; i < fb->len; i += step) {
        sum += fb->buf[i];
    }
    
    return static_cast<uint8_t>(sum / (fb->len / step));
}

void quickAdjust(const camera_fb_t* fb) {
    if (!fb) return;
    
    uint8_t brightness = calculateBrightness(fb);
    g_lastBrightness = brightness;
    
    int diff = static_cast<int>(brightness) - static_cast<int>(g_config.targetBrightness);
    if (diff < 0) diff = -diff;
    
    if (diff > g_config.tolerance) {
        int8_t adjust = (brightness < g_config.targetBrightness) ? 1 : -1;
        setExposureLevel(g_config.exposureLevel + adjust);
    }
}

void applyNightMode() {
    PXLCAM_LOGI_TAG(kLogTag, "Applying night mode");
    
    sensor_t* sensor = esp_camera_sensor_get();
    if (!sensor) return;
    
    // Maximize exposure for low light
    sensor->set_exposure_ctrl(sensor, 0);  // Manual exposure
    sensor->set_gain_ctrl(sensor, 0);      // Manual gain
    sensor->set_aec_value(sensor, 800);    // High exposure
    sensor->set_agc_gain(sensor, 20);      // High gain
    sensor->set_brightness(sensor, 1);     // Slight brightness boost
    sensor->set_contrast(sensor, 1);       // Slight contrast boost
}

void applyStandardMode() {
    PXLCAM_LOGI_TAG(kLogTag, "Applying standard mode");
    
    sensor_t* sensor = esp_camera_sensor_get();
    if (!sensor) return;
    
    // Re-enable auto exposure
    sensor->set_exposure_ctrl(sensor, g_config.autoExposure ? 1 : 0);
    sensor->set_gain_ctrl(sensor, g_config.autoGain ? 1 : 0);
    sensor->set_brightness(sensor, 0);
    sensor->set_contrast(sensor, 0);
    sensor->set_ae_level(sensor, g_config.exposureLevel);
}

}  // namespace pxlcam::exposure
