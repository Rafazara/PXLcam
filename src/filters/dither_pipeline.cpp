/**
 * @file dither_pipeline.cpp
 * @brief Dithering Pipeline implementation for PXLcam v1.3.0
 * 
 * @version 1.3.0
 * @date 2024
 */

#include "filters/dither_pipeline.h"

#if PXLCAM_FEATURE_STYLIZED_CAPTURE

namespace pxlcam {
namespace filters {

// =============================================================================
// Bayer Dithering Matrices
// =============================================================================

// 2x2 Bayer matrix (normalized to 0-255)
static const uint8_t s_bayer2x2[2][2] = {
    {   0, 128 },
    { 192,  64 }
};

// 4x4 Bayer matrix
static const uint8_t s_bayer4x4[4][4] = {
    {   0, 128,  32, 160 },
    { 192,  64, 224,  96 },
    {  48, 176,  16, 144 },
    { 240, 112, 208,  80 }
};

// 8x8 Bayer matrix
static const uint8_t s_bayer8x8[8][8] = {
    {   0, 128,  32, 160,   8, 136,  40, 168 },
    { 192,  64, 224,  96, 200,  72, 232, 104 },
    {  48, 176,  16, 144,  56, 184,  24, 152 },
    { 240, 112, 208,  80, 248, 120, 216,  88 },
    {  12, 140,  44, 172,   4, 132,  36, 164 },
    { 204,  76, 236, 108, 196,  68, 228, 100 },
    {  60, 188,  28, 156,  52, 180,  20, 148 },
    { 252, 124, 220,  92, 244, 116, 212,  84 }
};

// =============================================================================
// State
// =============================================================================

static DitherConfig s_config;
static bool s_initialized = false;

// Algorithm names
static const char* s_algorithmNames[] = {
    "None",
    "Threshold",
    "Ordered 2x2",
    "Ordered 4x4",
    "Ordered 8x8",
    "Floyd-Steinberg",
    "Atkinson"
};

// =============================================================================
// Implementation
// =============================================================================

bool dither_init() {
    if (s_initialized) return true;
    
    s_config = DitherConfig();
    s_initialized = true;
    
    return true;
}

void dither_shutdown() {
    s_initialized = false;
}

bool apply_palette_dither(
    const uint8_t* src,
    uint8_t* dst,
    int w,
    int h,
    const Palette& palette
) {
    return apply_palette_dither_ex(src, dst, w, h, palette, s_config);
}

bool apply_palette_dither_ex(
    const uint8_t* src,
    uint8_t* dst,
    int w,
    int h,
    const Palette& palette,
    const DitherConfig& config
) {
    if (!src || !dst || w <= 0 || h <= 0) return false;
    
    switch (config.algorithm) {
        case DitherAlgorithm::NONE:
            // Direct quantization without dithering
            for (int i = 0; i < w * h; i++) {
                dst[i] = palette_map_tone(src[i]);
            }
            break;
            
        case DitherAlgorithm::THRESHOLD:
            // Simple threshold at midpoints
            for (int i = 0; i < w * h; i++) {
                dst[i] = palette_map_tone(src[i]);
            }
            break;
            
        case DitherAlgorithm::ORDERED_2X2:
            dither_ordered(src, dst, w, h, 2, palette);
            break;
            
        case DitherAlgorithm::ORDERED_4X4:
            dither_ordered(src, dst, w, h, 4, palette);
            break;
            
        case DitherAlgorithm::ORDERED_8X8:
            dither_ordered(src, dst, w, h, 8, palette);
            break;
            
        case DitherAlgorithm::FLOYD_STEINBERG:
            dither_floyd_steinberg(src, dst, w, h, palette);
            break;
            
        case DitherAlgorithm::ATKINSON:
            dither_atkinson(src, dst, w, h, palette);
            break;
            
        default:
            return false;
    }
    
    return true;
}

void dither_set_config(const DitherConfig& config) {
    s_config = config;
}

const DitherConfig& dither_get_config() {
    return s_config;
}

void dither_set_algorithm(DitherAlgorithm algo) {
    s_config.algorithm = algo;
}

DitherAlgorithm dither_get_algorithm() {
    return s_config.algorithm;
}

const char* dither_get_algorithm_name(DitherAlgorithm algo) {
    uint8_t idx = static_cast<uint8_t>(algo);
    if (idx < static_cast<uint8_t>(DitherAlgorithm::COUNT)) {
        return s_algorithmNames[idx];
    }
    return "Unknown";
}

// =============================================================================
// Ordered Dithering
// =============================================================================

void dither_ordered(
    const uint8_t* src,
    uint8_t* dst,
    int w,
    int h,
    int matrixSize,
    const Palette& palette
) {
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int idx = y * w + x;
            uint8_t pixel = src[idx];
            
            // Get threshold from Bayer matrix
            uint8_t threshold;
            switch (matrixSize) {
                case 2:
                    threshold = s_bayer2x2[y % 2][x % 2];
                    break;
                case 4:
                    threshold = s_bayer4x4[y % 4][x % 4];
                    break;
                case 8:
                default:
                    threshold = s_bayer8x8[y % 8][x % 8];
                    break;
            }
            
            // Apply ordered dither
            // Scale threshold to palette range
            int adjusted = pixel + (threshold - 128) / 2;
            adjusted = constrain(adjusted, 0, 255);
            
            // Map to nearest palette tone
            dst[idx] = palette_map_tone(adjusted);
        }
    }
}

