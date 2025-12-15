/**
 * @file timelapse_settings.cpp
 * @brief Timelapse settings and NVS persistence implementation
 * 
 * @version 1.3.0
 * @date 2024
 */

#include "timelapse_settings.h"

#if PXLCAM_FEATURE_TIMELAPSE

#include "nvs_store.h"
#include "logging.h"

namespace pxlcam::timelapse {

namespace {

constexpr const char* kLogTag = "tl_settings";

// Current settings (runtime state)
TimelapseInterval g_currentInterval = TimelapseInterval::NORMAL_5S;
MaxFramesOption g_currentMaxFrames = MaxFramesOption::FRAMES_25;
bool g_initialized = false;

// Interval values in milliseconds
constexpr uint32_t kIntervalMs[] = {
    1000,       // FAST_1S
    5000,       // NORMAL_5S
    30000,      // SLOW_30S
    60000,      // MINUTE_1M
    300000      // MINUTE_5M
};

// Interval display names
const char* const kIntervalNames[] = {
    "1 seg",
    "5 seg",
    "30 seg",
    "1 min",
    "5 min"
};

// Max frames values (0 = unlimited)
constexpr uint32_t kMaxFramesValues[] = {
    10,         // FRAMES_10
    25,         // FRAMES_25
    50,         // FRAMES_50
    100,        // FRAMES_100
    0           // UNLIMITED
};

// Max frames display names
const char* const kMaxFramesNames[] = {
    "10",
    "25",
    "50",
    "100",
    "Infinito"
};

} // anonymous namespace

// =============================================================================
// Interval Functions
// =============================================================================

uint32_t intervalToMs(TimelapseInterval interval) {
    uint8_t idx = static_cast<uint8_t>(interval);
    if (idx >= static_cast<uint8_t>(TimelapseInterval::COUNT)) {
        return kIntervalMs[1]; // Default to 5s
    }
    return kIntervalMs[idx];
}

const char* intervalName(TimelapseInterval interval) {
    uint8_t idx = static_cast<uint8_t>(interval);
    if (idx >= static_cast<uint8_t>(TimelapseInterval::COUNT)) {
        return kIntervalNames[1]; // Default
    }
    return kIntervalNames[idx];
}

TimelapseInterval nextInterval(TimelapseInterval current) {
    uint8_t next = (static_cast<uint8_t>(current) + 1) % 
                   static_cast<uint8_t>(TimelapseInterval::COUNT);
    return static_cast<TimelapseInterval>(next);
}

TimelapseInterval prevInterval(TimelapseInterval current) {
    uint8_t curr = static_cast<uint8_t>(current);
    uint8_t count = static_cast<uint8_t>(TimelapseInterval::COUNT);
    uint8_t prev = (curr + count - 1) % count;
    return static_cast<TimelapseInterval>(prev);
}

// =============================================================================
// Max Frames Functions
// =============================================================================

uint32_t maxFramesToValue(MaxFramesOption option) {
    uint8_t idx = static_cast<uint8_t>(option);
    if (idx >= static_cast<uint8_t>(MaxFramesOption::COUNT)) {
        return kMaxFramesValues[1]; // Default to 25
    }
    return kMaxFramesValues[idx];
}

const char* maxFramesName(MaxFramesOption option) {
    uint8_t idx = static_cast<uint8_t>(option);
    if (idx >= static_cast<uint8_t>(MaxFramesOption::COUNT)) {
        return kMaxFramesNames[1]; // Default
    }
    return kMaxFramesNames[idx];
}

MaxFramesOption nextMaxFrames(MaxFramesOption current) {
    uint8_t next = (static_cast<uint8_t>(current) + 1) % 
                   static_cast<uint8_t>(MaxFramesOption::COUNT);
    return static_cast<MaxFramesOption>(next);
}

MaxFramesOption prevMaxFrames(MaxFramesOption current) {
    uint8_t curr = static_cast<uint8_t>(current);
    uint8_t count = static_cast<uint8_t>(MaxFramesOption::COUNT);
    uint8_t prev = (curr + count - 1) % count;
    return static_cast<MaxFramesOption>(prev);
}

// =============================================================================
// NVS Persistence
// =============================================================================

void settingsInit() {
    if (g_initialized) return;
    
    // Load saved settings
    g_currentInterval = loadInterval();
    g_currentMaxFrames = loadMaxFrames();
    g_initialized = true;
    
    PXLCAM_LOGI_TAG(kLogTag, "Settings loaded: interval=%s, maxFrames=%s",
                    intervalName(g_currentInterval),
                    maxFramesName(g_currentMaxFrames));
}

bool saveInterval(TimelapseInterval interval) {
    uint8_t value = static_cast<uint8_t>(interval);
    bool success = pxlcam::nvs::writeU8(keys::kInterval, value);
    
    if (success) {
        PXLCAM_LOGD_TAG(kLogTag, "Interval saved: %s", intervalName(interval));
    } else {
        PXLCAM_LOGE_TAG(kLogTag, "Failed to save interval");
    }
    
    return success;
}

TimelapseInterval loadInterval() {
    uint8_t value = pxlcam::nvs::readU8(keys::kInterval, 
                                         static_cast<uint8_t>(TimelapseInterval::NORMAL_5S));
    
    // Validate range
    if (value >= static_cast<uint8_t>(TimelapseInterval::COUNT)) {
        value = static_cast<uint8_t>(TimelapseInterval::NORMAL_5S);
    }
    
    return static_cast<TimelapseInterval>(value);
}

bool saveMaxFrames(MaxFramesOption option) {
    uint8_t value = static_cast<uint8_t>(option);
    bool success = pxlcam::nvs::writeU8(keys::kMaxFrames, value);
    
    if (success) {
        PXLCAM_LOGD_TAG(kLogTag, "MaxFrames saved: %s", maxFramesName(option));
    } else {
        PXLCAM_LOGE_TAG(kLogTag, "Failed to save maxFrames");
    }
    
    return success;
}

MaxFramesOption loadMaxFrames() {
    uint8_t value = pxlcam::nvs::readU8(keys::kMaxFrames,
                                         static_cast<uint8_t>(MaxFramesOption::FRAMES_25));
    
    // Validate range
    if (value >= static_cast<uint8_t>(MaxFramesOption::COUNT)) {
        value = static_cast<uint8_t>(MaxFramesOption::FRAMES_25);
    }
    
    return static_cast<MaxFramesOption>(value);
}

TimelapseInterval getCurrentInterval() {
    return g_currentInterval;
}

void setCurrentInterval(TimelapseInterval interval, bool persist) {
    g_currentInterval = interval;
    
    if (persist) {
        saveInterval(interval);
    }
    
    PXLCAM_LOGI_TAG(kLogTag, "Interval set to: %s", intervalName(interval));
}

MaxFramesOption getCurrentMaxFrames() {
    return g_currentMaxFrames;
}

void setCurrentMaxFrames(MaxFramesOption option, bool persist) {
    g_currentMaxFrames = option;
    
    if (persist) {
        saveMaxFrames(option);
    }
    
    PXLCAM_LOGI_TAG(kLogTag, "MaxFrames set to: %s", maxFramesName(option));
}

bool hasReachedMaxFrames(uint32_t framesCaptured) {
    uint32_t maxVal = maxFramesToValue(g_currentMaxFrames);
    
    // Unlimited = never reached
    if (maxVal == 0) return false;
    
    return framesCaptured >= maxVal;
}

} // namespace pxlcam::timelapse

#endif // PXLCAM_FEATURE_TIMELAPSE
