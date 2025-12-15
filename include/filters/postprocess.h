/**
 * @file postprocess.h
 * @brief Post-Processing Chain for PXLcam v1.3.0
 * 
 * Modular post-processing system that applies a chain of filters
 * to captured images. Filters can be enabled/disabled and reordered
 * at runtime.
 * 
 * Filter chain: Gamma → Contrast → Brightness → Sharpen → Denoise → Dither
 * 
 * @version 1.3.0
 * @date 2024
 * 
 * Feature Flag: PXLCAM_FEATURE_STYLIZED_CAPTURE
 */

#ifndef PXLCAM_FILTERS_POSTPROCESS_H
#define PXLCAM_FILTERS_POSTPROCESS_H

#include <Arduino.h>
#include <stdint.h>
#include <functional>

namespace pxlcam {
namespace filters {

// =============================================================================
// Filter Types
// =============================================================================

/**
 * @brief Available post-processing filters
 */
enum class FilterType : uint8_t {
    GAMMA_CORRECTION = 0,   ///< Gamma curve adjustment
    CONTRAST,               ///< Contrast enhancement
    BRIGHTNESS,             ///< Brightness adjustment
    SHARPEN,                ///< Unsharp mask sharpening
    DENOISE,                ///< Simple noise reduction
    HISTOGRAM_EQ,           ///< Histogram equalization
    VIGNETTE,               ///< Vignette effect
    GRAIN,                  ///< Film grain overlay
    DITHER,                 ///< Palette dithering (from dither_pipeline)
    
    COUNT
};

// =============================================================================
// Filter Parameter Structure
// =============================================================================

/**
 * @brief Parameters for a single filter
 */
struct FilterParams {
    FilterType type;        ///< Filter type
    bool enabled;           ///< Is filter active?
    float strength;         ///< Effect strength (0.0 - 1.0)
    float param1;           ///< Filter-specific parameter 1
    float param2;           ///< Filter-specific parameter 2
    
    FilterParams()
        : type(FilterType::GAMMA_CORRECTION)
        , enabled(false)
        , strength(1.0f)
        , param1(0.0f)
        , param2(0.0f) {}
        
    FilterParams(FilterType t, bool en = true, float str = 1.0f)
        : type(t)
        , enabled(en)
        , strength(str)
        , param1(0.0f)
        , param2(0.0f) {}
};

// =============================================================================
// Post-Process Chain Configuration
// =============================================================================

/// Maximum filters in the chain
constexpr uint8_t MAX_FILTER_CHAIN = 8;

/**
 * @brief Post-processing chain configuration
 */
struct PostProcessConfig {
    FilterParams filters[MAX_FILTER_CHAIN];
    uint8_t filterCount;
    bool enabled;           ///< Global enable/disable
    
