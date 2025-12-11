#pragma once
/**
 * @file preview_buffer.h
 * @brief Double buffering system for preview pipeline
 * 
 * Lock-free producer/consumer pattern for smooth preview
 */

#include <stdint.h>
#include <stddef.h>
#include <atomic>

namespace pxlcam::preview {

/// Buffer state flags
struct BufferState {
    std::atomic<bool> ready{false};
    std::atomic<bool> inUse{false};
};

/// Double buffer manager for 64x64 grayscale preview
class DoubleBuffer {
public:
    static constexpr int kBufferSize = 64 * 64;
    static constexpr int kBitmapSize = (64 * 64 + 7) / 8;
    
    DoubleBuffer() = default;
    ~DoubleBuffer();
    
    /// Allocate buffers (PSRAM if available)
    /// @return true if allocation successful
    bool allocate();
    
    /// Free all buffers
    void deallocate();
    
    /// Check if buffers are allocated
    bool isAllocated() const { return buffers_[0] != nullptr; }
    
    /// Get write buffer (for producer/camera)
    /// @return Pointer to current write buffer
    uint8_t* getWriteBuffer();
    
    /// Mark write buffer as ready and swap
    void commitWrite();
    
    /// Get read buffer (for consumer/display)
    /// @return Pointer to current read buffer, or nullptr if none ready
    uint8_t* getReadBuffer();
    
    /// Release read buffer
    void releaseRead();
    
    /// Check if a buffer is ready for reading
    bool hasReadyBuffer() const;
    
    /// Get 1-bit bitmap buffer for dithered output
    uint8_t* getBitmapBuffer();
    
    /// Get buffer index for diagnostics
    int getWriteIndex() const { return writeIdx_; }
    int getReadIndex() const { return readIdx_; }
    
    /// Get allocation info
    bool isInPSRAM() const { return inPsram_; }
    size_t getTotalAllocation() const;

private:
    uint8_t* buffers_[2] = {nullptr, nullptr};
    uint8_t* bitmapBuffer_ = nullptr;
    BufferState state_[2];
    int writeIdx_ = 0;
    int readIdx_ = 1;
    bool inPsram_ = false;
};

/// Global double buffer instance
extern DoubleBuffer g_previewBuffer;

/// Initialize preview buffer system
/// @return true if successful
bool initBuffers();

/// Cleanup preview buffers
void cleanupBuffers();

}  // namespace pxlcam::preview
