/**
 * @file fps_counter.cpp
 * @brief FPS calculation utility for preview
 */

#include "fps_counter.h"
#include "logging.h"

#include <Arduino.h>

namespace pxlcam::util {

namespace {
constexpr const char* kLogTag = "pxlcam-fps";
}

void FPSCounter::tick() {
    uint32_t now = millis();
    
    if (lastTickMs_ != 0) {
        frameTimeMs_ = now - lastTickMs_;
        frameTimes_[frameIndex_] = frameTimeMs_;
        frameIndex_ = (frameIndex_ + 1) % kSampleCount;
        
        // Calculate average FPS from rolling window
        uint32_t totalMs = 0;
        for (int i = 0; i < kSampleCount; ++i) {
            totalMs += frameTimes_[i];
        }
        
        if (totalMs > 0) {
            currentFps_ = (1000 * kSampleCount) / totalMs;
        }
    }
    
    lastTickMs_ = now;
}

void FPSCounter::reset() {
    for (int i = 0; i < kSampleCount; ++i) {
        frameTimes_[i] = 0;
    }
    frameIndex_ = 0;
    lastTickMs_ = 0;
    currentFps_ = 0;
    frameTimeMs_ = 0;
}

}  // namespace pxlcam::util
