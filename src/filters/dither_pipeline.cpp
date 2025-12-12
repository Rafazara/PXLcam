/**
 * @file dither_pipeline.cpp
 * @brief Implementation of the PXLcam Dithering Pipeline
 * 
 * @details
 * This file implements the complete dithering system for PXLcam v1.3.0.
 * It provides:
 * - Optimized Bayer 8×8 and 4×4 ordered dithering
 * - Floyd–Steinberg error diffusion
 * - Atkinson dithering
 * - Configuration management
 * 
 * **Optimization Notes:**
 * - Bayer matrices stored in PROGMEM to save RAM
 * - Fixed-point arithmetic for error diffusion (no floats)
 * - Row-by-row processing for cache efficiency
 * - In-place processing supported where possible
 * 
 * @see dither_pipeline.h for public API documentation
 * 
 * @author PXLcam Team
 * @version 1.3.0
 * @date 2024
 * 
 * @copyright MIT License
 */

#include "filters/dither_pipeline.h"
#include <string.h>  // For memcpy, memset

// Only compile if feature is enabled
#if PXLCAM_FEATURE_STYLIZED_CAPTURE

// ESP32 specific includes for PROGMEM
#ifdef ESP32
#include <pgmspace.h>
#else
// Fallback for non-ESP32 platforms (testing)
#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*(const uint8_t*)(addr))
#endif
#endif

