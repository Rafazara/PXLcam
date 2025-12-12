/**
 * @file dither_pipeline.h
 * @brief Official Dithering Pipeline for PXLcam v1.3.0
 * 
 * @details
 * This module provides the core dithering infrastructure for the PXLcam
 * stylized capture pipeline. It implements multiple dithering algorithms
 * optimized for the ESP32-CAM platform.
 * 
 * **Supported Algorithms:**
 * - Ordered Bayer 8×8 (best pattern quality)
 * - Ordered Bayer 4×4 (balanced quality/speed)
 * - Floyd–Steinberg (error diffusion, highest quality)
 * - Atkinson (classic Macintosh style, fast diffusion)
 * 
 * All algorithms work with the palette system defined in palette.h,
 * quantizing grayscale images to 4-tone palettes.
 * 
 * @section usage Usage Example
 * @code
 * #include "filters/dither_pipeline.h"
 * #include "filters/palette.h"
 * 
 * void process_image() {
 *     using namespace pxlcam::filters;
 *     
 *     dither_init();
 *     palette_init();
 *     
 *     const Palette& pal = palette_get(PaletteType::GB_CLASSIC);
 *     
 *     uint8_t src[160 * 120];  // Grayscale input
 *     uint8_t dst[160 * 120];  // Dithered output
 *     
 *     apply_dither(src, dst, 160, 120, pal, DitherAlgorithm::ORDERED_8X8);
 * }
 * @endcode
 * 
 * @section performance Performance Notes
 * 
 * | Algorithm       | Speed   | Quality | Memory    |
 * |-----------------|---------|---------|-----------|
 * | ORDERED_8X8     | Fast    | Good    | None      |
 * | ORDERED_4X4     | Fastest | Fair    | None      |
 * | FLOYD_STEINBERG | Slow    | Best    | 2×width   |
 * | ATKINSON        | Medium  | Good    | 2×width   |
 * 
 * @author PXLcam Team
 * @version 1.3.0
 * @date 2024
 * 
 * @copyright MIT License
 * 
 * Feature Flag: PXLCAM_FEATURE_STYLIZED_CAPTURE
 */

#ifndef PXLCAM_FILTERS_DITHER_PIPELINE_H
#define PXLCAM_FILTERS_DITHER_PIPELINE_H

#include <stdint.h>
#include <stddef.h>
#include "filters/palette.h"

// =============================================================================
// Feature Gate
// =============================================================================

#ifndef PXLCAM_FEATURE_STYLIZED_CAPTURE
#define PXLCAM_FEATURE_STYLIZED_CAPTURE 1
#endif

namespace pxlcam {
namespace filters {

// =============================================================================
// Constants
// =============================================================================

/**
 * @brief Size of the Bayer 8×8 matrix
 */
constexpr uint8_t BAYER_8X8_SIZE = 8;

/**
 * @brief Size of the Bayer 4×4 matrix
 */
constexpr uint8_t BAYER_4X4_SIZE = 4;

/**
 * @brief Maximum image width supported for error diffusion
 * 
 * Error diffusion algorithms require temporary buffers sized by image width.
 * This constant limits memory usage on the ESP32.
 */
constexpr uint16_t DITHER_MAX_WIDTH = 640;

/**
 * @brief Maximum image height supported
 */
constexpr uint16_t DITHER_MAX_HEIGHT = 480;

// =============================================================================
// Dither Algorithm Enumeration
// =============================================================================

/**
 * @brief Available dithering algorithms
 * 
 * @details
 * Each algorithm represents a different trade-off between quality,
 * speed, and visual characteristics.
 * 
 * **Ordered Dithering (ORDERED_8X8, ORDERED_4X4):**
 * Uses a threshold matrix (Bayer pattern) to determine pixel mapping.
 * Fast, produces regular patterns, good for retro aesthetics.
 * 
 * **Error Diffusion (FLOYD_STEINBERG, ATKINSON):**
 * Propagates quantization error to neighboring pixels.
 * Slower, produces organic patterns, higher perceived quality.
 */
enum class DitherAlgorithm : uint8_t {
    /**
     * @brief Ordered dithering with 8×8 Bayer matrix
     * 
     * Highest quality ordered dither with 64 threshold levels.
     * Best for detailed images and smooth gradients.
     */
    ORDERED_8X8 = 0,
    
    /**
     * @brief Ordered dithering with 4×4 Bayer matrix
     * 
     * Balanced quality/speed with 16 threshold levels.
     * Good general-purpose choice, visible but uniform pattern.
     */
    ORDERED_4X4 = 1,
    
    /**
     * @brief Floyd–Steinberg error diffusion
     * 
     * Classic error diffusion algorithm (1976).
     * Distributes error to 4 neighbors with weights 7/16, 3/16, 5/16, 1/16.
     * Highest quality but slowest. Best for photographs.
     */
    FLOYD_STEINBERG = 2,
    
