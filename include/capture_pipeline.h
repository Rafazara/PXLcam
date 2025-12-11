#pragma once
/**
 * @file capture_pipeline.h
 * @brief Unified capture and post-processing pipeline for PXLcam v1.2.0
 * 
 * Handles the complete capture flow:
 * 1. Acquire frame from camera
 * 2. Apply mode-specific post-processing:
 *    - GAMEBOY: RGB→Grayscale→Bayer 8x8 dithering (4 tons)
 *    - NIGHT: RGB→Grayscale→Gamma boost + contrast
 *    - NORMAL: RGB→Grayscale (neutral)
 * 3. Encode as BMP for easy viewing
 * 4. Return processed buffer ready for save
 * 
 * All heavy allocations use PSRAM when available.
 */

#include <stdint.h>
#include <stddef.h>
#include <esp_camera.h>

#include "mode_manager.h"

namespace pxlcam::capture {

//==============================================================================
// Types
//==============================================================================

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
    const char* extension;  ///< File extension ("bmp", "raw", "jpg")
};

//==============================================================================
// Core API
//==============================================================================

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

//==============================================================================
// Filter API
//==============================================================================

/// Apply filter to RGB888 buffer
/// @param rgb Input RGB888 buffer
/// @param w Width
/// @param h Height
/// @param mode Processing mode
/// @param outGray Output grayscale buffer (must be w*h bytes)
/// @return true if successful
bool applyFilter(const uint8_t* rgb, int w, int h, 
                 pxlcam::mode::CaptureMode mode, uint8_t* outGray);

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

//==============================================================================
// BMP Encoding
//==============================================================================

/// Encode grayscale buffer as 8-bit BMP
/// @param gray Grayscale buffer (w*h bytes)
/// @param w Width
/// @param h Height
/// @param outBmp Output BMP buffer (must be at least getBmpSize(w,h))
/// @param outLength Actual BMP size written
/// @return true if successful
bool encodeGrayscaleBmp(const uint8_t* gray, int w, int h, 
                        uint8_t* outBmp, size_t& outLength);

/// Get required buffer size for BMP
/// @param w Width
/// @param h Height
/// @return Required buffer size in bytes
size_t getBmpSize(int w, int h);

//==============================================================================
// Utilities
//==============================================================================

/// Get estimated output size for given dimensions and mode
size_t estimateOutputSize(uint16_t width, uint16_t height, pxlcam::mode::CaptureMode mode);

/// Get last capture duration (ms)
uint32_t getLastCaptureDuration();

/// Get last processing duration (ms)
uint32_t getLastProcessDuration();

/// Get result message for status
const char* getResultMessage(CaptureResult result);

//==============================================================================
// Debug / Histogram
//==============================================================================

/// Log histogram of grayscale buffer (debug)
/// @param gray Grayscale buffer
/// @param length Buffer length
void logHistogram(const uint8_t* gray, size_t length);

/// Log sample tones from buffer (debug)
/// @param gray Grayscale buffer  
/// @param w Width
/// @param h Height
void logSampleTones(const uint8_t* gray, int w, int h);

}  // namespace pxlcam::capture
