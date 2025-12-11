/**
 * @file preview_buffer.cpp
 * @brief Double buffering system for preview pipeline
 */

#include "preview_buffer.h"
#include "pxlcam_config.h"
#include "logging.h"

#include <esp_heap_caps.h>
#include <cstring>

namespace pxlcam::preview {

namespace {
constexpr const char* kLogTag = "pxlcam-buffer";
}

// Global double buffer instance
DoubleBuffer g_previewBuffer;

DoubleBuffer::~DoubleBuffer() {
    deallocate();
}

bool DoubleBuffer::allocate() {
    if (buffers_[0] != nullptr) {
        return true;  // Already allocated
    }
    
    PXLCAM_LOGI_TAG(kLogTag, "Allocating double buffer: %d bytes each", kBufferSize);
    
    // Try PSRAM first
    buffers_[0] = static_cast<uint8_t*>(heap_caps_malloc(kBufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    buffers_[1] = static_cast<uint8_t*>(heap_caps_malloc(kBufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    bitmapBuffer_ = static_cast<uint8_t*>(heap_caps_malloc(kBitmapSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    
    if (buffers_[0] && buffers_[1] && bitmapBuffer_) {
        inPsram_ = true;
        PXLCAM_LOGI_TAG(kLogTag, "Buffers allocated in PSRAM");
    } else {
        // Fallback to regular heap
        if (buffers_[0]) { heap_caps_free(buffers_[0]); buffers_[0] = nullptr; }
        if (buffers_[1]) { heap_caps_free(buffers_[1]); buffers_[1] = nullptr; }
        if (bitmapBuffer_) { heap_caps_free(bitmapBuffer_); bitmapBuffer_ = nullptr; }
        
        PXLCAM_LOGW_TAG(kLogTag, "PSRAM alloc failed, trying heap");
        buffers_[0] = static_cast<uint8_t*>(malloc(kBufferSize));
        buffers_[1] = static_cast<uint8_t*>(malloc(kBufferSize));
        bitmapBuffer_ = static_cast<uint8_t*>(malloc(kBitmapSize));
        inPsram_ = false;
    }
    
    if (!buffers_[0] || !buffers_[1] || !bitmapBuffer_) {
        PXLCAM_LOGE_TAG(kLogTag, "Failed to allocate preview buffers");
        deallocate();
        return false;
    }
    
    // Clear buffers
    memset(buffers_[0], 0, kBufferSize);
    memset(buffers_[1], 0, kBufferSize);
    memset(bitmapBuffer_, 0, kBitmapSize);
    
    // Reset state
    writeIdx_ = 0;
    readIdx_ = 1;
    state_[0].ready.store(false);
    state_[0].inUse.store(false);
    state_[1].ready.store(false);
    state_[1].inUse.store(false);
    
    PXLCAM_LOGI_TAG(kLogTag, "Double buffer initialized, PSRAM=%d", inPsram_);
    return true;
}

void DoubleBuffer::deallocate() {
    if (buffers_[0]) {
        if (inPsram_) heap_caps_free(buffers_[0]);
        else free(buffers_[0]);
        buffers_[0] = nullptr;
    }
    if (buffers_[1]) {
        if (inPsram_) heap_caps_free(buffers_[1]);
        else free(buffers_[1]);
        buffers_[1] = nullptr;
    }
    if (bitmapBuffer_) {
        if (inPsram_) heap_caps_free(bitmapBuffer_);
        else free(bitmapBuffer_);
        bitmapBuffer_ = nullptr;
    }
}

uint8_t* DoubleBuffer::getWriteBuffer() {
    return buffers_[writeIdx_];
}

void DoubleBuffer::commitWrite() {
    // Mark current write buffer as ready
    state_[writeIdx_].ready.store(true, std::memory_order_release);
    
    // Swap indices
    writeIdx_ = 1 - writeIdx_;
}

uint8_t* DoubleBuffer::getReadBuffer() {
    int idx = 1 - writeIdx_;  // Read from the other buffer
    
    if (!state_[idx].ready.load(std::memory_order_acquire)) {
        return nullptr;  // No ready buffer
    }
    
    state_[idx].inUse.store(true, std::memory_order_release);
    readIdx_ = idx;
    return buffers_[idx];
}

void DoubleBuffer::releaseRead() {
    state_[readIdx_].inUse.store(false, std::memory_order_release);
    state_[readIdx_].ready.store(false, std::memory_order_release);
}

bool DoubleBuffer::hasReadyBuffer() const {
    int idx = 1 - writeIdx_;
    return state_[idx].ready.load(std::memory_order_acquire);
}

uint8_t* DoubleBuffer::getBitmapBuffer() {
    return bitmapBuffer_;
}

size_t DoubleBuffer::getTotalAllocation() const {
    if (!isAllocated()) return 0;
    return kBufferSize * 2 + kBitmapSize;
}

bool initBuffers() {
    return g_previewBuffer.allocate();
}

void cleanupBuffers() {
    g_previewBuffer.deallocate();
}

}  // namespace pxlcam::preview