    /**
     * @brief Atkinson dithering
     * 
     * Developed by Bill Atkinson for the original Macintosh (1984).
     * Distributes 6/8 of error to 6 neighbors (1/8 each).
     * Preserves highlights/shadows better, classic Mac look.
     */
    ATKINSON = 3,
    
    /**
     * @brief Sentinel value for algorithm count
     * 
     * @note Do not use as an algorithm index
     */
    COUNT = 4
};

// =============================================================================
// Dither Configuration
// =============================================================================

/**
 * @brief Configuration structure for dithering operations
 * 
 * @details
 * Provides additional control over the dithering process.
 * Use with `apply_dither_ex()` for advanced control.
 */
struct DitherConfig {
    /**
     * @brief Dithering algorithm to use
     */
    DitherAlgorithm algorithm;
    
    /**
     * @brief Dither strength/intensity (0-255)
     * 
     * - 0: Minimal dithering (nearly direct quantization)
     * - 128: Normal dithering (default)
     * - 255: Maximum dithering (exaggerated patterns)
     * 
     * Only affects ordered dithering algorithms.
     */
    uint8_t strength;
    
    /**
     * @brief Enable serpentine (bidirectional) scanning
     * 
     * For error diffusion only. Alternates scan direction
     * on each row to reduce directional artifacts.
     */
    bool serpentine;
    
    /**
     * @brief Default constructor with sensible defaults
     */
    DitherConfig()
        : algorithm(DitherAlgorithm::ORDERED_4X4)
        , strength(128)
        , serpentine(true)
    {}
    
