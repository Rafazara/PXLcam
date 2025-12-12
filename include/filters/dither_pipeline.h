/**
 * @file dither_pipeline.h
 * @brief Dithering Pipeline for PXLcam v1.3.0
 * 
 * Unified dithering system that applies palette-based dithering
 * to grayscale images. Supports multiple algorithms:
 * - Ordered (Bayer) dithering
 * - Floyd-Steinberg error diffusion
 * - Threshold-based quantization
 * 
 * @version 1.3.0
 * @date 2024
 * 
 * Feature Flag: PXLCAM_FEATURE_STYLIZED_CAPTURE
 */

#ifndef PXLCAM_FILTERS_DITHER_PIPELINE_H
#define PXLCAM_FILTERS_DITHER_PIPELINE_H

#include <Arduino.h>
#include <stdint.h>
#include "filters/palette.h"

// Feature gate
#ifndef PXLCAM_FEATURE_STYLIZED_CAPTURE
#define PXLCAM_FEATURE_STYLIZED_CAPTURE 1
#endif

namespace pxlcam {
namespace filters {

// =============================================================================
// Dither Algorithm Types
// =============================================================================

/**
 * @brief Available dithering algorithms
 */
enum class DitherAlgorithm : uint8_t {
    NONE = 0,           ///< No dithering, direct quantization
    THRESHOLD,          ///< Simple threshold-based
    ORDERED_2X2,        ///< 2x2 Bayer matrix
    ORDERED_4X4,        ///< 4x4 Bayer matrix (default)
    ORDERED_8X8,        ///< 8x8 Bayer matrix (highest quality)
    FLOYD_STEINBERG,    ///< Error diffusion (slow but best quality)
    ATKINSON,           ///< Atkinson dithering (Mac classic style)
    
    COUNT
};

// =============================================================================
// Dither Pipeline Configuration
// =============================================================================

/**
 * @brief Configuration for the dither pipeline
 */
struct DitherConfig {
    DitherAlgorithm algorithm;      ///< Dithering algorithm to use
    uint8_t strength;               ///< Dither strength (0-255, 128 = normal)
    bool applyGamma;                ///< Apply gamma correction before dithering
    float gamma;                    ///< Gamma value (default 2.2)
    bool preserveContrast;          ///< Boost contrast before processing
    
    DitherConfig() 
        : algorithm(DitherAlgorithm::ORDERED_4X4)
        , strength(128)
        , applyGamma(false)
        , gamma(2.2f)
        , preserveContrast(true) {}
};

// =============================================================================
// Dither Pipeline Interface
// =============================================================================

/**
 * @brief Initialize dither pipeline
 * 
 * Sets up lookup tables and allocates any required buffers.
 * 
 * @return true on success
 */
bool dither_init();

/**
 * @brief Shutdown dither pipeline
 * 
 * Frees allocated resources.
 */
void dither_shutdown();

/**
 * @brief Apply palette-based dithering to grayscale image
 * 
 * Main dithering function that converts a grayscale image to
 * a palette-quantized output using the selected algorithm.
 * 
 * @param src Source grayscale buffer (w * h bytes)
 * @param dst Destination buffer (w * h bytes)
 * @param w Image width
 * @param h Image height
 * @param palette Palette to use for quantization
 * @return true on success
 * 
 * @note src and dst can be the same buffer for in-place processing
 */
bool apply_palette_dither(
    const uint8_t* src,
    uint8_t* dst,
    int w,
    int h,
    const Palette& palette
);

/**
 * @brief Apply dithering with explicit configuration
 * 
 * @param src Source grayscale buffer
 * @param dst Destination buffer
 * @param w Image width
 * @param h Image height
 * @param palette Palette for quantization
 * @param config Dithering configuration
 * @return true on success
 */
bool apply_palette_dither_ex(
    const uint8_t* src,
    uint8_t* dst,
    int w,
    int h,
    const Palette& palette,
    const DitherConfig& config
);

/**
 * @brief Set global dither configuration
 * 
 * @param config New configuration
 */
void dither_set_config(const DitherConfig& config);

/**
 * @brief Get current dither configuration
 * 
 * @return Reference to current config
 */
const DitherConfig& dither_get_config();

/**
 * @brief Set dithering algorithm
 * 
 * @param algo Algorithm to use
 */
void dither_set_algorithm(DitherAlgorithm algo);

/**
 * @brief Get current algorithm
 * 
 * @return Current dithering algorithm
 */
DitherAlgorithm dither_get_algorithm();

/**
 * @brief Get algorithm name as string
 * 
 * @param algo Algorithm enum
 * @return Human-readable name
 */
const char* dither_get_algorithm_name(DitherAlgorithm algo);

// =============================================================================
// Low-Level Dithering Functions
// =============================================================================

/**
 * @brief Apply ordered dithering with Bayer matrix
 * 
 * @param src Source buffer
 * @param dst Destination buffer
 * @param w Width
 * @param h Height
 * @param matrixSize Bayer matrix size (2, 4, or 8)
 * @param palette Palette for quantization
 */
void dither_ordered(
    const uint8_t* src,
    uint8_t* dst,
    int w,
    int h,
    int matrixSize,
    const Palette& palette
);

/**
 * @brief Apply Floyd-Steinberg error diffusion
 * 
 * @param src Source buffer
 * @param dst Destination buffer
 * @param w Width
 * @param h Height
 * @param palette Palette for quantization
 * 
 * @note Requires temporary buffer allocation (w * 2 bytes)
 */
void dither_floyd_steinberg(
    const uint8_t* src,
    uint8_t* dst,
    int w,
    int h,
    const Palette& palette
);

/**
 * @brief Apply Atkinson dithering
 * 
 * @param src Source buffer
 * @param dst Destination buffer
 * @param w Width
 * @param h Height
 * @param palette Palette for quantization
 */
void dither_atkinson(
    const uint8_t* src,
    uint8_t* dst,
    int w,
    int h,
    const Palette& palette
);

// =============================================================================
// TODOs for v1.3.0 Implementation
// =============================================================================

// TODO: Implement all dithering algorithms
// TODO: Add SIMD optimization for ESP32
// TODO: Implement in-place dithering for memory efficiency
// TODO: Add dither strength adjustment
// TODO: Benchmark all algorithms for performance comparison
// TODO: Add dither preview in menu system

} // namespace filters
} // namespace pxlcam

#endif // PXLCAM_FILTERS_DITHER_PIPELINE_H
