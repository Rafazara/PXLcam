/**
 * @file capture_pipeline.cpp
 * @brief Unified capture and post-processing pipeline for PXLcam v1.2.0
 */

#include "capture_pipeline.h"
#include "mode_manager.h"
#include "preview_dither.h"
#include "exposure_ctrl.h"
#include "logging.h"
#include "pxlcam_config.h"

#include <Arduino.h>
#include <esp_camera.h>
#include <esp_heap_caps.h>
#include <img_converters.h>
#include <cstring>

namespace pxlcam::capture {

namespace {

constexpr const char* kLogTag = "pxlcam-capture";

// Internal state
bool g_initialized = false;
camera_fb_t* g_activeFrame = nullptr;
uint8_t* g_processedBuffer = nullptr;
size_t g_processedBufferSize = 0;

// Metrics
uint32_t g_lastCaptureDuration = 0;
uint32_t g_lastProcessDuration = 0;

// Maximum buffer size for processed output (QVGA RGB888)
constexpr size_t kMaxProcessedSize = 320 * 240 * 3;

// Allocate processing buffer in PSRAM if available
bool allocateProcessBuffer() {
    if (g_processedBuffer) {
        return true;  // Already allocated
    }
    
    // Try PSRAM first
    g_processedBuffer = static_cast<uint8_t*>(
        heap_caps_malloc(kMaxProcessedSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    
    if (g_processedBuffer) {
        PXLCAM_LOGI_TAG(kLogTag, "Process buffer allocated in PSRAM (%u bytes)", kMaxProcessedSize);
    } else {
        // Fallback to regular heap (may fail on large images)
        g_processedBuffer = static_cast<uint8_t*>(malloc(kMaxProcessedSize));
        if (g_processedBuffer) {
            PXLCAM_LOGW_TAG(kLogTag, "Process buffer allocated in heap (%u bytes)", kMaxProcessedSize);
        }
    }
    
    return g_processedBuffer != nullptr;
}

void freeProcessBuffer() {
    if (g_processedBuffer) {
        // Check if in PSRAM or heap
        if (heap_caps_get_allocated_size(g_processedBuffer) > 0) {
            heap_caps_free(g_processedBuffer);
        } else {
            free(g_processedBuffer);
        }
        g_processedBuffer = nullptr;
        g_processedBufferSize = 0;
    }
}

// Apply GameBoy-style processing to full image
bool applyGameBoyProcess(const uint8_t* rgb888, uint16_t width, uint16_t height,
                          uint8_t* outBuffer, size_t& outLength) {
    // Convert RGB888 to grayscale first
    const size_t pixelCount = width * height;
    
    // Use first part of output buffer as temp grayscale buffer
    uint8_t* grayBuf = outBuffer;
    
    for (size_t i = 0; i < pixelCount; ++i) {
        const uint8_t* p = rgb888 + i * 3;
        // Luminance: Y = 0.299R + 0.587G + 0.114B
        grayBuf[i] = (77 * p[0] + 150 * p[1] + 29 * p[2]) >> 8;
    }
    
    // Apply GameBoy dithering
    // Output is 1-bit packed, but we'll store as 8-bit for compatibility
    // Each pixel becomes 0x00 (black) or 0xFF (white)
    
    // Initialize dither module if needed
    if (!pxlcam::dither::isInitialized()) {
        pxlcam::dither::initDitherModule(true);
    }
    
    // Create temp 1-bit buffer
    const size_t bitmapSize = (pixelCount + 7) / 8;
    static uint8_t tempBitmap[320 * 240 / 8];  // Max QVGA
    
    if (bitmapSize > sizeof(tempBitmap)) {
        PXLCAM_LOGE_TAG(kLogTag, "Image too large for dither buffer");
        return false;
    }
    
    // Apply dithering
    pxlcam::dither::applyGameBoyDither(grayBuf, width, height, tempBitmap);
    
    // Expand 1-bit to 8-bit grayscale for storage
    for (size_t i = 0; i < pixelCount; ++i) {
        const int byteIdx = i >> 3;
        const int bitIdx = 7 - (i & 7);
        const bool bit = (tempBitmap[byteIdx] >> bitIdx) & 1;
        outBuffer[i] = bit ? 0xFF : 0x00;
    }
    
    outLength = pixelCount;  // 1 byte per pixel grayscale
    return true;
}

// Apply night vision enhancement to full image
bool applyNightProcess(const uint8_t* rgb888, uint16_t width, uint16_t height,
                        uint8_t* outBuffer, size_t& outLength) {
    const size_t pixelCount = width * height;
    
    // Convert to grayscale with gamma boost
    for (size_t i = 0; i < pixelCount; ++i) {
        const uint8_t* p = rgb888 + i * 3;
        uint8_t gray = (77 * p[0] + 150 * p[1] + 29 * p[2]) >> 8;
        outBuffer[i] = gray;
    }
    
    // Apply night vision enhancement in-place
    pxlcam::dither::applyNightVision(outBuffer, width, height, 0.6f, 1.4f);
    
    // Apply threshold dithering for 1-bit output
    const size_t bitmapSize = (pixelCount + 7) / 8;
    static uint8_t tempBitmap[320 * 240 / 8];
    
    if (bitmapSize > sizeof(tempBitmap)) {
        PXLCAM_LOGE_TAG(kLogTag, "Image too large for night buffer");
        return false;
    }
    
    // Lower threshold for night mode (100 instead of 128)
    pxlcam::dither::convertTo1bitThreshold(outBuffer, width, height, tempBitmap, 100);
    
    // Expand to 8-bit
    for (size_t i = 0; i < pixelCount; ++i) {
        const int byteIdx = i >> 3;
        const int bitIdx = 7 - (i & 7);
        const bool bit = (tempBitmap[byteIdx] >> bitIdx) & 1;
        outBuffer[i] = bit ? 0xFF : 0x00;
    }
    
    outLength = pixelCount;
    return true;
}

}  // anonymous namespace

bool init() {
    if (g_initialized) {
        return true;
    }
    
    if (!allocateProcessBuffer()) {
        PXLCAM_LOGE_TAG(kLogTag, "Failed to allocate processing buffer");
        return false;
    }
    
    // Initialize dither module
    pxlcam::dither::initDitherModule(true);
    
    g_initialized = true;
    PXLCAM_LOGI_TAG(kLogTag, "Capture pipeline initialized");
    return true;
}

bool isReady() {
    return g_initialized && g_processedBuffer != nullptr;
}

CaptureResult captureFrame(ProcessedImage& outImage) {
    return captureWithMode(pxlcam::mode::getCurrentMode(), outImage);
}

CaptureResult captureWithMode(pxlcam::mode::CaptureMode mode, ProcessedImage& outImage) {
    // Clear output
    outImage.data = nullptr;
    outImage.length = 0;
    outImage.width = 0;
    outImage.height = 0;
    outImage.isProcessed = false;
    outImage.extension = "raw";
    
    if (!g_initialized) {
        if (!init()) {
            return CaptureResult::MemoryError;
        }
    }
    
    // Release any previous frame
    releaseFrame();
    
    // Apply night exposure settings if needed
    if (mode == pxlcam::mode::CaptureMode::Night) {
        pxlcam::exposure::applyNightMode();
    }
    
    // Capture frame
    uint32_t captureStart = millis();
    g_activeFrame = esp_camera_fb_get();
    g_lastCaptureDuration = millis() - captureStart;
    
    if (!g_activeFrame) {
        PXLCAM_LOGE_TAG(kLogTag, "Camera capture failed");
        return CaptureResult::CameraError;
    }
    
    PXLCAM_LOGI_TAG(kLogTag, "Captured frame: %dx%d, %u bytes, format=%d",
                    g_activeFrame->width, g_activeFrame->height,
                    g_activeFrame->len, g_activeFrame->format);
    
    // For Normal mode, return raw frame directly
    if (mode == pxlcam::mode::CaptureMode::Normal) {
        outImage.data = g_activeFrame->buf;
        outImage.length = g_activeFrame->len;
        outImage.width = g_activeFrame->width;
        outImage.height = g_activeFrame->height;
        outImage.isProcessed = false;
        outImage.extension = (g_activeFrame->format == PIXFORMAT_JPEG) ? "jpg" : "raw";
        g_lastProcessDuration = 0;
        return CaptureResult::Success;
    }
    
    // Need to convert to RGB888 for processing
    uint32_t processStart = millis();
    
    // Check if frame is JPEG and needs decoding
    uint8_t* rgb888Data = nullptr;
    size_t rgb888Size = g_activeFrame->width * g_activeFrame->height * 3;
    
    if (g_activeFrame->format == PIXFORMAT_JPEG) {
        // Decode JPEG to RGB888
        rgb888Data = g_processedBuffer;  // Use process buffer for decoded RGB
        
        if (!fmt2rgb888(g_activeFrame->buf, g_activeFrame->len, PIXFORMAT_JPEG, rgb888Data)) {
            PXLCAM_LOGE_TAG(kLogTag, "JPEG decode failed");
            return CaptureResult::ProcessingError;
        }
    } else if (g_activeFrame->format == PIXFORMAT_RGB888) {
        rgb888Data = g_activeFrame->buf;
    } else {
        PXLCAM_LOGE_TAG(kLogTag, "Unsupported pixel format: %d", g_activeFrame->format);
        return CaptureResult::ProcessingError;
    }
    
    // Apply mode-specific processing
    size_t processedLength = 0;
    bool processOk = false;
    
    switch (mode) {
        case pxlcam::mode::CaptureMode::GameBoy:
            processOk = applyGameBoyProcess(rgb888Data, g_activeFrame->width, g_activeFrame->height,
                                            g_processedBuffer, processedLength);
            break;
            
        case pxlcam::mode::CaptureMode::Night:
            processOk = applyNightProcess(rgb888Data, g_activeFrame->width, g_activeFrame->height,
                                          g_processedBuffer, processedLength);
            // Restore standard exposure
            pxlcam::exposure::applyStandardMode();
            break;
            
        default:
            // Shouldn't reach here, but copy raw for safety
            memcpy(g_processedBuffer, rgb888Data, rgb888Size);
            processedLength = rgb888Size;
            processOk = true;
            break;
    }
    
    g_lastProcessDuration = millis() - processStart;
    
    if (!processOk) {
        PXLCAM_LOGE_TAG(kLogTag, "Post-processing failed");
        return CaptureResult::ProcessingError;
    }
    
    // Set output
    outImage.data = g_processedBuffer;
    outImage.length = processedLength;
    outImage.width = g_activeFrame->width;
    outImage.height = g_activeFrame->height;
    outImage.isProcessed = true;
    outImage.extension = "raw";  // Grayscale raw output
    
    PXLCAM_LOGI_TAG(kLogTag, "Processing complete: capture=%ums, process=%ums",
                    g_lastCaptureDuration, g_lastProcessDuration);
    
    return CaptureResult::Success;
}

void releaseFrame() {
    if (g_activeFrame) {
        esp_camera_fb_return(g_activeFrame);
        g_activeFrame = nullptr;
    }
    g_processedBufferSize = 0;
}

bool postProcess(const uint8_t* rgb888, uint16_t width, uint16_t height,
                 pxlcam::mode::CaptureMode mode, uint8_t* outBuffer, size_t& outLength) {
    if (!rgb888 || !outBuffer || width == 0 || height == 0) {
        return false;
    }
    
    switch (mode) {
        case pxlcam::mode::CaptureMode::GameBoy:
            return applyGameBoyProcess(rgb888, width, height, outBuffer, outLength);
        case pxlcam::mode::CaptureMode::Night:
            return applyNightProcess(rgb888, width, height, outBuffer, outLength);
        case pxlcam::mode::CaptureMode::Normal:
        default:
            outLength = width * height * 3;
            memcpy(outBuffer, rgb888, outLength);
            return true;
    }
}

size_t estimateOutputSize(uint16_t width, uint16_t height, pxlcam::mode::CaptureMode mode) {
    const size_t pixelCount = width * height;
    
    switch (mode) {
        case pxlcam::mode::CaptureMode::GameBoy:
        case pxlcam::mode::CaptureMode::Night:
            return pixelCount;  // 8-bit grayscale
        case pxlcam::mode::CaptureMode::Normal:
        default:
            return pixelCount * 3;  // RGB888
    }
}

uint32_t getLastCaptureDuration() {
    return g_lastCaptureDuration;
}

uint32_t getLastProcessDuration() {
    return g_lastProcessDuration;
}

const char* getResultMessage(CaptureResult result) {
    switch (result) {
        case CaptureResult::Success:        return "Captura OK";
        case CaptureResult::CameraError:    return "Erro camera";
        case CaptureResult::ProcessingError: return "Erro processo";
        case CaptureResult::MemoryError:    return "Sem memoria";
        case CaptureResult::Cancelled:      return "Cancelado";
        default:                            return "Erro";
    }
}

}  // namespace pxlcam::capture