// =============================================================================
// Floyd-Steinberg Error Diffusion
// =============================================================================

void dither_floyd_steinberg(
    const uint8_t* src,
    uint8_t* dst,
    int w,
    int h,
    const Palette& palette
) {
    // Copy source to destination for in-place processing
    if (src != dst) {
        memcpy(dst, src, w * h);
    }
    
    // Process each pixel
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int idx = y * w + x;
            
            uint8_t oldPixel = dst[idx];
            uint8_t newPixel = palette_map_tone(oldPixel);
            dst[idx] = newPixel;
            
            // Calculate quantization error
            int error = (int)oldPixel - (int)newPixel;
            
            // Distribute error to neighbors (Floyd-Steinberg coefficients)
            // Right: 7/16, Down-Left: 3/16, Down: 5/16, Down-Right: 1/16
            
            if (x + 1 < w) {
                int neighbor = dst[idx + 1] + (error * 7) / 16;
                dst[idx + 1] = constrain(neighbor, 0, 255);
            }
            
            if (y + 1 < h) {
                if (x > 0) {
                    int neighbor = dst[idx + w - 1] + (error * 3) / 16;
                    dst[idx + w - 1] = constrain(neighbor, 0, 255);
                }
                
                int neighbor = dst[idx + w] + (error * 5) / 16;
                dst[idx + w] = constrain(neighbor, 0, 255);
                
                if (x + 1 < w) {
                    neighbor = dst[idx + w + 1] + (error * 1) / 16;
                    dst[idx + w + 1] = constrain(neighbor, 0, 255);
                }
            }
        }
    }
}

// =============================================================================
// Atkinson Dithering
// =============================================================================

void dither_atkinson(
    const uint8_t* src,
    uint8_t* dst,
    int w,
    int h,
    const Palette& palette
) {
    // Copy source to destination for in-place processing
    if (src != dst) {
        memcpy(dst, src, w * h);
    }
    
    // Process each pixel
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int idx = y * w + x;
            
            uint8_t oldPixel = dst[idx];
            uint8_t newPixel = palette_map_tone(oldPixel);
            dst[idx] = newPixel;
            
            // Calculate quantization error (Atkinson uses 1/8 for each)
            int error = ((int)oldPixel - (int)newPixel) / 8;
            
            // Distribute error to 6 neighbors (1/8 each = 6/8 total, 2/8 lost)
            // Pattern:     *  1  1
            //           1  1  1
            //              1
            
            if (x + 1 < w) {
                dst[idx + 1] = constrain((int)dst[idx + 1] + error, 0, 255);
            }
            if (x + 2 < w) {
                dst[idx + 2] = constrain((int)dst[idx + 2] + error, 0, 255);
            }
            
            if (y + 1 < h) {
                if (x > 0) {
                    dst[idx + w - 1] = constrain((int)dst[idx + w - 1] + error, 0, 255);
                }
                dst[idx + w] = constrain((int)dst[idx + w] + error, 0, 255);
                if (x + 1 < w) {
                    dst[idx + w + 1] = constrain((int)dst[idx + w + 1] + error, 0, 255);
                }
            }
            
            if (y + 2 < h) {
                dst[idx + 2 * w] = constrain((int)dst[idx + 2 * w] + error, 0, 255);
            }
        }
    }
}

} // namespace filters
} // namespace pxlcam

#endif // PXLCAM_FEATURE_STYLIZED_CAPTURE
