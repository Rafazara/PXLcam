/**
 * @file capture_pipeline.cpp
 * @brief Unified capture and post-processing pipeline for PXLcam v1.2.0
 * 
 * Implements stylized capture:
 * - GAMEBOY: Bayer 8x8 ordered dithering → 4 tons
 * - NIGHT: Gamma boost + contrast enhancement
 * - NORMAL: Clean grayscale conversion
 * 
 * Output: 8-bit grayscale BMP for easy viewing
 * 
 * v1.3.0 TODOs:
 * - Integrate new palette system from filters/palette.h
 * - Use dither_pipeline for algorithm selection
 * - Add postprocess chain integration
 */

#include "capture_pipeline.h"
#include "mode_manager.h"
#include "logging.h"
#include "pxlcam_config.h"

#include <Arduino.h>
#include <esp_camera.h>
#include <esp_heap_caps.h>
#include <img_converters.h>
#include <cstring>
#include <cmath>

// =============================================================================
// v1.3.0 Filter Pipeline Includes
// =============================================================================

#if PXLCAM_FEATURE_STYLIZED_CAPTURE
// TODO v1.3.0: Enable new filter pipeline
// #include "filters/palette.h"
// #include "filters/dither_pipeline.h"
// #include "filters/postprocess.h"
#endif