    /**
     * @brief Construct with specific algorithm
     * 
     * @param algo Algorithm to use
     */
    explicit DitherConfig(DitherAlgorithm algo)
        : algorithm(algo)
        , strength(128)
        , serpentine(true)
    {}
};

// =============================================================================
// Public API - Initialization
// =============================================================================

/**
 * @brief Initialize the dithering pipeline
 * 
 * @details
 * Must be called once during system startup before any dithering
 * operations. This function:
 * 
 * 1. Validates Bayer matrices
 * 2. Initializes default configuration
 * 3. Allocates error diffusion buffers (if needed)
 * 
 * Safe to call multiple times (idempotent).
 * 
 * @note Call after `palette_init()` for proper integration.
 * 
 * @code
 * void setup() {
 *     pxlcam::filters::palette_init();
 *     pxlcam::filters::dither_init();
 * }
 * @endcode
 */
void dither_init();

/**
 * @brief Check if dithering pipeline is initialized
 * 
 * @return true if dither_init() has been called successfully
 * @return false if not yet initialized
 */
bool dither_is_initialized();

/**
 * @brief Shutdown dithering pipeline and free resources
 * 
 * @details
 * Releases any dynamically allocated buffers.
 * Call during system shutdown or when dithering is no longer needed.
 */
void dither_shutdown();

// =============================================================================
// Public API - Main Dithering Functions
// =============================================================================

/**
 * @brief Apply dithering to a grayscale image
 * 
 * @details
 * Main dithering function that converts a continuous grayscale image
 * to a palette-quantized output using the specified algorithm.
 * 
 * **Buffer Requirements:**
 * - `src` and `dst` must be at least `w * h` bytes
 * - `src` and `dst` can point to the same buffer (in-place processing)
 * - For error diffusion, `w` must not exceed `DITHER_MAX_WIDTH`
 * 
 * @param src Source grayscale buffer (values 0-255)
 * @param dst Destination buffer for dithered output
 * @param w Image width in pixels
 * @param h Image height in pixels
 * @param palette Palette to quantize against
 * @param algo Dithering algorithm to use
 * 
 * @code
 * const Palette& pal = palette_get(PaletteType::GB_CLASSIC);
 * apply_dither(frame, output, 160, 120, pal, DitherAlgorithm::ORDERED_8X8);
 * @endcode
 * 
 * @warning Behavior is undefined if buffers overlap partially.
 *          Use same pointer for src and dst for in-place processing.
 */
void apply_dither(
    const uint8_t* src,
    uint8_t* dst,
    int w,
    int h,
    const Palette& palette,
    DitherAlgorithm algo
);

/**
 * @brief Apply dithering with extended configuration
 * 
 * @details
 * Advanced dithering function with full configuration control.
 * Use when you need to adjust strength or enable serpentine scanning.
 * 
 * @param src Source grayscale buffer
 * @param dst Destination buffer
 * @param w Image width
 * @param h Image height
 * @param palette Palette to quantize against
 * @param config Dithering configuration
 * 
 * @return true on success
 * @return false if parameters are invalid or buffers are too large
 */
bool apply_dither_ex(
    const uint8_t* src,
    uint8_t* dst,
    int w,
    int h,
    const Palette& palette,
    const DitherConfig& config
);

// =============================================================================
// Public API - Configuration
// =============================================================================

/**
 * @brief Set the global default dithering configuration
 * 
 * @param config New default configuration
 */
void dither_set_config(const DitherConfig& config);

/**
 * @brief Get the current global dithering configuration
 * 
 * @return const DitherConfig& Current configuration
 */
const DitherConfig& dither_get_config();

/**
 * @brief Set the default dithering algorithm
 * 
 * @param algo Algorithm to use by default
 */
void dither_set_algorithm(DitherAlgorithm algo);

/**
 * @brief Get the current default algorithm
 * 
 * @return DitherAlgorithm Current default algorithm
 */
DitherAlgorithm dither_get_algorithm();

// =============================================================================
// Public API - Algorithm Information
// =============================================================================

/**
 * @brief Get human-readable name for an algorithm
 * 
 * @param algo Algorithm enum value
 * @return const char* Algorithm name (e.g., "Ordered 8×8")
 * 
 * @note Returns "Unknown" for invalid enum values
 */
const char* dither_get_algorithm_name(DitherAlgorithm algo);

/**
 * @brief Get total number of available algorithms
 * 
 * @return uint8_t Number of algorithms (excluding COUNT sentinel)
 */
uint8_t dither_get_algorithm_count();

/**
 * @brief Check if an algorithm requires error buffer
 * 
 * @param algo Algorithm to check
 * @return true if algorithm uses error diffusion
 * @return false if algorithm is ordered/threshold based
 */
bool dither_algorithm_uses_error_buffer(DitherAlgorithm algo);

// =============================================================================
// Public API - Validation
// =============================================================================

/**
 * @brief Validate dithering parameters
 * 
 * @param w Image width
 * @param h Image height
 * @param algo Algorithm to use
 * @return true if parameters are valid
 * @return false if dimensions exceed limits or algorithm is invalid
 */
bool dither_validate_params(int w, int h, DitherAlgorithm algo);

// =============================================================================
// Low-Level API - Individual Algorithm Functions
// =============================================================================

/**
 * @brief Apply Bayer 8×8 ordered dithering
 * 
 * @param src Source buffer
 * @param dst Destination buffer
 * @param w Image width
 * @param h Image height
 * @param palette Palette for quantization
 * @param strength Dither strength (0-255, 128 = normal)
 */
void dither_ordered_8x8(
    const uint8_t* src,
    uint8_t* dst,
    int w,
    int h,
    const Palette& palette,
    uint8_t strength = 128
);

/**
 * @brief Apply Bayer 4×4 ordered dithering
 * 
 * @param src Source buffer
 * @param dst Destination buffer
 * @param w Image width
 * @param h Image height
 * @param palette Palette for quantization
 * @param strength Dither strength (0-255, 128 = normal)
 */
void dither_ordered_4x4(
    const uint8_t* src,
    uint8_t* dst,
    int w,
    int h,
    const Palette& palette,
    uint8_t strength = 128
);

/**
 * @brief Apply Floyd–Steinberg error diffusion
 * 
 * @param src Source buffer
 * @param dst Destination buffer
 * @param w Image width
 * @param h Image height
 * @param palette Palette for quantization
 * @param serpentine Enable bidirectional scanning
 * 
 * @note Requires internal error buffer (2 × width × sizeof(int16_t))
 */
void dither_floyd_steinberg(
    const uint8_t* src,
    uint8_t* dst,
    int w,
    int h,
    const Palette& palette,
    bool serpentine = true
);

/**
 * @brief Apply Atkinson dithering
 * 
 * @param src Source buffer
 * @param dst Destination buffer
 * @param w Image width
 * @param h Image height
 * @param palette Palette for quantization
 * @param serpentine Enable bidirectional scanning
 * 
 * @note Requires internal error buffer (3 × width × sizeof(int16_t))
 */
void dither_atkinson(
    const uint8_t* src,
    uint8_t* dst,
    int w,
    int h,
    const Palette& palette,
    bool serpentine = true
);

// =============================================================================
// Debug Utilities
// =============================================================================

#ifdef PXLCAM_DEBUG_DITHER

/**
 * @brief Print dither configuration to Serial
 * 
 * @param config Configuration to print
 */
void dither_debug_print_config(const DitherConfig& config);

/**
 * @brief Print Bayer matrix to Serial (for debugging)
 * 
 * @param size Matrix size (4 or 8)
 */
void dither_debug_print_matrix(uint8_t size);

/**
 * @brief Benchmark all algorithms and print results
 * 
 * @param w Test image width
 * @param h Test image height
 */
void dither_debug_benchmark(int w, int h);

#endif // PXLCAM_DEBUG_DITHER

} // namespace filters
} // namespace pxlcam

#endif // PXLCAM_FILTERS_DITHER_PIPELINE_H
