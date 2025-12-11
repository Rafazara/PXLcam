/**
 * @file preview_dither.cpp
 * @brief GameBoy-style dithering and histogram equalization implementation
 */

#include "preview_dither.h"
#include "pxlcam_config.h"
#include "logging.h"

#include <cstring>
#include <cmath>
#include <algorithm>

namespace pxlcam::dither {

namespace {

constexpr const char* kLogTag = "pxlcam-dither";

bool g_initialized = false;
DitherMode g_currentMode = DitherMode::GameBoy;

// GameBoy inspired tone remap LUT (maps 0..255 -> 0..3 levels)
uint8_t g_gbLut[256];

// Night vision gamma LUT
uint8_t g_nightLut[256];

// 8x8 Bayer matrix normalized to range 0..63
static const uint8_t kBayer8x8[64] = {
     0, 48, 12, 60,  3, 51, 15, 63,
    32, 16, 44, 28, 35, 19, 47, 31,
     8, 56,  4, 52, 11, 59,  7, 55,
    40, 24, 36, 20, 43, 27, 39, 23,
     2, 50, 14, 62,  1, 49, 13, 61,
    34, 18, 46, 30, 33, 17, 45, 29,
    10, 58,  6, 54,  9, 57,  5, 53,
    42, 26, 38, 22, 41, 25, 37, 21
};

// Utility: set bit in packed bitmap
inline void setBitPacked(uint8_t* out, int x, int y, int w, bool bit) {
    const int idx = y * w + x;
    const int byteIndex = idx >> 3;
    const int bitIndex = 7 - (idx & 7);
    if (bit) {
        out[byteIndex] |= (1 << bitIndex);
    } else {
        out[byteIndex] &= ~(1 << bitIndex);
    }
}

// Build GameBoy-like LUT
void buildGameBoyLUT() {
    for (int i = 0; i < 256; ++i) {
        float v = i / 255.0f;
        // Non-linear remap (gamma-ish) to emulate GB tonal response
        float gm = powf(v, 0.9f);
        // Map to 4 levels (0, 1, 2, 3)
        int level = static_cast<int>(roundf(gm * 3.0f));
        level = std::max(0, std::min(3, level));
        g_gbLut[i] = static_cast<uint8_t>(level);
    }
}

// Build night vision gamma LUT
void buildNightLUT(float gamma = 0.6f) {
    for (int i = 0; i < 256; ++i) {
        float v = i / 255.0f;
        float nv = powf(v, gamma);
        g_nightLut[i] = static_cast<uint8_t>(std::min(255.0f, nv * 255.0f));
    }
}

}  // namespace

void initDitherModule(bool useGameBoyLUT) {
    if (g_initialized) return;
    
    PXLCAM_LOGI_TAG(kLogTag, "Initializing dither module");
    
    if (useGameBoyLUT) {
        buildGameBoyLUT();
    }
    buildNightLUT(0.6f);
    
    g_currentMode = static_cast<DitherMode>(PXLCAM_DEFAULT_DITHER_MODE);
    g_initialized = true;
    
    PXLCAM_LOGI_TAG(kLogTag, "Dither module initialized, mode=%d", static_cast<int>(g_currentMode));
}

bool isInitialized() {
    return g_initialized;
}

void setDitherMode(DitherMode mode) {
    g_currentMode = mode;
}

DitherMode getDitherMode() {
    return g_currentMode;
}

void histogramEqualize(uint8_t* gray, int w, int h) {
    if (!gray || w <= 0 || h <= 0) return;
    
    const int N = w * h;
    
    // Build histogram
    uint32_t hist[256] = {0};
    for (int i = 0; i < N; ++i) {
        hist[gray[i]]++;
    }
    
    // Cumulative histogram
    uint32_t cum[256];
    cum[0] = hist[0];
    for (int i = 1; i < 256; ++i) {
        cum[i] = cum[i - 1] + hist[i];
    }
    
    // Apply equalization
    for (int i = 0; i < N; ++i) {
        const uint8_t v = gray[i];
        const float c = static_cast<float>(cum[v]) / static_cast<float>(N);
        gray[i] = static_cast<uint8_t>(roundf(c * 255.0f));
    }
}

void applyGameBoyDither(const uint8_t* gray, int w, int h, uint8_t* outBitmap) {
    if (!g_initialized) {
        initDitherModule(true);
    }
    
    if (!gray || !outBitmap || w <= 0 || h <= 0) return;
    
    const int N = w * h;
    const int bytes = (N + 7) / 8;
    memset(outBitmap, 0, bytes);
    
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const int idx = y * w + x;
            const uint8_t g = gray[idx];
            const uint8_t level = g_gbLut[g];  // 0..3
            
            // Map level to mid-level value (0, 85, 170, 255)
            const uint8_t target = level * 85;
            
            // Bayer threshold with scaling
            const uint8_t b = kBayer8x8[(y & 7) * 8 + (x & 7)];
            const int threshold = static_cast<int>(target) + (b * 85 / 64) - 42;
            
            setBitPacked(outBitmap, x, y, w, static_cast<int>(g) >= threshold);
        }
    }
}