namespace pxlcam::capture {

namespace {

constexpr const char* kLogTag = "capture";

//==============================================================================
// Internal State
//==============================================================================

bool g_initialized = false;
camera_fb_t* g_activeFrame = nullptr;
uint8_t* g_processedBuffer = nullptr;
uint8_t* g_tempRgbBuffer = nullptr;
size_t g_allocatedSize = 0;

// Metrics
uint32_t g_lastCaptureDuration = 0;
uint32_t g_lastProcessDuration = 0;

// Maximum buffer size for QVGA
constexpr int kMaxWidth = 320;
constexpr int kMaxHeight = 240;
constexpr size_t kMaxPixels = kMaxWidth * kMaxHeight;
constexpr size_t kMaxRgbSize = kMaxPixels * 3;
constexpr size_t kMaxBmpSize = 54 + 1024 + kMaxPixels;  // Header + palette + pixels

//==============================================================================
// Bayer 8x8 Dithering Matrix
//==============================================================================

// Classic Bayer 8x8 ordered dithering matrix (normalized 0-63)
const uint8_t kBayer8x8[8][8] = {
    {  0, 32,  8, 40,  2, 34, 10, 42 },
    { 48, 16, 56, 24, 50, 18, 58, 26 },
    { 12, 44,  4, 36, 14, 46,  6, 38 },
    { 60, 28, 52, 20, 62, 30, 54, 22 },
    {  3, 35, 11, 43,  1, 33,  9, 41 },
    { 51, 19, 59, 27, 49, 17, 57, 25 },
    { 15, 47,  7, 39, 13, 45,  5, 37 },
    { 63, 31, 55, 23, 61, 29, 53, 21 }
};

// GameBoy 4-tone palette (dark→light)
const uint8_t kGameBoyPalette[4] = { 0x0F, 0x56, 0x9B, 0xCF };

//==============================================================================
// Gamma LUT for Night Mode
//==============================================================================

uint8_t g_gammaLut[256];
bool g_gammaLutReady = false;

void buildGammaLut(float gamma) {
    for (int i = 0; i < 256; i++) {
        float normalized = i / 255.0f;
        float corrected = powf(normalized, gamma);
        int val = static_cast<int>(corrected * 255.0f + 0.5f);
        g_gammaLut[i] = (val > 255) ? 255 : ((val < 0) ? 0 : val);
    }
    g_gammaLutReady = true;
}

//==============================================================================
// Memory Allocation (PSRAM Priority)
//==============================================================================

uint8_t* allocatePsram(size_t size, const char* name) {
    uint8_t* ptr = static_cast<uint8_t*>(
        heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    
    if (ptr) {
        PXLCAM_LOGI_TAG(kLogTag, "%s: %u bytes em PSRAM", name, size);
    } else {
        // Fallback to heap
        ptr = static_cast<uint8_t*>(heap_caps_malloc(size, MALLOC_CAP_8BIT));
        if (ptr) {
            PXLCAM_LOGW_TAG(kLogTag, "%s: %u bytes em HEAP (sem PSRAM)", name, size);
        } else {
            PXLCAM_LOGE_TAG(kLogTag, "%s: FALHA ao alocar %u bytes", name, size);
        }
    }
    return ptr;
}

void freePsram(uint8_t** ptr) {
    if (*ptr) {
        heap_caps_free(*ptr);
        *ptr = nullptr;
    }
}

//==============================================================================
// RGB → Grayscale Conversion
//==============================================================================

void rgbToGrayscale(const uint8_t* rgb, uint8_t* gray, int w, int h) {
    const size_t pixels = w * h;
    for (size_t i = 0; i < pixels; i++) {
        const uint8_t* p = rgb + i * 3;
        // ITU-R BT.601: Y = 0.299R + 0.587G + 0.114B
        gray[i] = (77 * p[0] + 150 * p[1] + 29 * p[2]) >> 8;
    }
}

//==============================================================================
// GameBoy Dithering (Bayer 8x8, 4 tons)
//==============================================================================

void applyGameBoyDither(uint8_t* gray, int w, int h) {
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            const int idx = y * w + x;
            const uint8_t pixel = gray[idx];
            
            // Get Bayer threshold (0-63) and scale to 0-255
            const uint8_t threshold = kBayer8x8[y & 7][x & 7] * 4;
            
            // Determine which of 4 tones based on pixel + dither threshold
            // Divide into 4 bands with dither offset
            int adjusted = pixel + (threshold - 128);
            adjusted = (adjusted < 0) ? 0 : ((adjusted > 255) ? 255 : adjusted);
            
            // Map to 4 tones
            uint8_t tone;
            if (adjusted < 64) {
                tone = kGameBoyPalette[0];       // Darkest
            } else if (adjusted < 128) {
                tone = kGameBoyPalette[1];
            } else if (adjusted < 192) {
                tone = kGameBoyPalette[2];
            } else {
                tone = kGameBoyPalette[3];       // Lightest
            }
            
            gray[idx] = tone;
        }
    }
}

//==============================================================================
// Night Mode Enhancement
//==============================================================================

void applyNightEnhance(uint8_t* gray, int w, int h) {
    const size_t pixels = w * h;
    
    // Build gamma LUT if needed (gamma < 1 = brighten)
    if (!g_gammaLutReady) {
        buildGammaLut(0.6f);  // Night boost
    }
    
    // Apply gamma + contrast
    const float contrast = 1.4f;
    const int mid = 128;
    
    for (size_t i = 0; i < pixels; i++) {
        // Gamma correction
        int val = g_gammaLut[gray[i]];
        
        // Contrast enhancement
        val = static_cast<int>((val - mid) * contrast + mid);
        val = (val < 0) ? 0 : ((val > 255) ? 255 : val);
        
        gray[i] = static_cast<uint8_t>(val);
    }
}

//==============================================================================
// BMP Encoding (8-bit Grayscale)
//==============================================================================

#pragma pack(push, 1)
struct BmpFileHeader {
    uint16_t type;          // "BM"
    uint32_t size;          // File size
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offBits;       // Offset to pixel data
};

struct BmpInfoHeader {
    uint32_t size;          // Header size (40)
    int32_t  width;
    int32_t  height;        // Negative = top-down
    uint16_t planes;        // 1
    uint16_t bitCount;      // 8 for grayscale
    uint32_t compression;   // 0 = none
    uint32_t sizeImage;
    int32_t  xPelsPerMeter;
    int32_t  yPelsPerMeter;
    uint32_t clrUsed;       // 256 for 8-bit
    uint32_t clrImportant;
};
#pragma pack(pop)

bool encodeBmpGrayscale(const uint8_t* gray, int w, int h, 
                         uint8_t* outBmp, size_t& outLength) {
    // Calculate row padding (BMP rows must be 4-byte aligned)
    const int rowBytes = w;
    const int padding = (4 - (rowBytes % 4)) % 4;
    const int paddedRow = rowBytes + padding;
    
    // Header sizes
    const size_t headerSize = sizeof(BmpFileHeader) + sizeof(BmpInfoHeader);
    const size_t paletteSize = 256 * 4;  // RGBX for each grayscale level
    const size_t pixelSize = paddedRow * h;
    const size_t totalSize = headerSize + paletteSize + pixelSize;
    
    outLength = totalSize;
    
    // File header
    BmpFileHeader* fileHdr = reinterpret_cast<BmpFileHeader*>(outBmp);
    fileHdr->type = 0x4D42;  // "BM"
    fileHdr->size = totalSize;
    fileHdr->reserved1 = 0;
    fileHdr->reserved2 = 0;
    fileHdr->offBits = headerSize + paletteSize;
    
    // Info header
    BmpInfoHeader* infoHdr = reinterpret_cast<BmpInfoHeader*>(outBmp + sizeof(BmpFileHeader));
    infoHdr->size = sizeof(BmpInfoHeader);
    infoHdr->width = w;
    infoHdr->height = -h;  // Negative = top-down (no flip needed)
    infoHdr->planes = 1;
    infoHdr->bitCount = 8;
    infoHdr->compression = 0;
    infoHdr->sizeImage = pixelSize;
    infoHdr->xPelsPerMeter = 2835;  // 72 DPI
    infoHdr->yPelsPerMeter = 2835;
    infoHdr->clrUsed = 256;
    infoHdr->clrImportant = 256;
    
    // Grayscale palette (256 entries)
    uint8_t* palette = outBmp + headerSize;
    for (int i = 0; i < 256; i++) {
        palette[i * 4 + 0] = i;  // Blue
        palette[i * 4 + 1] = i;  // Green
        palette[i * 4 + 2] = i;  // Red
        palette[i * 4 + 3] = 0;  // Reserved
    }
    
    // Pixel data (with padding)
    uint8_t* pixels = outBmp + headerSize + paletteSize;
    for (int y = 0; y < h; y++) {
        memcpy(pixels + y * paddedRow, gray + y * w, w);
        // Clear padding bytes
        if (padding > 0) {
            memset(pixels + y * paddedRow + w, 0, padding);
        }
    }
    
    return true;
}

}  // anonymous namespace

