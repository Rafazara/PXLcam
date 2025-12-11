#pragma once
/**
 * @file fps_counter.h
 * @brief FPS calculation utility for preview
 */

#include <stdint.h>

namespace pxlcam::util {

/// Simple FPS counter with rolling average
class FPSCounter {
public:
    FPSCounter() = default;
    
    /// Record a frame timestamp
    void tick();
    
    /// Get current FPS (updated each tick)
    int getFPS() const { return currentFps_; }
    
    /// Get frame time in ms
    uint32_t getFrameTimeMs() const { return frameTimeMs_; }
    
    /// Reset counter
    void reset();
    
private:
    static constexpr int kSampleCount = 10;
    
    uint32_t frameTimes_[kSampleCount] = {0};
    int frameIndex_ = 0;
    uint32_t lastTickMs_ = 0;
    int currentFps_ = 0;
    uint32_t frameTimeMs_ = 0;
};

}  // namespace pxlcam::util