namespace pxlcam {
namespace filters {

// =============================================================================
// Bayer Dithering Matrices (PROGMEM)
// =============================================================================

/**
 * @brief Bayer 4×4 threshold matrix
 * 
 * Classic Bayer ordered dither matrix normalized to 0-255 range.
 * Pattern produces characteristic crosshatch appearance.
 * 
 * Original values (0-15) scaled by 16 and offset:
 * value = (original * 16) + 8 - 128 → range -120 to +127
 * 
 * Stored as unsigned with 128 bias for easier arithmetic.
 */
static const uint8_t PROGMEM s_bayer4x4[BAYER_4X4_SIZE][BAYER_4X4_SIZE] = {
    {   0, 128,  32, 160 },
    { 192,  64, 224,  96 },
    {  48, 176,  16, 144 },
    { 240, 112, 208,  80 }
};

/**
 * @brief Bayer 8×8 threshold matrix
 * 
 * Higher resolution Bayer matrix for smoother gradients.
 * 64 distinct threshold levels provide better tonal reproduction.
 * 
 * Normalized to 0-255 range with characteristic ordered pattern.
 */
static const uint8_t PROGMEM s_bayer8x8[BAYER_8X8_SIZE][BAYER_8X8_SIZE] = {
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
// Algorithm Names
// =============================================================================

/**
 * @brief Human-readable algorithm names (matches DitherAlgorithm enum order)
 */
static const char* const s_algorithmNames[] = {
    "Ordered 8x8",       // ORDERED_8X8
    "Ordered 4x4",       // ORDERED_4X4
    "Floyd-Steinberg",   // FLOYD_STEINBERG
    "Atkinson"           // ATKINSON
};

// =============================================================================
// Internal State
// =============================================================================

/**
 * @brief Initialization flag
 */
static bool s_initialized = false;

/**
 * @brief Global default configuration
 */
static DitherConfig s_config;

/**
 * @brief Error diffusion buffer for current row
 * 
 * Stores accumulated error values as signed 16-bit.
 * Using int16_t to handle overflow during error propagation.
 */
static int16_t s_errorRow0[DITHER_MAX_WIDTH + 4];

/**
 * @brief Error diffusion buffer for next row
 */
static int16_t s_errorRow1[DITHER_MAX_WIDTH + 4];

/**
 * @brief Error diffusion buffer for row+2 (Atkinson only)
 */
static int16_t s_errorRow2[DITHER_MAX_WIDTH + 4];

// =============================================================================
// Internal Helper Functions
// =============================================================================

/**
 * @brief Clamp value to uint8_t range
 * 
 * @param value Value to clamp (can be negative or > 255)
 * @return uint8_t Clamped value [0, 255]
 */
static inline uint8_t clamp_u8(int value) {
    if (value < 0) return 0;
    if (value > 255) return 255;
    return static_cast<uint8_t>(value);
}

/**
 * @brief Convert RGB888 to luminance using ITU-R BT.601
 * 
 * Y = 0.299R + 0.587G + 0.114B
 * Using fixed-point: (77*R + 150*G + 29*B) >> 8
 * 
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @return uint8_t Luminance value (0-255)
 */
static inline uint8_t rgb_to_luma(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint8_t>((77 * r + 150 * g + 29 * b) >> 8);
}

/**
 * @brief Convert RGB565 to luminance
 * 
 * @param rgb565 Packed RGB565 value (5-6-5 bits)
 * @return uint8_t Luminance value (0-255)
 */
static inline uint8_t rgb565_to_luma(uint16_t rgb565) {
    // Extract components and scale to 8-bit
    uint8_t r = ((rgb565 >> 11) & 0x1F) << 3;  // 5 bits → 8 bits
    uint8_t g = ((rgb565 >> 5) & 0x3F) << 2;   // 6 bits → 8 bits
    uint8_t b = (rgb565 & 0x1F) << 3;          // 5 bits → 8 bits
    return rgb_to_luma(r, g, b);
}

/**
 * @brief Find the closest tone index for a given luminance value
 * 
 * Uses minimum distance comparison to find the best matching
 * palette tone. Returns index (0-3) not the tone value.
 * 
 * @param luma Input luminance (0-255)
 * @param palette Palette to search
 * @return uint8_t Tone index (0-3)
 */
static inline uint8_t find_closest_tone_index(uint8_t luma, const Palette& palette) {
    uint8_t bestIndex = 0;
    int bestDist = 256;  // Max possible distance + 1
    
    for (uint8_t i = 0; i < PALETTE_TONE_COUNT; ++i) {
        int dist = static_cast<int>(luma) - static_cast<int>(palette.tones[i]);
        if (dist < 0) dist = -dist;  // abs()
        
        if (dist < bestDist) {
            bestDist = dist;
            bestIndex = i;
        }
    }
    
    return bestIndex;
}

/**
 * @brief Apply strength scaling to threshold offset
 * 
 * @param threshold Raw threshold from Bayer matrix (0-255)
 * @param strength Dither strength (0-255, 128 = normal)
 * @return int Scaled threshold offset for addition to pixel value
 */
static inline int scale_threshold(uint8_t threshold, uint8_t strength) {
    // Convert threshold to signed offset (-128 to +127)
    int offset = static_cast<int>(threshold) - 128;
    
    // Scale by strength (strength/128 factor)
    // Using fixed-point: (offset * strength) >> 7
    offset = (offset * static_cast<int>(strength)) >> 7;
    
    return offset;
}

/**
 * @brief Clear error diffusion buffers
 * 
 * @param width Image width (determines clear length)
 * @param includeRow2 Also clear row2 buffer (for Atkinson)
 */
static void clear_error_buffers(int width, bool includeRow2 = false) {
    // Add padding on both sides for boundary handling
    const int bufLen = width + 4;
    memset(s_errorRow0, 0, bufLen * sizeof(int16_t));
    memset(s_errorRow1, 0, bufLen * sizeof(int16_t));
    if (includeRow2) {
        memset(s_errorRow2, 0, bufLen * sizeof(int16_t));
    }
}

/**
 * @brief Rotate error buffers down one row
 * 
 * @param width Image width
 * @param threeRows Also rotate row2 (for Atkinson)
 */
static void rotate_error_buffers(int width, bool threeRows = false) {
    const int bufLen = width + 4;
    
    if (threeRows) {
        // Row0 = Row1, Row1 = Row2, Row2 = 0
        memcpy(s_errorRow0, s_errorRow1, bufLen * sizeof(int16_t));
        memcpy(s_errorRow1, s_errorRow2, bufLen * sizeof(int16_t));
        memset(s_errorRow2, 0, bufLen * sizeof(int16_t));
    } else {
        // Row0 = Row1, Row1 = 0
        memcpy(s_errorRow0, s_errorRow1, bufLen * sizeof(int16_t));
        memset(s_errorRow1, 0, bufLen * sizeof(int16_t));
    }
}

// =============================================================================
// Public API - Initialization
// =============================================================================

void dither_init() {
    if (s_initialized) {
        return;  // Idempotent
    }
    
    // Initialize default configuration
    s_config = DitherConfig();
    
    // Clear error buffers
    clear_error_buffers(DITHER_MAX_WIDTH, true);
    
    s_initialized = true;
}

bool dither_is_initialized() {
    return s_initialized;
}

void dither_shutdown() {
    if (!s_initialized) {
        return;
    }
    
    // Clear buffers for security
    clear_error_buffers(DITHER_MAX_WIDTH, true);
    
    s_initialized = false;
}

// =============================================================================
// Public API - Main Dithering Functions
// =============================================================================

void apply_dither(
    const uint8_t* src,
    uint8_t* dst,
    int w,
    int h,
    const Palette& palette,
    DitherAlgorithm algo
) {
    DitherConfig config(algo);
    apply_dither_ex(src, dst, w, h, palette, config);
}

bool apply_dither_ex(
    const uint8_t* src,
    uint8_t* dst,
    int w,
    int h,
    const Palette& palette,
    const DitherConfig& config
) {
    // Validate parameters
    if (!dither_validate_params(w, h, config.algorithm)) {
        return false;
    }
    
    if (src == nullptr || dst == nullptr) {
        return false;
    }
    
    // Dispatch to appropriate algorithm
    switch (config.algorithm) {
        case DitherAlgorithm::ORDERED_8X8:
            dither_ordered_8x8(src, dst, w, h, palette, config.strength);
            break;
            
        case DitherAlgorithm::ORDERED_4X4:
            dither_ordered_4x4(src, dst, w, h, palette, config.strength);
            break;
            
        case DitherAlgorithm::FLOYD_STEINBERG:
            dither_floyd_steinberg(src, dst, w, h, palette, config.serpentine);
            break;
            
        case DitherAlgorithm::ATKINSON:
            dither_atkinson(src, dst, w, h, palette, config.serpentine);
            break;
            
        default:
            return false;
    }
    
    return true;
}

// =============================================================================
// Public API - Configuration
// =============================================================================

void dither_set_config(const DitherConfig& config) {
    s_config = config;
}

const DitherConfig& dither_get_config() {
    return s_config;
}

void dither_set_algorithm(DitherAlgorithm algo) {
    if (static_cast<uint8_t>(algo) < static_cast<uint8_t>(DitherAlgorithm::COUNT)) {
        s_config.algorithm = algo;
    }
}

DitherAlgorithm dither_get_algorithm() {
    return s_config.algorithm;
}

// =============================================================================
// Public API - Algorithm Information
// =============================================================================

const char* dither_get_algorithm_name(DitherAlgorithm algo) {
    const uint8_t idx = static_cast<uint8_t>(algo);
    if (idx < static_cast<uint8_t>(DitherAlgorithm::COUNT)) {
        return s_algorithmNames[idx];
    }
    return "Unknown";
}

uint8_t dither_get_algorithm_count() {
    return static_cast<uint8_t>(DitherAlgorithm::COUNT);
}

bool dither_algorithm_uses_error_buffer(DitherAlgorithm algo) {
    return (algo == DitherAlgorithm::FLOYD_STEINBERG || 
            algo == DitherAlgorithm::ATKINSON);
}

// =============================================================================
// Public API - Validation
// =============================================================================

bool dither_validate_params(int w, int h, DitherAlgorithm algo) {
    // Check dimensions
    if (w <= 0 || h <= 0) {
        return false;
    }
    
    if (w > DITHER_MAX_WIDTH || h > DITHER_MAX_HEIGHT) {
        return false;
    }
    
    // Check algorithm validity
    if (static_cast<uint8_t>(algo) >= static_cast<uint8_t>(DitherAlgorithm::COUNT)) {
        return false;
    }
    
    return true;
}

// =============================================================================
// Low-Level API - Ordered Dithering 8×8
// =============================================================================

void dither_ordered_8x8(
    const uint8_t* src,
    uint8_t* dst,
    int w,
    int h,
    const Palette& palette,
    uint8_t strength
) {
    // Process row by row for cache efficiency
    for (int y = 0; y < h; ++y) {
        // Precompute row offset in source/dest
        const int rowOffset = y * w;
        
        // Precompute y modulo for Bayer lookup
        const int yMod = y & 7;  // y % 8 using bitmask
        
        for (int x = 0; x < w; ++x) {
            // Get source pixel
            const uint8_t srcPixel = src[rowOffset + x];
            
            // Get threshold from Bayer matrix (PROGMEM read)
            const int xMod = x & 7;  // x % 8 using bitmask
            const uint8_t threshold = pgm_read_byte(&s_bayer8x8[yMod][xMod]);
            
            // Apply threshold offset scaled by strength
            const int offset = scale_threshold(threshold, strength);
            const int adjusted = static_cast<int>(srcPixel) + offset;
            
            // Clamp and map to palette
            const uint8_t clamped = clamp_u8(adjusted);
            dst[rowOffset + x] = palette_map_value(clamped, palette);
        }
    }
}

// =============================================================================
// Low-Level API - Ordered Dithering 4×4
// =============================================================================

void dither_ordered_4x4(
    const uint8_t* src,
    uint8_t* dst,
    int w,
    int h,
    const Palette& palette,
    uint8_t strength
) {
    // Process row by row for cache efficiency
    for (int y = 0; y < h; ++y) {
        const int rowOffset = y * w;
        const int yMod = y & 3;  // y % 4 using bitmask
        
        for (int x = 0; x < w; ++x) {
            const uint8_t srcPixel = src[rowOffset + x];
            
            // Get threshold from Bayer matrix
            const int xMod = x & 3;  // x % 4 using bitmask
            const uint8_t threshold = pgm_read_byte(&s_bayer4x4[yMod][xMod]);
            
            // Apply threshold offset scaled by strength
            const int offset = scale_threshold(threshold, strength);
            const int adjusted = static_cast<int>(srcPixel) + offset;
            
            // Clamp and map to palette
            const uint8_t clamped = clamp_u8(adjusted);
            dst[rowOffset + x] = palette_map_value(clamped, palette);
        }
    }
}

// =============================================================================
// Low-Level API - Floyd–Steinberg Error Diffusion
// =============================================================================

/**
 * @brief Floyd–Steinberg error distribution weights
 * 
 * Classic FS pattern:
 *       * 7/16
 * 3/16 5/16 1/16
 * 
 * Using fixed-point with denominator 16 for integer math.
 */
void dither_floyd_steinberg(
    const uint8_t* src,
    uint8_t* dst,
    int w,
    int h,
    const Palette& palette,
    bool serpentine
) {
    // Clear error buffers
    clear_error_buffers(w, false);
    
    // Error buffer index offset (2 pixels padding on left)
    const int errOffset = 2;
    
    for (int y = 0; y < h; ++y) {
        const int rowOffset = y * w;
        
        // Determine scan direction for serpentine
        const bool leftToRight = !serpentine || ((y & 1) == 0);
        
        const int xStart = leftToRight ? 0 : (w - 1);
        const int xEnd = leftToRight ? w : -1;
        const int xStep = leftToRight ? 1 : -1;
        
        for (int x = xStart; x != xEnd; x += xStep) {
            // Get source pixel with accumulated error
            int pixel = static_cast<int>(src[rowOffset + x]) + s_errorRow0[x + errOffset];
            pixel = clamp_u8(pixel);
            
            // Quantize to nearest palette tone
            const uint8_t quantized = palette_map_value(static_cast<uint8_t>(pixel), palette);
            dst[rowOffset + x] = quantized;
            
            // Calculate quantization error
            const int error = pixel - static_cast<int>(quantized);
            
            // Distribute error to neighbors using fixed-point arithmetic
            // Floyd-Steinberg weights: 7/16, 3/16, 5/16, 1/16
            
            if (leftToRight) {
                // Standard left-to-right distribution
                //       * 7/16
                // 3/16 5/16 1/16
                
                // Right neighbor (same row): 7/16
                s_errorRow0[x + errOffset + 1] += (error * 7) >> 4;
                
                // Bottom-left neighbor: 3/16
                s_errorRow1[x + errOffset - 1] += (error * 3) >> 4;
                
                // Bottom neighbor: 5/16
                s_errorRow1[x + errOffset] += (error * 5) >> 4;
                
                // Bottom-right neighbor: 1/16
                s_errorRow1[x + errOffset + 1] += (error * 1) >> 4;
            } else {
                // Right-to-left (mirrored) distribution
                // 7/16 *
                // 1/16 5/16 3/16
                
                // Left neighbor (same row): 7/16
                s_errorRow0[x + errOffset - 1] += (error * 7) >> 4;
                
                // Bottom-right neighbor: 3/16
                s_errorRow1[x + errOffset + 1] += (error * 3) >> 4;
                
                // Bottom neighbor: 5/16
                s_errorRow1[x + errOffset] += (error * 5) >> 4;
                
                // Bottom-left neighbor: 1/16
                s_errorRow1[x + errOffset - 1] += (error * 1) >> 4;
            }
        }
        
        // Rotate error buffers for next row
        rotate_error_buffers(w, false);
    }
}

// =============================================================================
// Low-Level API - Atkinson Dithering
// =============================================================================

/**
 * @brief Atkinson error distribution pattern
 * 
 * Bill Atkinson's pattern (1984):
 *     * 1/8 1/8
 * 1/8 1/8 1/8
 *     1/8
 * 
 * Total: 6/8 = 75% of error propagated (25% discarded)
 * This gives more contrast than Floyd-Steinberg.
 */
void dither_atkinson(
    const uint8_t* src,
    uint8_t* dst,
    int w,
    int h,
    const Palette& palette,
    bool serpentine
) {
    // Clear error buffers (including row2 for Atkinson)
    clear_error_buffers(w, true);
    
    // Error buffer index offset
    const int errOffset = 2;
    
    for (int y = 0; y < h; ++y) {
        const int rowOffset = y * w;
        
        // Determine scan direction
        const bool leftToRight = !serpentine || ((y & 1) == 0);
        
        const int xStart = leftToRight ? 0 : (w - 1);
        const int xEnd = leftToRight ? w : -1;
        const int xStep = leftToRight ? 1 : -1;
        
        for (int x = xStart; x != xEnd; x += xStep) {
            // Get source pixel with accumulated error
            int pixel = static_cast<int>(src[rowOffset + x]) + s_errorRow0[x + errOffset];
            pixel = clamp_u8(pixel);
            
            // Quantize to nearest palette tone
            const uint8_t quantized = palette_map_value(static_cast<uint8_t>(pixel), palette);
            dst[rowOffset + x] = quantized;
            
            // Calculate quantization error
            // Atkinson uses 1/8 for each of 6 neighbors (6/8 total)
            const int error = pixel - static_cast<int>(quantized);
            const int errFrac = error >> 3;  // error / 8
            
            if (leftToRight) {
                // Standard left-to-right distribution
                //     * 1/8 1/8
                // 1/8 1/8 1/8
                //     1/8
                
                // Same row: x+1, x+2
                s_errorRow0[x + errOffset + 1] += errFrac;
                s_errorRow0[x + errOffset + 2] += errFrac;
                
                // Next row: x-1, x, x+1
                s_errorRow1[x + errOffset - 1] += errFrac;
                s_errorRow1[x + errOffset] += errFrac;
                s_errorRow1[x + errOffset + 1] += errFrac;
                
                // Row after next: x
                s_errorRow2[x + errOffset] += errFrac;
            } else {
                // Right-to-left (mirrored) distribution
                // 1/8 1/8 *
                //     1/8 1/8 1/8
                //         1/8
                
                // Same row: x-1, x-2
                s_errorRow0[x + errOffset - 1] += errFrac;
                s_errorRow0[x + errOffset - 2] += errFrac;
                
                // Next row: x+1, x, x-1
                s_errorRow1[x + errOffset + 1] += errFrac;
                s_errorRow1[x + errOffset] += errFrac;
                s_errorRow1[x + errOffset - 1] += errFrac;
                
                // Row after next: x
                s_errorRow2[x + errOffset] += errFrac;
            }
        }
        
        // Rotate error buffers for next row
        rotate_error_buffers(w, true);
    }
}

// =============================================================================
// Primary API - apply_palette_dither()
// =============================================================================

/**
 * @brief Static grayscale buffer for format conversion
 * 
 * Used when input is RGB and needs conversion to grayscale before dithering.
 * Size: DITHER_MAX_WIDTH * DITHER_MAX_HEIGHT = 307,200 bytes
 * 
 * TODO v1.3.1: Consider PSRAM allocation for larger buffers
 */
static uint8_t s_grayBuffer[DITHER_MAX_WIDTH * 4];  // 4 rows for streaming conversion

/**
 * @brief Convert source buffer to grayscale row by row
 * 
 * @param src Source buffer
 * @param srcFormat Source format
 * @param grayRow Output grayscale row buffer
 * @param row Row index
 * @param w Image width
 */
static void convert_row_to_gray(
    const uint8_t* src,
    SourceFormat srcFormat,
    uint8_t* grayRow,
    int row,
    int w
) {
    switch (srcFormat) {
        case SourceFormat::GRAYSCALE:
            // Direct copy
            memcpy(grayRow, src + row * w, w);
            break;
            
        case SourceFormat::RGB888: {
            const uint8_t* srcRow = src + row * w * 3;
            for (int x = 0; x < w; ++x) {
                const uint8_t* p = srcRow + x * 3;
                grayRow[x] = rgb_to_luma(p[0], p[1], p[2]);
            }
            break;
        }
        
        case SourceFormat::RGB565: {
            const uint16_t* srcRow = reinterpret_cast<const uint16_t*>(src) + row * w;
            for (int x = 0; x < w; ++x) {
                grayRow[x] = rgb565_to_luma(srcRow[x]);
            }
            break;
        }
    }
}

/**
 * @brief Internal ordered dithering with index output
 * 
 * Same as dither_ordered_8x8 but outputs tone indices instead of values.
 */
static void ordered_8x8_to_indices(
    const uint8_t* src,
    SourceFormat srcFormat,
    uint8_t* dstIndices,
    int w,
    int h,
    const Palette& palette,
    uint8_t strength
) {
    // Row buffer for format conversion
    uint8_t rowBuf[DITHER_MAX_WIDTH];
    
    for (int y = 0; y < h; ++y) {
        // Convert row to grayscale if needed
        const uint8_t* grayRow;
        if (srcFormat == SourceFormat::GRAYSCALE) {
            grayRow = src + y * w;
        } else {
            convert_row_to_gray(src, srcFormat, rowBuf, y, w);
            grayRow = rowBuf;
        }
        
        const int rowOffset = y * w;
        const int yMod = y & 7;
        
        for (int x = 0; x < w; ++x) {
            const uint8_t srcPixel = grayRow[x];
            const int xMod = x & 7;
            const uint8_t threshold = pgm_read_byte(&s_bayer8x8[yMod][xMod]);
            
            const int offset = scale_threshold(threshold, strength);
            const int adjusted = static_cast<int>(srcPixel) + offset;
            const uint8_t clamped = clamp_u8(adjusted);
            
            // Output tone INDEX (0-3) instead of tone value
            dstIndices[rowOffset + x] = find_closest_tone_index(clamped, palette);
        }
    }
}

/**
 * @brief Internal ordered 4x4 dithering with index output
 */
static void ordered_4x4_to_indices(
    const uint8_t* src,
    SourceFormat srcFormat,
    uint8_t* dstIndices,
    int w,
    int h,
    const Palette& palette,
    uint8_t strength
) {
    uint8_t rowBuf[DITHER_MAX_WIDTH];
    
    for (int y = 0; y < h; ++y) {
        const uint8_t* grayRow;
        if (srcFormat == SourceFormat::GRAYSCALE) {
            grayRow = src + y * w;
        } else {
            convert_row_to_gray(src, srcFormat, rowBuf, y, w);
            grayRow = rowBuf;
        }
        
        const int rowOffset = y * w;
        const int yMod = y & 3;
        
        for (int x = 0; x < w; ++x) {
            const uint8_t srcPixel = grayRow[x];
            const int xMod = x & 3;
            const uint8_t threshold = pgm_read_byte(&s_bayer4x4[yMod][xMod]);
            
            const int offset = scale_threshold(threshold, strength);
            const int adjusted = static_cast<int>(srcPixel) + offset;
            const uint8_t clamped = clamp_u8(adjusted);
            
            dstIndices[rowOffset + x] = find_closest_tone_index(clamped, palette);
        }
    }
}

/**
 * @brief Internal Floyd-Steinberg with index output
 */
static void floyd_steinberg_to_indices(
    const uint8_t* src,
    SourceFormat srcFormat,
    uint8_t* dstIndices,
    int w,
    int h,
    const Palette& palette,
    bool serpentine
) {
    // Convert entire image to grayscale first for error diffusion
    // (Error diffusion needs access to neighboring pixels)
    static uint8_t s_fullGrayBuf[DITHER_MAX_WIDTH * DITHER_MAX_HEIGHT];
    
    // Convert all rows
    for (int y = 0; y < h; ++y) {
        convert_row_to_gray(src, srcFormat, s_fullGrayBuf + y * w, y, w);
    }
    
    // Clear error buffers
    clear_error_buffers(w, false);
    
    const int errOffset = 2;
    
    for (int y = 0; y < h; ++y) {
        const int rowOffset = y * w;
        
        const bool leftToRight = !serpentine || ((y & 1) == 0);
        
        const int xStart = leftToRight ? 0 : (w - 1);
        const int xEnd = leftToRight ? w : -1;
        const int xStep = leftToRight ? 1 : -1;
        
        for (int x = xStart; x != xEnd; x += xStep) {
            int pixel = static_cast<int>(s_fullGrayBuf[rowOffset + x]) + s_errorRow0[x + errOffset];
            pixel = clamp_u8(pixel);
            
            // Find closest tone INDEX
            const uint8_t toneIndex = find_closest_tone_index(static_cast<uint8_t>(pixel), palette);
            dstIndices[rowOffset + x] = toneIndex;
            
            // Get the actual tone value for error calculation
            const uint8_t quantized = palette.tones[toneIndex];
            const int error = pixel - static_cast<int>(quantized);
            
            // Floyd-Steinberg error distribution
            if (leftToRight) {
                s_errorRow0[x + errOffset + 1] += (error * 7) >> 4;
                s_errorRow1[x + errOffset - 1] += (error * 3) >> 4;
                s_errorRow1[x + errOffset] += (error * 5) >> 4;
                s_errorRow1[x + errOffset + 1] += (error * 1) >> 4;
            } else {
                s_errorRow0[x + errOffset - 1] += (error * 7) >> 4;
                s_errorRow1[x + errOffset + 1] += (error * 3) >> 4;
                s_errorRow1[x + errOffset] += (error * 5) >> 4;
                s_errorRow1[x + errOffset - 1] += (error * 1) >> 4;
            }
        }
        
        rotate_error_buffers(w, false);
    }
}

/**
 * @brief Internal Atkinson with index output
 */
static void atkinson_to_indices(
    const uint8_t* src,
    SourceFormat srcFormat,
    uint8_t* dstIndices,
    int w,
    int h,
    const Palette& palette,
    bool serpentine
) {
    // Convert entire image to grayscale first
    static uint8_t s_fullGrayBuf[DITHER_MAX_WIDTH * DITHER_MAX_HEIGHT];
    
    for (int y = 0; y < h; ++y) {
        convert_row_to_gray(src, srcFormat, s_fullGrayBuf + y * w, y, w);
    }
    
    // Clear error buffers
    clear_error_buffers(w, true);
    
    const int errOffset = 2;
    
    for (int y = 0; y < h; ++y) {
        const int rowOffset = y * w;
        
        const bool leftToRight = !serpentine || ((y & 1) == 0);
        
        const int xStart = leftToRight ? 0 : (w - 1);
        const int xEnd = leftToRight ? w : -1;
        const int xStep = leftToRight ? 1 : -1;
        
        for (int x = xStart; x != xEnd; x += xStep) {
            int pixel = static_cast<int>(s_fullGrayBuf[rowOffset + x]) + s_errorRow0[x + errOffset];
            pixel = clamp_u8(pixel);
            
            const uint8_t toneIndex = find_closest_tone_index(static_cast<uint8_t>(pixel), palette);
            dstIndices[rowOffset + x] = toneIndex;
            
            const uint8_t quantized = palette.tones[toneIndex];
            const int error = pixel - static_cast<int>(quantized);
            const int errFrac = error >> 3;  // error / 8
            
            // Atkinson distribution (6 neighbors, 1/8 each)
            if (leftToRight) {
                s_errorRow0[x + errOffset + 1] += errFrac;
                s_errorRow0[x + errOffset + 2] += errFrac;
                s_errorRow1[x + errOffset - 1] += errFrac;
                s_errorRow1[x + errOffset] += errFrac;
                s_errorRow1[x + errOffset + 1] += errFrac;
                s_errorRow2[x + errOffset] += errFrac;
            } else {
                s_errorRow0[x + errOffset - 1] += errFrac;
                s_errorRow0[x + errOffset - 2] += errFrac;
                s_errorRow1[x + errOffset + 1] += errFrac;
                s_errorRow1[x + errOffset] += errFrac;
                s_errorRow1[x + errOffset - 1] += errFrac;
                s_errorRow2[x + errOffset] += errFrac;
            }
        }
        
        rotate_error_buffers(w, true);
    }
}

DitherResult apply_palette_dither(
    const uint8_t* src,
    SourceFormat srcFormat,
    uint8_t* dst,
    int w,
    int h,
    const Palette& palette,
    DitherAlgorithm algo
) {
    DitherConfig config(algo);
    return apply_palette_dither_ex(src, srcFormat, dst, w, h, palette, config);
}

DitherResult apply_palette_dither_ex(
    const uint8_t* src,
    SourceFormat srcFormat,
    uint8_t* dst,
    int w,
    int h,
    const Palette& palette,
    const DitherConfig& config
) {
    // Validate parameters
    if (src == nullptr) {
        return DitherResult::Error("src is null");
    }
    if (dst == nullptr) {
        return DitherResult::Error("dst is null");
    }
    if (w <= 0 || h <= 0) {
        return DitherResult::Error("Invalid dimensions");
    }
    if (w > DITHER_MAX_WIDTH) {
        return DitherResult::Error("Width exceeds max");
    }
    if (h > DITHER_MAX_HEIGHT) {
        return DitherResult::Error("Height exceeds max");
    }
    if (static_cast<uint8_t>(config.algorithm) >= static_cast<uint8_t>(DitherAlgorithm::COUNT)) {
        return DitherResult::Error("Invalid algorithm");
    }
    
    // Dispatch to appropriate index-output algorithm
    switch (config.algorithm) {
        case DitherAlgorithm::ORDERED_8X8:
            ordered_8x8_to_indices(src, srcFormat, dst, w, h, palette, config.strength);
            break;
            
        case DitherAlgorithm::ORDERED_4X4:
            ordered_4x4_to_indices(src, srcFormat, dst, w, h, palette, config.strength);
            break;
            
        case DitherAlgorithm::FLOYD_STEINBERG:
            floyd_steinberg_to_indices(src, srcFormat, dst, w, h, palette, config.serpentine);
            break;
            
        case DitherAlgorithm::ATKINSON:
            atkinson_to_indices(src, srcFormat, dst, w, h, palette, config.serpentine);
            break;
            
        default:
            return DitherResult::Error("Unsupported algorithm");
    }
    
    return DitherResult::Ok(static_cast<uint32_t>(w * h));
}

// =============================================================================
// Utility Functions
// =============================================================================

void indices_to_grayscale(
    const uint8_t* indices,
    uint8_t* grayOut,
    size_t length,
    const Palette& palette
) {
    if (!indices || !grayOut || length == 0) return;
    
    for (size_t i = 0; i < length; ++i) {
        // Ensure index is valid (0-3)
        uint8_t idx = indices[i] & 0x03;
        grayOut[i] = palette.tones[idx];
    }
}

uint8_t source_format_bpp(SourceFormat format) {
    switch (format) {
        case SourceFormat::GRAYSCALE: return 1;
        case SourceFormat::RGB565:    return 2;
        case SourceFormat::RGB888:    return 3;
        default:                      return 1;
    }
}

const char* source_format_name(SourceFormat format) {
    switch (format) {
        case SourceFormat::GRAYSCALE: return "Grayscale";
        case SourceFormat::RGB565:    return "RGB565";
        case SourceFormat::RGB888:    return "RGB888";
        default:                      return "Unknown";
    }
}

// =============================================================================
// Debug Utilities
// =============================================================================

#ifdef PXLCAM_DEBUG_DITHER

#include <Arduino.h>  // For Serial

void dither_debug_print_config(const DitherConfig& config) {
    Serial.println(F("=== DITHER CONFIG ==="));
    Serial.printf("  Algorithm: %s (%u)\n", 
        dither_get_algorithm_name(config.algorithm),
        static_cast<uint8_t>(config.algorithm));
    Serial.printf("  Strength: %u\n", config.strength);
    Serial.printf("  Serpentine: %s\n", config.serpentine ? "Yes" : "No");
    Serial.println(F("====================="));
}

void dither_debug_print_matrix(uint8_t size) {
    Serial.printf("=== BAYER %ux%u MATRIX ===\n", size, size);
    
    if (size == 4) {
        for (int y = 0; y < 4; ++y) {
            Serial.print("  ");
            for (int x = 0; x < 4; ++x) {
                Serial.printf("%3u ", pgm_read_byte(&s_bayer4x4[y][x]));
            }
            Serial.println();
        }
    } else if (size == 8) {
        for (int y = 0; y < 8; ++y) {
            Serial.print("  ");
            for (int x = 0; x < 8; ++x) {
                Serial.printf("%3u ", pgm_read_byte(&s_bayer8x8[y][x]));
            }
            Serial.println();
        }
    }
    
    Serial.println(F("========================="));
}

void dither_debug_benchmark(int w, int h) {
    // Allocate test buffers
    const int size = w * h;
    uint8_t* src = (uint8_t*)malloc(size);
    uint8_t* dst = (uint8_t*)malloc(size);
    
    if (!src || !dst) {
        Serial.println(F("[DITHER] Benchmark: Allocation failed"));
        free(src);
        free(dst);
        return;
    }
    
    // Generate gradient test pattern
    for (int i = 0; i < size; ++i) {
        src[i] = (i * 256) / size;
    }
    
    // Get default palette
    const Palette& pal = palette_get(PaletteType::GB_CLASSIC);
    
    Serial.println(F("=== DITHER BENCHMARK ==="));
    Serial.printf("  Image: %dx%d (%d bytes)\n", w, h, size);
    Serial.println();
    
    // Benchmark each algorithm
    for (uint8_t i = 0; i < static_cast<uint8_t>(DitherAlgorithm::COUNT); ++i) {
        DitherAlgorithm algo = static_cast<DitherAlgorithm>(i);
        
        unsigned long start = micros();
        apply_dither(src, dst, w, h, pal, algo);
        unsigned long elapsed = micros() - start;
        
        float fps = 1000000.0f / elapsed;
        
        Serial.printf("  %-16s: %6lu us (%.1f FPS)\n",
            dither_get_algorithm_name(algo), elapsed, fps);
    }
    
    Serial.println(F("========================"));
    
    free(src);
    free(dst);
}

#endif // PXLCAM_DEBUG_DITHER

} // namespace filters
} // namespace pxlcam

#endif // PXLCAM_FEATURE_STYLIZED_CAPTURE
