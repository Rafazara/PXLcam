#pragma once
/**
 * @file capture_pipeline.h
 * @brief Unified capture and post-processing pipeline for PXLcam v1.2.0
 * 
 * Handles the complete capture flow:
 * 1. Acquire frame from camera
 * 2. Apply mode-specific post-processing (dithering, night enhance)
 * 3. Encode/format for storage
 * 4. Return processed buffer ready for save
 */

#include <stdint.h>
#include <stddef.h>
#include <esp_camera.h>

#include "mode_manager.h"

namespace pxlcam::capture {

/// Capture result status
enum class CaptureResult : uint8_t {
    Success = 0,
    CameraError,
    ProcessingError,
    MemoryError,
    Cancelled
};

/// Processed image container
struct ProcessedImage {
    uint8_t* data;          ///< Image data buffer (caller must NOT free - managed internally)
    size_t length;          ///< Data length in bytes
    uint16_t width;         ///< Image width
    uint16_t height;        ///< Image height
    bool isProcessed;       ///< True if post-processing was applied
    const char* extension;  ///< File extension ("raw", "bmp", "jpg")
};

/// Initialize capture pipeline
/// @return true if initialization successful
bool init();

/// Check if pipeline is ready
bool isReady();

/// Capture and process a single frame using current mode
/// @param outImage Output image container
/// @return Capture result status
CaptureResult captureFrame(ProcessedImage& outImage);

/// Capture with specific mode override
/// @param mode Mode to use for this capture
/// @param outImage Output image container
/// @return Capture result status
CaptureResult captureWithMode(pxlcam::mode::CaptureMode mode, ProcessedImage& outImage);

/// Release internal buffers after save is complete
/// Must be called after each captureFrame() to free resources
void releaseFrame();

/// Apply post-processing to raw RGB buffer
/// @param rgb888 Input RGB888 buffer
/// @param width Image width
/// @param height Image height
/// @param mode Processing mode
/// @param outBuffer Output buffer (must be pre-allocated)
/// @param outLength Output data length
/// @return true if processing successful
bool postProcess(const uint8_t* rgb888, uint16_t width, uint16_t height,
                 pxlcam::mode::CaptureMode mode, uint8_t* outBuffer, size_t& outLength);

/// Get estimated output size for given dimensions and mode
/// @param width Image width
/// @param height Image height
/// @param mode Processing mode
/// @return Estimated buffer size needed
size_t estimateOutputSize(uint16_t width, uint16_t height, pxlcam::mode::CaptureMode mode);

/// Get last capture duration (ms)
uint32_t getLastCaptureDuration();

/// Get last processing duration (ms)
uint32_t getLastProcessDuration();

/// Get result message for status
const char* getResultMessage(CaptureResult result);

}  // namespace pxlcam::capture