void applyFloydSteinbergDither(const uint8_t* gray, int w, int h, uint8_t* outBitmap) {
    if (!gray || !outBitmap || w <= 0 || h <= 0) return;
    
    const int N = w * h;
    const int bytes = (N + 7) / 8;
    memset(outBitmap, 0, bytes);
    
    // Need working buffer for error diffusion
    // Using static buffer to avoid heap allocation in loop
    static float errorBuf[64 * 64];
    if (N > 64 * 64) return;  // Safety check
    
    for (int i = 0; i < N; ++i) {
        errorBuf[i] = static_cast<float>(gray[i]);
    }
    
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const int idx = y * w + x;
            const float oldv = errorBuf[idx];
            const float newv = oldv < 128.0f ? 0.0f : 255.0f;
            const float err = oldv - newv;
            
            setBitPacked(outBitmap, x, y, w, newv >= 128.0f);
            
            // Distribute error to neighbors
            if (x + 1 < w) {
                errorBuf[idx + 1] += err * 7.0f / 16.0f;
            }
            if (x > 0 && y + 1 < h) {
                errorBuf[idx + w - 1] += err * 3.0f / 16.0f;
            }
            if (y + 1 < h) {
                errorBuf[idx + w] += err * 5.0f / 16.0f;
            }
            if (x + 1 < w && y + 1 < h) {
                errorBuf[idx + w + 1] += err * 1.0f / 16.0f;
            }
        }
    }
}

void convertTo1bitThreshold(const uint8_t* gray, int w, int h, uint8_t* outBitmap, uint8_t threshold) {
    if (!gray || !outBitmap || w <= 0 || h <= 0) return;
    
    const int N = w * h;
    const int bytes = (N + 7) / 8;
    memset(outBitmap, 0, bytes);
    
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const int idx = y * w + x;
            setBitPacked(outBitmap, x, y, w, gray[idx] > threshold);
        }
    }
}

void applyNightVision(uint8_t* gray, int w, int h, float gammaBoost, float contrastMult) {
    if (!gray || w <= 0 || h <= 0) return;
    
    (void)gammaBoost;  // Using pre-built LUT
    
    const int N = w * h;
    
    // Apply gamma via LUT
    for (int i = 0; i < N; ++i) {
        gray[i] = g_nightLut[gray[i]];
    }
    
    // Apply contrast enhancement
    if (contrastMult != 1.0f) {
        for (int i = 0; i < N; ++i) {
            float v = (gray[i] - 128.0f) * contrastMult + 128.0f;
            gray[i] = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, v)));
        }
    }
}

void processDither(const uint8_t* gray, int w, int h, uint8_t* outBitmap, bool enableHistEq) {
    if (!gray || !outBitmap || w <= 0 || h <= 0) return;
    
    // Make working copy if we need to modify
    static uint8_t workBuf[64 * 64];
    const int N = w * h;
    if (N > 64 * 64) return;
    
    memcpy(workBuf, gray, N);
    
    // Optional histogram equalization
    if (enableHistEq) {
        histogramEqualize(workBuf, w, h);
    }
    
    // Apply current dither mode
    switch (g_currentMode) {
        case DitherMode::Threshold:
            convertTo1bitThreshold(workBuf, w, h, outBitmap, 128);
            break;
            
        case DitherMode::GameBoy:
            applyGameBoyDither(workBuf, w, h, outBitmap);
            break;
            
        case DitherMode::FloydSteinberg:
            applyFloydSteinbergDither(workBuf, w, h, outBitmap);
            break;
            
        case DitherMode::Night:
            applyNightVision(workBuf, w, h);
            convertTo1bitThreshold(workBuf, w, h, outBitmap, 100);  // Lower threshold for night
            break;
    }
}

bool selfTest() {
    PXLCAM_LOGI_TAG(kLogTag, "Running dither self-test");
    
    // Test 1: Initialize
    initDitherModule(true);
    if (!g_initialized) {
        PXLCAM_LOGE_TAG(kLogTag, "FAIL: initialization");
        return false;
    }
    
    // Test 2: LUT values check
    if (g_gbLut[0] != 0 || g_gbLut[255] != 3) {
        PXLCAM_LOGE_TAG(kLogTag, "FAIL: LUT values incorrect");
        return false;
    }
    
    // Test 3: Small dither test
    uint8_t testGray[16] = {0, 32, 64, 96, 128, 160, 192, 224, 255, 128, 128, 128, 0, 255, 0, 255};
    uint8_t testOut[2] = {0};
    convertTo1bitThreshold(testGray, 4, 4, testOut, 128);
    
    // First 4 pixels: 0,32,64,96 -> all below 128 -> bits 0000
    // Next 4 pixels: 128,160,192,224 -> all >= 128 -> bits 1111
    // Expected first byte: 0b00001111 = 0x0F
    if (testOut[0] != 0x0F) {
        PXLCAM_LOGE_TAG(kLogTag, "FAIL: threshold test, got 0x%02X expected 0x0F", testOut[0]);
        return false;
    }
    
    PXLCAM_LOGI_TAG(kLogTag, "PASS: dither self-test");
    return true;
}

}  // namespace pxlcam::dither
