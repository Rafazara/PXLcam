/**
 * @file capture_pipeline.cpp
 * @brief Stylized Capture Pipeline Implementation for PXLcam v1.2.0
 * 
 * Implements complete capture workflow:
 * RGB → Dither → LUT → BMP → MockStorage
 * 
 * @author PXLcam Team
 * @version 1.2.0
 * @date 2024
 */

#include "capture_pipeline.h"
#include "../mocks/mock_storage.h"
#include "preview_dither.h"
#include <Arduino.h>
#include <cstring>
#include <cmath>

namespace pxlcam {
namespace features {
namespace capture {

// ============================================================================
// Static State
// ============================================================================

static CaptureStats s_lastStats;
static MiniPreview s_lastPreview;
static uint32_t s_captureCount = 0;

// Mock storage instance
static mocks::MockStorage s_mockStorage(512 * 1024);  // 512KB simulated storage

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Generate simulated RGB test pattern
 * 
 * Creates a gradient + circle pattern for testing without real camera.
 */
static void generateTestPattern(uint8_t* rgb, uint16_t width, uint16_t height) {
    int cx = width / 2;
    int cy = height / 2;
    int radius = min(width, height) / 3;
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 3;
            
            // Distance from center
            int dx = x - cx;
            int dy = y - cy;
            float dist = sqrt(dx * dx + dy * dy);
            
            if (dist < radius) {
                // Inside circle: lighter gradient
                uint8_t val = 200 - (uint8_t)((dist / radius) * 150);
                rgb[idx + 0] = val;     // R
                rgb[idx + 1] = val;     // G
                rgb[idx + 2] = val;     // B
            } else {
                // Outside: diagonal gradient
                uint8_t val = (uint8_t)(((x + y) * 255) / (width + height));
                rgb[idx + 0] = val;
                rgb[idx + 1] = val;
                rgb[idx + 2] = val;
            }
        }
    }
}

/**
 * @brief Convert RGB to grayscale using luminance formula
 */
static void rgbToGray(const uint8_t* rgb, uint8_t* gray, size_t pixelCount) {
    for (size_t i = 0; i < pixelCount; i++) {
        // ITU-R BT.601 luminance: Y = 0.299R + 0.587G + 0.114B
        uint8_t r = rgb[i * 3 + 0];
        uint8_t g = rgb[i * 3 + 1];
        uint8_t b = rgb[i * 3 + 2];
        gray[i] = (uint8_t)((r * 77 + g * 150 + b * 29) >> 8);
    }
}

/**
 * @brief Get palette colors for a given palette type
 */
static const uint8_t (*getPaletteColors(core::Palette palette))[3] {
    switch (palette) {
        case core::Palette::GAMEBOY:
            return GAMEBOY_PALETTE;
        case core::Palette::CGA:
            return CGA_PALETTE;
        default:
            return GAMEBOY_PALETTE;  // Default to GameBoy
    }
}

/**
 * @brief Quantize grayscale value to 4 levels (2-bit)
 */
static uint8_t quantize4Level(uint8_t gray) {
    if (gray < 64) return 3;       // Darkest
    if (gray < 128) return 2;      // Dark
    if (gray < 192) return 1;      // Light
    return 0;                       // Lightest
}

// ============================================================================
// Pipeline Implementation
// ============================================================================

CaptureResult runCapture(core::AppContext& ctx) {
    if (!ctx.isInitialized()) {
        Serial.println("[Capture] ERROR: AppContext not initialized");
        return CaptureResult::ERROR_INVALID_CTX;
    }
    
    Serial.println("[Capture] ========== Starting Capture Pipeline ==========");
    
    uint32_t pipelineStart = millis();
    s_lastStats.reset();
    s_lastStats.width = DEFAULT_WIDTH;
    s_lastStats.height = DEFAULT_HEIGHT;
    
    size_t pixelCount = DEFAULT_WIDTH * DEFAULT_HEIGHT;
    
    // Allocate buffers
    uint8_t* rgbBuffer = new (std::nothrow) uint8_t[pixelCount * 3];
    uint8_t* grayBuffer = new (std::nothrow) uint8_t[pixelCount];
    uint8_t* indexedBuffer = new (std::nothrow) uint8_t[pixelCount];
    uint8_t* lutRgbBuffer = new (std::nothrow) uint8_t[pixelCount * 3];
    
    // BMP size: header + row padding (rows must be multiple of 4 bytes)
    size_t rowSize = ((DEFAULT_WIDTH * 3 + 3) / 4) * 4;
    size_t bmpDataSize = rowSize * DEFAULT_HEIGHT;
    size_t bmpTotalSize = BMP_HEADER_SIZE + bmpDataSize;
    uint8_t* bmpBuffer = new (std::nothrow) uint8_t[bmpTotalSize];
    
    if (!rgbBuffer || !grayBuffer || !indexedBuffer || !lutRgbBuffer || !bmpBuffer) {
        Serial.println("[Capture] ERROR: Memory allocation failed");
        delete[] rgbBuffer;
        delete[] grayBuffer;
        delete[] indexedBuffer;
        delete[] lutRgbBuffer;
        delete[] bmpBuffer;
        return CaptureResult::ERROR_MEMORY;
    }
    
    CaptureResult result = CaptureResult::SUCCESS;
    
    // =========================================================================
    // Stage 1: RGB Capture (simulated)
    // =========================================================================
    Serial.println("[Capture] Stage 1: RGB Capture (simulated)");
    uint32_t stageStart = millis();
    
    generateTestPattern(rgbBuffer, DEFAULT_WIDTH, DEFAULT_HEIGHT);
    
    s_lastStats.captureTimeMs = millis() - stageStart;
    s_lastStats.imageSizeBytes = pixelCount * 3;
    Serial.printf("[Capture]   -> %d ms, %zu bytes RGB\n", 
                  s_lastStats.captureTimeMs, s_lastStats.imageSizeBytes);
    
    // =========================================================================
    // Stage 2: Convert to Grayscale + Dithering
    // =========================================================================
    Serial.println("[Capture] Stage 2: Grayscale + Dithering");
    stageStart = millis();
    
    // Convert to grayscale
    rgbToGray(rgbBuffer, grayBuffer, pixelCount);
    
    // Apply dithering based on palette
    if (!applyDither(grayBuffer, DEFAULT_WIDTH, DEFAULT_HEIGHT, indexedBuffer, ctx.getPalette())) {
        Serial.println("[Capture] ERROR: Dithering failed");
        result = CaptureResult::ERROR_DITHER;
        goto cleanup;
    }
    
    s_lastStats.ditherTimeMs = millis() - stageStart;
    Serial.printf("[Capture]   -> %d ms, palette=%d\n", 
                  s_lastStats.ditherTimeMs, static_cast<int>(ctx.getPalette()));
    
    // =========================================================================
    // Stage 3: Apply LUT (palette colors)
    // =========================================================================
    Serial.println("[Capture] Stage 3: Apply LUT");
    stageStart = millis();
    
    if (!applyLUT(indexedBuffer, DEFAULT_WIDTH, DEFAULT_HEIGHT, lutRgbBuffer, ctx.getPalette())) {
        Serial.println("[Capture] ERROR: LUT application failed");
        result = CaptureResult::ERROR_DITHER;
        goto cleanup;
    }
    
    s_lastStats.lutTimeMs = millis() - stageStart;
    Serial.printf("[Capture]   -> %d ms\n", s_lastStats.lutTimeMs);
    
    // =========================================================================
    // Stage 4: Encode BMP
    // =========================================================================
    Serial.println("[Capture] Stage 4: Encode BMP");
    stageStart = millis();
    
    {
        size_t actualBmpSize = 0;
        if (!encodeBMP(lutRgbBuffer, DEFAULT_WIDTH, DEFAULT_HEIGHT, bmpBuffer, actualBmpSize)) {
            Serial.println("[Capture] ERROR: BMP encoding failed");
            result = CaptureResult::ERROR_ENCODE;
            goto cleanup;
        }
        s_lastStats.bmpSizeBytes = actualBmpSize;
    }
    
    s_lastStats.encodeTimeMs = millis() - stageStart;
    Serial.printf("[Capture]   -> %d ms, %zu bytes BMP\n", 
                  s_lastStats.encodeTimeMs, s_lastStats.bmpSizeBytes);
    
    // =========================================================================
    // Stage 5: Save to MockStorage
    // =========================================================================
    Serial.println("[Capture] Stage 5: Save to MockStorage");
    stageStart = millis();
    
    {
        // Initialize storage if needed
        if (!s_mockStorage.isReady()) {
            s_mockStorage.init();
        }
        
        // Generate filename
        char filename[32];
        snprintf(filename, sizeof(filename), "IMG_%04lu.bmp", s_captureCount++);
        
        auto storageResult = s_mockStorage.write(filename, bmpBuffer, s_lastStats.bmpSizeBytes);
        if (storageResult != hal::StorageResult::OK) {
            Serial.printf("[Capture] ERROR: Storage write failed (%d)\n", static_cast<int>(storageResult));
            result = CaptureResult::ERROR_STORAGE;
            goto cleanup;
        }
        
        Serial.printf("[Capture]   -> Saved as '%s'\n", filename);
    }
    
    s_lastStats.storageTimeMs = millis() - stageStart;
    Serial.printf("[Capture]   -> %d ms\n", s_lastStats.storageTimeMs);
    
    // =========================================================================
    // Stage 6: Generate Mini Preview (64x64 for UI)
    // =========================================================================
    Serial.println("[Capture] Stage 6: Generate Mini Preview");
    
    if (!generateMiniPreview(indexedBuffer, DEFAULT_WIDTH, DEFAULT_HEIGHT, s_lastPreview)) {
        Serial.println("[Capture] WARNING: Mini preview generation failed");
        // Non-fatal, continue
    }
    
    // Calculate total time
    s_lastStats.totalTimeMs = millis() - pipelineStart;
    
    Serial.println("[Capture] ========== Pipeline Complete ==========");
    logStats(s_lastStats);
    
cleanup:
    delete[] rgbBuffer;
    delete[] grayBuffer;
    delete[] indexedBuffer;
    delete[] lutRgbBuffer;
    delete[] bmpBuffer;
    
    return result;
}

bool applyDither(const uint8_t* gray, uint16_t width, uint16_t height,
                 uint8_t* output, core::Palette palette) {
    if (!gray || !output || width == 0 || height == 0) {
        return false;
    }
    
    // Initialize dither module if needed
    if (!dither::isInitialized()) {
        dither::initDitherModule(true);
    }
    
    // Set dithering mode based on palette
    switch (palette) {
        case core::Palette::GAMEBOY:
            dither::setDitherMode(dither::DitherMode::GameBoy);
            break;
        case core::Palette::SEPIA:
            dither::setDitherMode(dither::DitherMode::FloydSteinberg);
            break;
        default:
            dither::setDitherMode(dither::DitherMode::GameBoy);
            break;
    }
    
    // Apply ordered dithering with Bayer matrix (simplified 4-level quantization)
    // Instead of 1-bit output, we produce 2-bit indexed output (4 colors)
    static const int8_t BAYER_4X4[4][4] = {
        { -8,  0, -6,  2},
        {  4, -4,  6, -2},
        { -5,  3, -7,  1},
        {  7, -1,  5, -3}
    };
    
    size_t pixelCount = width * height;
    for (size_t i = 0; i < pixelCount; i++) {
        int x = i % width;
        int y = i / width;
        
        // Add Bayer dither offset (scaled for 4-level quantization)
        int ditheredValue = gray[i] + BAYER_4X4[y % 4][x % 4] * 4;
        ditheredValue = constrain(ditheredValue, 0, 255);
        
        // Quantize to 4 levels
        output[i] = quantize4Level((uint8_t)ditheredValue);
    }
    
    return true;
}

bool applyLUT(const uint8_t* indexed, uint16_t width, uint16_t height,
              uint8_t* rgbOutput, core::Palette palette) {
    if (!indexed || !rgbOutput || width == 0 || height == 0) {
        return false;
    }
    
    const uint8_t (*colors)[3] = getPaletteColors(palette);
    size_t pixelCount = width * height;
    
    for (size_t i = 0; i < pixelCount; i++) {
        uint8_t idx = indexed[i] & 0x03;  // 2-bit index (0-3)
        rgbOutput[i * 3 + 0] = colors[idx][0];  // R
        rgbOutput[i * 3 + 1] = colors[idx][1];  // G
        rgbOutput[i * 3 + 2] = colors[idx][2];  // B
    }
    
    return true;
}

bool encodeBMP(const uint8_t* rgb, uint16_t width, uint16_t height,
               uint8_t* bmpOutput, size_t& bmpSize) {
    if (!rgb || !bmpOutput || width == 0 || height == 0) {
        return false;
    }
    
    // Calculate sizes
    size_t rowSize = ((width * 3 + 3) / 4) * 4;  // Row must be multiple of 4
    size_t paddingSize = rowSize - (width * 3);
    size_t dataSize = rowSize * height;
    size_t fileSize = BMP_HEADER_SIZE + dataSize;
    
    // =========================================================================
    // BMP File Header (14 bytes)
    // =========================================================================
    bmpOutput[0] = 'B';
    bmpOutput[1] = 'M';
    
    // File size (little endian)
    bmpOutput[2] = (fileSize >> 0) & 0xFF;
    bmpOutput[3] = (fileSize >> 8) & 0xFF;
    bmpOutput[4] = (fileSize >> 16) & 0xFF;
    bmpOutput[5] = (fileSize >> 24) & 0xFF;
    
    // Reserved (4 bytes)
    bmpOutput[6] = 0;
    bmpOutput[7] = 0;
    bmpOutput[8] = 0;
    bmpOutput[9] = 0;
    
    // Data offset
    bmpOutput[10] = BMP_HEADER_SIZE & 0xFF;
    bmpOutput[11] = (BMP_HEADER_SIZE >> 8) & 0xFF;
    bmpOutput[12] = 0;
    bmpOutput[13] = 0;
    
    // =========================================================================
    // DIB Header (BITMAPINFOHEADER - 40 bytes)
    // =========================================================================
    bmpOutput[14] = 40;  // Header size
    bmpOutput[15] = 0;
    bmpOutput[16] = 0;
    bmpOutput[17] = 0;
    
    // Width (little endian)
    bmpOutput[18] = (width >> 0) & 0xFF;
    bmpOutput[19] = (width >> 8) & 0xFF;
    bmpOutput[20] = 0;
    bmpOutput[21] = 0;
    
    // Height (positive = bottom-up)
    bmpOutput[22] = (height >> 0) & 0xFF;
    bmpOutput[23] = (height >> 8) & 0xFF;
    bmpOutput[24] = 0;
    bmpOutput[25] = 0;
    
    // Planes (always 1)
    bmpOutput[26] = 1;
    bmpOutput[27] = 0;
    
    // Bits per pixel (24-bit)
    bmpOutput[28] = 24;
    bmpOutput[29] = 0;
    
    // Compression (0 = BI_RGB, no compression)
    bmpOutput[30] = 0;
    bmpOutput[31] = 0;
    bmpOutput[32] = 0;
    bmpOutput[33] = 0;
    
    // Image size (can be 0 for BI_RGB)
    bmpOutput[34] = (dataSize >> 0) & 0xFF;
    bmpOutput[35] = (dataSize >> 8) & 0xFF;
    bmpOutput[36] = (dataSize >> 16) & 0xFF;
    bmpOutput[37] = (dataSize >> 24) & 0xFF;
    
    // Horizontal resolution (pixels per meter, ~72 DPI)
    bmpOutput[38] = 0x13;
    bmpOutput[39] = 0x0B;
    bmpOutput[40] = 0;
    bmpOutput[41] = 0;
    
    // Vertical resolution
    bmpOutput[42] = 0x13;
    bmpOutput[43] = 0x0B;
    bmpOutput[44] = 0;
    bmpOutput[45] = 0;
    
    // Colors in palette (0 = 2^n)
    bmpOutput[46] = 0;
    bmpOutput[47] = 0;
    bmpOutput[48] = 0;
    bmpOutput[49] = 0;
    
    // Important colors (0 = all)
    bmpOutput[50] = 0;
    bmpOutput[51] = 0;
    bmpOutput[52] = 0;
    bmpOutput[53] = 0;
    
    // =========================================================================
    // Pixel Data (bottom-up, BGR order)
    // =========================================================================
    uint8_t* dataPtr = bmpOutput + BMP_HEADER_SIZE;
    
    for (int y = height - 1; y >= 0; y--) {  // Bottom-up
        for (int x = 0; x < width; x++) {
            size_t srcIdx = (y * width + x) * 3;
            // BMP uses BGR order
            *dataPtr++ = rgb[srcIdx + 2];  // B
            *dataPtr++ = rgb[srcIdx + 1];  // G
            *dataPtr++ = rgb[srcIdx + 0];  // R
        }
        // Row padding
        for (size_t p = 0; p < paddingSize; p++) {
            *dataPtr++ = 0;
        }
    }
    
    bmpSize = fileSize;
    return true;
}

bool generateMiniPreview(const uint8_t* indexed, uint16_t width, uint16_t height,
                         MiniPreview& preview) {
    if (!indexed || width == 0 || height == 0) {
        preview.valid = false;
        return false;
    }
    
    // Simple nearest-neighbor downscaling
    float scaleX = (float)width / MINI_PREVIEW_SIZE;
    float scaleY = (float)height / MINI_PREVIEW_SIZE;
    
    for (int py = 0; py < MINI_PREVIEW_SIZE; py++) {
        for (int px = 0; px < MINI_PREVIEW_SIZE; px++) {
            int srcX = (int)(px * scaleX);
            int srcY = (int)(py * scaleY);
            
            srcX = min(srcX, (int)width - 1);
            srcY = min(srcY, (int)height - 1);
            
            uint8_t idx = indexed[srcY * width + srcX];
            // Convert 2-bit index to 8-bit grayscale for preview
            preview.data[py * MINI_PREVIEW_SIZE + px] = (3 - idx) * 85;  // 0->255, 1->170, 2->85, 3->0
        }
    }
    
    preview.width = MINI_PREVIEW_SIZE;
    preview.height = MINI_PREVIEW_SIZE;
    preview.valid = true;
    
    return true;
}

const CaptureStats& getLastStats() {
    return s_lastStats;
}

const MiniPreview& getLastPreview() {
    return s_lastPreview;
}

const char* resultToString(CaptureResult result) {
    switch (result) {
        case CaptureResult::SUCCESS:        return "SUCCESS";
        case CaptureResult::ERROR_CAPTURE:  return "ERROR_CAPTURE";
        case CaptureResult::ERROR_DITHER:   return "ERROR_DITHER";
        case CaptureResult::ERROR_ENCODE:   return "ERROR_ENCODE";
        case CaptureResult::ERROR_STORAGE:  return "ERROR_STORAGE";
        case CaptureResult::ERROR_MEMORY:   return "ERROR_MEMORY";
        case CaptureResult::ERROR_INVALID_CTX: return "ERROR_INVALID_CTX";
        default: return "UNKNOWN";
    }
}

void logStats(const CaptureStats& stats) {
    Serial.println("[Capture] ========== Capture Statistics ==========");
    Serial.printf("[Capture] Resolution: %dx%d\n", stats.width, stats.height);
    Serial.printf("[Capture] Timing breakdown:\n");
    Serial.printf("[Capture]   Capture:  %4lu ms\n", stats.captureTimeMs);
    Serial.printf("[Capture]   Dither:   %4lu ms\n", stats.ditherTimeMs);
    Serial.printf("[Capture]   LUT:      %4lu ms\n", stats.lutTimeMs);
    Serial.printf("[Capture]   Encode:   %4lu ms\n", stats.encodeTimeMs);
    Serial.printf("[Capture]   Storage:  %4lu ms\n", stats.storageTimeMs);
    Serial.printf("[Capture]   ----------------------\n");
    Serial.printf("[Capture]   TOTAL:    %4lu ms\n", stats.totalTimeMs);
    Serial.printf("[Capture] Sizes:\n");
    Serial.printf("[Capture]   RGB data: %zu bytes\n", stats.imageSizeBytes);
    Serial.printf("[Capture]   BMP file: %zu bytes\n", stats.bmpSizeBytes);
    Serial.println("[Capture] ===========================================");
}

} // namespace capture
} // namespace features
} // namespace pxlcam