//==============================================================================
// Public API
//==============================================================================

bool init() {
    if (g_initialized) {
        return true;
    }
    
    // Allocate processing buffer (for BMP output)
    g_processedBuffer = allocatePsram(kMaxBmpSize, "ProcessBuffer");
    if (!g_processedBuffer) {
        return false;
    }
    
    // Allocate temp RGB buffer (for JPEG decode)
    g_tempRgbBuffer = allocatePsram(kMaxRgbSize, "TempRgbBuffer");
    if (!g_tempRgbBuffer) {
        freePsram(&g_processedBuffer);
        return false;
    }
    
    g_allocatedSize = kMaxBmpSize + kMaxRgbSize;
    g_initialized = true;
    
    PXLCAM_LOGI_TAG(kLogTag, "Pipeline inicializado (total: %u KB)", g_allocatedSize / 1024);
    return true;
}

bool isReady() {
    return g_initialized && g_processedBuffer && g_tempRgbBuffer;
}

CaptureResult captureFrame(ProcessedImage& outImage) {
    return captureWithMode(pxlcam::mode::getCurrentMode(), outImage);
}

CaptureResult captureWithMode(pxlcam::mode::CaptureMode mode, ProcessedImage& outImage) {
    // Clear output
    outImage = { nullptr, 0, 0, 0, false, "bmp" };
    
    if (!g_initialized && !init()) {
        PXLCAM_LOGE_TAG(kLogTag, "Pipeline nao inicializado");
        return CaptureResult::MemoryError;
    }
    
    // Release any previous frame
    releaseFrame();
    
    //==========================================================================
    // Step 1: Capture from camera
    //==========================================================================
    PXLCAM_LOGI_TAG(kLogTag, "Capturando frame (modo: %s)...", 
                    pxlcam::mode::getModeName(mode));
    
    uint32_t captureStart = millis();
    g_activeFrame = esp_camera_fb_get();
    g_lastCaptureDuration = millis() - captureStart;
    
    if (!g_activeFrame) {
        PXLCAM_LOGE_TAG(kLogTag, "ERRO: esp_camera_fb_get() falhou");
        return CaptureResult::CameraError;
    }
    
    const int w = g_activeFrame->width;
    const int h = g_activeFrame->height;
    const size_t pixels = w * h;
    
    PXLCAM_LOGI_TAG(kLogTag, "Frame: %dx%d, %u bytes, format=%d", 
                    w, h, g_activeFrame->len, g_activeFrame->format);
    
    // Validate dimensions
    if (w > kMaxWidth || h > kMaxHeight) {
        PXLCAM_LOGE_TAG(kLogTag, "Frame muito grande: %dx%d (max %dx%d)", w, h, kMaxWidth, kMaxHeight);
        return CaptureResult::MemoryError;
    }
    
    //==========================================================================
    // Step 2: Convert to RGB888 if needed
    //==========================================================================
    uint32_t processStart = millis();
    uint8_t* rgb888 = nullptr;
    
    if (g_activeFrame->format == PIXFORMAT_JPEG) {
        PXLCAM_LOGI_TAG(kLogTag, "Decodificando JPEG...");
        
        if (!fmt2rgb888(g_activeFrame->buf, g_activeFrame->len, 
                        PIXFORMAT_JPEG, g_tempRgbBuffer)) {
            PXLCAM_LOGE_TAG(kLogTag, "ERRO: Falha ao decodificar JPEG");
            return CaptureResult::ProcessingError;
        }
        rgb888 = g_tempRgbBuffer;
    } 
    else if (g_activeFrame->format == PIXFORMAT_RGB888) {
        rgb888 = g_activeFrame->buf;
    } 
    else {
        PXLCAM_LOGE_TAG(kLogTag, "Formato nao suportado: %d", g_activeFrame->format);
        return CaptureResult::ProcessingError;
    }
    
    //==========================================================================
    // Step 3: Convert to Grayscale
    //==========================================================================
    // Use beginning of processedBuffer as grayscale temp
    uint8_t* grayBuf = g_processedBuffer;
    
    PXLCAM_LOGI_TAG(kLogTag, "Convertendo para grayscale...");
    rgbToGrayscale(rgb888, grayBuf, w, h);
    
    //==========================================================================
    // Step 4: Apply Mode-Specific Filter
    //==========================================================================
    switch (mode) {
        case pxlcam::mode::CaptureMode::GameBoy:
            PXLCAM_LOGI_TAG(kLogTag, "Aplicando dithering GameBoy (Bayer 8x8)...");
            applyGameBoyDither(grayBuf, w, h);
            break;
            
        case pxlcam::mode::CaptureMode::Night:
            PXLCAM_LOGI_TAG(kLogTag, "Aplicando enhance Night (gamma + contraste)...");
            applyNightEnhance(grayBuf, w, h);
            break;
            
        case pxlcam::mode::CaptureMode::Normal:
        default:
            PXLCAM_LOGI_TAG(kLogTag, "Modo Normal - grayscale neutro");
            // Keep as-is
            break;
    }
    
    //==========================================================================
    // Step 5: Log Histogram & Sample Tones (Debug)
    //==========================================================================
    logHistogram(grayBuf, pixels);
    logSampleTones(grayBuf, w, h);
    
    //==========================================================================
    // Step 6: Encode as BMP
    //==========================================================================
    PXLCAM_LOGI_TAG(kLogTag, "Codificando BMP...");
    
    // Allocate separate BMP buffer after grayscale processing
    size_t bmpSize = getBmpSize(w, h);
    uint8_t* bmpBuf = g_processedBuffer + pixels;  // After grayscale data
    
    // Check buffer space
    if (pixels + bmpSize > kMaxBmpSize + pixels) {
        PXLCAM_LOGE_TAG(kLogTag, "Buffer insuficiente para BMP");
        return CaptureResult::MemoryError;
    }
    
    size_t actualBmpSize = 0;
    if (!encodeBmpGrayscale(grayBuf, w, h, bmpBuf, actualBmpSize)) {
        PXLCAM_LOGE_TAG(kLogTag, "ERRO: Falha ao codificar BMP");
        return CaptureResult::ProcessingError;
    }
    
    // Move BMP to start of buffer for cleaner output
    memmove(g_processedBuffer, bmpBuf, actualBmpSize);
    
    g_lastProcessDuration = millis() - processStart;
    
    //==========================================================================
    // Step 7: Return Result
    //==========================================================================
    outImage.data = g_processedBuffer;
    outImage.length = actualBmpSize;
    outImage.width = w;
    outImage.height = h;
    outImage.isProcessed = true;
    outImage.extension = "bmp";
    
    PXLCAM_LOGI_TAG(kLogTag, "Captura completa: %dx%d, %u bytes BMP", w, h, actualBmpSize);
    PXLCAM_LOGI_TAG(kLogTag, "Tempos: captura=%ums, processo=%ums", 
                    g_lastCaptureDuration, g_lastProcessDuration);
    
    return CaptureResult::Success;
}