    PostProcessConfig() : filterCount(0), enabled(true) {}
};

// =============================================================================
// Post-Process Chain Interface
// =============================================================================

/**
 * @brief Initialize post-processing system
 * 
 * @return true on success
 */
bool postprocess_init();

/**
 * @brief Shutdown post-processing system
 */
void postprocess_shutdown();

/**
 * @brief Apply the complete post-processing chain
 * 
 * Processes the image through all enabled filters in order.
 * 
 * @param buffer Image buffer (grayscale, modified in place)
 * @param w Image width
 * @param h Image height
 * @return true on success
 */
bool apply_postprocess_chain(uint8_t* buffer, int w, int h);

/**
 * @brief Apply single filter to image
 * 
 * @param buffer Image buffer (modified in place)
 * @param w Image width
 * @param h Image height
 * @param params Filter parameters
 * @return true on success
 */
bool apply_filter(uint8_t* buffer, int w, int h, const FilterParams& params);

/**
 * @brief Set the filter chain configuration
 * 
 * @param config New configuration
 */
void postprocess_set_config(const PostProcessConfig& config);

/**
 * @brief Get current filter chain configuration
 * 
 * @return Reference to current config
 */
const PostProcessConfig& postprocess_get_config();

/**
 * @brief Enable/disable specific filter in chain
 * 
 * @param type Filter type to toggle
 * @param enabled New state
 */
void postprocess_set_filter_enabled(FilterType type, bool enabled);

/**
 * @brief Check if filter is enabled
 * 
 * @param type Filter type
 * @return true if enabled
 */
bool postprocess_is_filter_enabled(FilterType type);

/**
 * @brief Set filter strength
 * 
 * @param type Filter type
 * @param strength Strength value (0.0 - 1.0)
 */
void postprocess_set_filter_strength(FilterType type, float strength);

/**
 * @brief Enable/disable entire chain
 * 
 * @param enabled New state
 */
void postprocess_enable(bool enabled);

/**
 * @brief Get filter name as string
 * 
 * @param type Filter type
 * @return Human-readable name
 */
const char* postprocess_get_filter_name(FilterType type);

// =============================================================================
// Individual Filter Functions
// =============================================================================

/**
 * @brief Apply gamma correction
 * 
 * @param buffer Image buffer
 * @param w Width
 * @param h Height
 * @param gamma Gamma value (< 1.0 = brighter, > 1.0 = darker)
 */
void filter_gamma(uint8_t* buffer, int w, int h, float gamma);

/**
 * @brief Apply contrast adjustment
 * 
 * @param buffer Image buffer
 * @param w Width
 * @param h Height
 * @param contrast Contrast multiplier (1.0 = no change)
 */
void filter_contrast(uint8_t* buffer, int w, int h, float contrast);

/**
 * @brief Apply brightness adjustment
 * 
 * @param buffer Image buffer
 * @param w Width
 * @param h Height
 * @param brightness Brightness offset (-128 to +128)
 */
void filter_brightness(uint8_t* buffer, int w, int h, int brightness);

/**
 * @brief Apply sharpening filter
 * 
 * @param buffer Image buffer
 * @param w Width
 * @param h Height
 * @param strength Sharpening strength (0.0 - 1.0)
 */
void filter_sharpen(uint8_t* buffer, int w, int h, float strength);

/**
 * @brief Apply noise reduction
 * 
 * @param buffer Image buffer
 * @param w Width
 * @param h Height
 * @param strength Denoise strength (0.0 - 1.0)
 */
void filter_denoise(uint8_t* buffer, int w, int h, float strength);

/**
 * @brief Apply histogram equalization
 * 
 * @param buffer Image buffer
 * @param w Width
 * @param h Height
 */
void filter_histogram_eq(uint8_t* buffer, int w, int h);

/**
 * @brief Apply vignette effect
 * 
 * @param buffer Image buffer
 * @param w Width
 * @param h Height
 * @param strength Vignette darkness (0.0 - 1.0)
 * @param radius Vignette radius (0.5 - 1.5)
 */
void filter_vignette(uint8_t* buffer, int w, int h, float strength, float radius);

/**
 * @brief Apply film grain overlay
 * 
 * @param buffer Image buffer
 * @param w Width
 * @param h Height
 * @param amount Grain amount (0.0 - 1.0)
 */
void filter_grain(uint8_t* buffer, int w, int h, float amount);

// =============================================================================
// Preset Configurations
// =============================================================================

/**
 * @brief Load preset filter chain
 */
enum class PostProcessPreset : uint8_t {
    NONE,               ///< No post-processing
    GAMEBOY_CLASSIC,    ///< GameBoy-style processing
    VINTAGE_PHOTO,      ///< Old photo look
    HIGH_CONTRAST,      ///< Maximum contrast
    NIGHT_VISION,       ///< Night vision green
    THERMAL,            ///< Thermal camera style
    
    COUNT
};

/**
 * @brief Load a preset configuration
 * 
 * @param preset Preset to load
 */
void postprocess_load_preset(PostProcessPreset preset);

// =============================================================================
// TODOs for v1.3.0 Implementation
// =============================================================================

// TODO: Implement all individual filters
// TODO: Add PSRAM buffer for non-destructive processing
// TODO: Implement filter parameter NVS persistence
// TODO: Add real-time preview of filter effects
// TODO: Benchmark filter chain performance
// TODO: Add custom filter support via function pointer

} // namespace filters
} // namespace pxlcam

#endif // PXLCAM_FILTERS_POSTPROCESS_H