void releaseFrame() {
    if (g_activeFrame) {
        esp_camera_fb_return(g_activeFrame);
        g_activeFrame = nullptr;
    }
}

//==============================================================================
// Filter API
//==============================================================================

bool applyFilter(const uint8_t* rgb, int w, int h,
                 pxlcam::mode::CaptureMode mode, uint8_t* outGray) {
    if (!rgb || !outGray || w <= 0 || h <= 0) {
        return false;
    }
    
    // Convert to grayscale
    rgbToGrayscale(rgb, outGray, w, h);
    
    // Apply mode filter
    switch (mode) {
        case pxlcam::mode::CaptureMode::GameBoy:
            applyGameBoyDither(outGray, w, h);
            break;
        case pxlcam::mode::CaptureMode::Night:
            applyNightEnhance(outGray, w, h);
            break;
        default:
            break;
    }
    
    return true;
}

bool postProcess(const uint8_t* rgb888, uint16_t width, uint16_t height,
                 pxlcam::mode::CaptureMode mode, uint8_t* outBuffer, size_t& outLength) {
    if (!rgb888 || !outBuffer) {
        return false;
    }
    
    const size_t pixels = width * height;
    
    // Convert + filter
    if (!applyFilter(rgb888, width, height, mode, outBuffer)) {
        return false;
    }
    
    outLength = pixels;  // Grayscale: 1 byte per pixel
    return true;
}

//==============================================================================
// BMP Utilities
//==============================================================================

bool encodeGrayscaleBmp(const uint8_t* gray, int w, int h,
                         uint8_t* outBmp, size_t& outLength) {
    return encodeBmpGrayscale(gray, w, h, outBmp, outLength);
}

size_t getBmpSize(int w, int h) {
    const int padding = (4 - (w % 4)) % 4;
    const int paddedRow = w + padding;
    return 54 + 1024 + paddedRow * h;  // Headers + palette + pixels
}

size_t estimateOutputSize(uint16_t width, uint16_t height, pxlcam::mode::CaptureMode mode) {
    (void)mode;  // All modes output BMP
    return getBmpSize(width, height);
}

//==============================================================================
// Metrics
//==============================================================================

uint32_t getLastCaptureDuration() {
    return g_lastCaptureDuration;
}

uint32_t getLastProcessDuration() {
    return g_lastProcessDuration;
}

const char* getResultMessage(CaptureResult result) {
    switch (result) {
        case CaptureResult::Success:         return "OK";
        case CaptureResult::CameraError:     return "Erro camera";
        case CaptureResult::ProcessingError: return "Erro processo";
        case CaptureResult::MemoryError:     return "Sem memoria";
        case CaptureResult::Cancelled:       return "Cancelado";
        default:                             return "Erro";
    }
}

//==============================================================================
// Debug: Histogram & Tone Analysis
//==============================================================================

void logHistogram(const uint8_t* gray, size_t length) {
    if (!gray || length == 0) return;
    
    // Simple 8-bin histogram
    uint32_t bins[8] = {0};
    
    for (size_t i = 0; i < length; i++) {
        bins[gray[i] >> 5]++;  // Divide 256 levels into 8 bins
    }
    
    // Find max for scaling
    uint32_t maxBin = 1;
    for (int i = 0; i < 8; i++) {
        if (bins[i] > maxBin) maxBin = bins[i];
    }
    
    // Log histogram bars
    PXLCAM_LOGI_TAG(kLogTag, "=== HISTOGRAMA ===");
    const char* labels[] = {"0-31", "32-63", "64-95", "96-127", 
                            "128-159", "160-191", "192-223", "224-255"};
    
    for (int i = 0; i < 8; i++) {
        int barLen = (bins[i] * 20) / maxBin;
        char bar[22];
        memset(bar, '#', barLen);
        bar[barLen] = '\0';
        PXLCAM_LOGI_TAG(kLogTag, "%s: %s (%u)", labels[i], bar, bins[i]);
    }
}

void logSampleTones(const uint8_t* gray, int w, int h) {
    if (!gray || w <= 0 || h <= 0) return;
    
    // Sample 9 points in a 3x3 grid
    PXLCAM_LOGI_TAG(kLogTag, "=== AMOSTRA TONS ===");
    
    const int xStep = w / 4;
    const int yStep = h / 4;
    
    char line[64];
    for (int row = 0; row < 3; row++) {
        int y = yStep * (row + 1);
        snprintf(line, sizeof(line), "Y%d: ", y);
        
        for (int col = 0; col < 3; col++) {
            int x = xStep * (col + 1);
            uint8_t val = gray[y * w + x];
            char tmp[16];
            snprintf(tmp, sizeof(tmp), "[%3d] ", val);
            strcat(line, tmp);
        }
        PXLCAM_LOGI_TAG(kLogTag, "%s", line);
    }
}

}  // namespace pxlcam::capture
