/**
 * @file postprocess.cpp
 * @brief Post-Processing Chain implementation for PXLcam v1.3.0
 * 
 * @version 1.3.0
 * @date 2024
 */

#include "filters/postprocess.h"
#include <cmath>

namespace pxlcam {
namespace filters {

// =============================================================================
// State
// =============================================================================

static PostProcessConfig s_config;
static bool s_initialized = false;

// Filter names
static const char* s_filterNames[] = {
    "Gamma",
    "Contrast",
    "Brightness",
    "Sharpen",
    "Denoise",
    "Histogram EQ",
    "Vignette",
    "Grain",
    "Dither"
};

// Gamma lookup table (256 entries)
static uint8_t s_gammaLUT[256];
static float s_currentGamma = 0.0f;

// =============================================================================
// Helper Functions
// =============================================================================

static void buildGammaLUT(float gamma) {
    if (s_currentGamma == gamma) return;
    
    for (int i = 0; i < 256; i++) {
        float normalized = i / 255.0f;
        float corrected = powf(normalized, gamma);
        s_gammaLUT[i] = (uint8_t)(corrected * 255.0f + 0.5f);
    }
    s_currentGamma = gamma;
}

// =============================================================================
// Implementation
// =============================================================================

bool postprocess_init() {
    if (s_initialized) return true;
    
    s_config = PostProcessConfig();
    
    // Build default gamma LUT
    buildGammaLUT(2.2f);
    
    s_initialized = true;
    return true;
}

void postprocess_shutdown() {
    s_initialized = false;
}

bool apply_postprocess_chain(uint8_t* buffer, int w, int h) {
    if (!buffer || w <= 0 || h <= 0) return false;
    if (!s_config.enabled) return true;
    
    // Apply each enabled filter in order
    for (uint8_t i = 0; i < s_config.filterCount; i++) {
        if (s_config.filters[i].enabled) {
            if (!apply_filter(buffer, w, h, s_config.filters[i])) {
                return false;
            }
        }
    }
    
    return true;
}

bool apply_filter(uint8_t* buffer, int w, int h, const FilterParams& params) {
    if (!params.enabled) return true;
    
    switch (params.type) {
        case FilterType::GAMMA_CORRECTION:
            filter_gamma(buffer, w, h, params.param1 > 0 ? params.param1 : 2.2f);
            break;
            
        case FilterType::CONTRAST:
            filter_contrast(buffer, w, h, params.strength);
            break;
            
        case FilterType::BRIGHTNESS:
            filter_brightness(buffer, w, h, (int)(params.param1 * 255) - 128);
            break;
            
        case FilterType::SHARPEN:
            filter_sharpen(buffer, w, h, params.strength);
            break;
            
        case FilterType::DENOISE:
            filter_denoise(buffer, w, h, params.strength);
            break;
            
        case FilterType::HISTOGRAM_EQ:
            filter_histogram_eq(buffer, w, h);
            break;
            
        case FilterType::VIGNETTE:
            filter_vignette(buffer, w, h, params.strength, params.param1);
            break;
            
        case FilterType::GRAIN:
            filter_grain(buffer, w, h, params.strength);
            break;
            
        case FilterType::DITHER:
            // Dithering handled by dither_pipeline
            break;
            
        default:
            return false;
    }
    
    return true;
}

void postprocess_set_config(const PostProcessConfig& config) {
    s_config = config;
}

const PostProcessConfig& postprocess_get_config() {
    return s_config;
}

void postprocess_set_filter_enabled(FilterType type, bool enabled) {
    for (uint8_t i = 0; i < s_config.filterCount; i++) {
        if (s_config.filters[i].type == type) {
            s_config.filters[i].enabled = enabled;
            return;
        }
    }
}

bool postprocess_is_filter_enabled(FilterType type) {
    for (uint8_t i = 0; i < s_config.filterCount; i++) {
        if (s_config.filters[i].type == type) {
            return s_config.filters[i].enabled;
        }
    }
    return false;
}

void postprocess_set_filter_strength(FilterType type, float strength) {
    for (uint8_t i = 0; i < s_config.filterCount; i++) {
        if (s_config.filters[i].type == type) {
            s_config.filters[i].strength = strength;
            return;
        }
    }
}

void postprocess_enable(bool enabled) {
    s_config.enabled = enabled;
}

const char* postprocess_get_filter_name(FilterType type) {
    uint8_t idx = static_cast<uint8_t>(type);
    if (idx < static_cast<uint8_t>(FilterType::COUNT)) {
        return s_filterNames[idx];
    }
    return "Unknown";
}

// =============================================================================
// Individual Filter Implementations
// =============================================================================

void filter_gamma(uint8_t* buffer, int w, int h, float gamma) {
    buildGammaLUT(gamma);
    
    int size = w * h;
    for (int i = 0; i < size; i++) {
        buffer[i] = s_gammaLUT[buffer[i]];
    }
}

void filter_contrast(uint8_t* buffer, int w, int h, float contrast) {
    int size = w * h;
    float factor = (259.0f * (contrast * 255.0f + 255.0f)) / (255.0f * (259.0f - contrast * 255.0f));
    
    for (int i = 0; i < size; i++) {
        int val = (int)(factor * (buffer[i] - 128) + 128);
        buffer[i] = constrain(val, 0, 255);
    }
}

void filter_brightness(uint8_t* buffer, int w, int h, int brightness) {
    int size = w * h;
    for (int i = 0; i < size; i++) {
        int val = buffer[i] + brightness;
        buffer[i] = constrain(val, 0, 255);
    }
}

void filter_sharpen(uint8_t* buffer, int w, int h, float strength) {
    // Simple unsharp mask using 3x3 kernel
    // TODO: Implement proper convolution with temporary buffer
    
    // Skip edges for now
    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            int idx = y * w + x;
            
            // Calculate Laplacian
            int center = buffer[idx] * 5;
            int neighbors = buffer[idx - 1] + buffer[idx + 1] + 
                           buffer[idx - w] + buffer[idx + w];
            
            int sharpened = center - neighbors;
            int result = buffer[idx] + (int)(sharpened * strength);
            buffer[idx] = constrain(result, 0, 255);
        }
    }
}

void filter_denoise(uint8_t* buffer, int w, int h, float strength) {
    // Simple 3x3 median filter approximation
    // TODO: Implement proper median filter
    
    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            int idx = y * w + x;
            
            // Calculate local average
            int sum = buffer[idx - w - 1] + buffer[idx - w] + buffer[idx - w + 1] +
                     buffer[idx - 1] + buffer[idx] + buffer[idx + 1] +
                     buffer[idx + w - 1] + buffer[idx + w] + buffer[idx + w + 1];
            
            int avg = sum / 9;
            int result = buffer[idx] + (int)((avg - buffer[idx]) * strength);
            buffer[idx] = constrain(result, 0, 255);
        }
    }
}

void filter_histogram_eq(uint8_t* buffer, int w, int h) {
    int size = w * h;
    
    // Calculate histogram
    int histogram[256] = {0};
    for (int i = 0; i < size; i++) {
        histogram[buffer[i]]++;
    }
    
    // Calculate CDF
    int cdf[256];
    cdf[0] = histogram[0];
    for (int i = 1; i < 256; i++) {
        cdf[i] = cdf[i - 1] + histogram[i];
    }
    
    // Find minimum non-zero CDF
    int cdfMin = 0;
    for (int i = 0; i < 256; i++) {
        if (cdf[i] > 0) {
            cdfMin = cdf[i];
            break;
        }
    }
    
    // Build equalization LUT
    uint8_t lut[256];
    for (int i = 0; i < 256; i++) {
        if (cdf[i] > 0) {
            lut[i] = (uint8_t)(((cdf[i] - cdfMin) * 255) / (size - cdfMin));
        } else {
            lut[i] = 0;
        }
    }
    
    // Apply equalization
    for (int i = 0; i < size; i++) {
        buffer[i] = lut[buffer[i]];
    }
}

void filter_vignette(uint8_t* buffer, int w, int h, float strength, float radius) {
    float cx = w / 2.0f;
    float cy = h / 2.0f;
    float maxDist = sqrtf(cx * cx + cy * cy);
    
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int idx = y * w + x;
            
            float dx = x - cx;
            float dy = y - cy;
            float dist = sqrtf(dx * dx + dy * dy) / maxDist;
            
            // Calculate vignette factor
            float factor = 1.0f - (dist - radius) * strength;
            factor = fmaxf(0.0f, fminf(1.0f, factor));
            
            buffer[idx] = (uint8_t)(buffer[idx] * factor);
        }
    }
}

void filter_grain(uint8_t* buffer, int w, int h, float amount) {
    int size = w * h;
    int grainRange = (int)(amount * 50);
    
    for (int i = 0; i < size; i++) {
        int grain = (rand() % (grainRange * 2 + 1)) - grainRange;
        int result = buffer[i] + grain;
        buffer[i] = constrain(result, 0, 255);
    }
}

// =============================================================================
// Presets
// =============================================================================

void postprocess_load_preset(PostProcessPreset preset) {
    s_config = PostProcessConfig();
    
    switch (preset) {
        case PostProcessPreset::NONE:
            s_config.enabled = false;
            break;
            
        case PostProcessPreset::GAMEBOY_CLASSIC:
            s_config.filterCount = 2;
            s_config.filters[0] = FilterParams(FilterType::CONTRAST, true, 1.2f);
            s_config.filters[1] = FilterParams(FilterType::DITHER, true, 1.0f);
            break;
            
        case PostProcessPreset::VINTAGE_PHOTO:
            s_config.filterCount = 3;
            s_config.filters[0] = FilterParams(FilterType::CONTRAST, true, 0.9f);
            s_config.filters[1] = FilterParams(FilterType::VIGNETTE, true, 0.5f);
            s_config.filters[1].param1 = 0.5f;
            s_config.filters[2] = FilterParams(FilterType::GRAIN, true, 0.2f);
            break;
            
        case PostProcessPreset::HIGH_CONTRAST:
            s_config.filterCount = 2;
            s_config.filters[0] = FilterParams(FilterType::CONTRAST, true, 1.5f);
            s_config.filters[1] = FilterParams(FilterType::HISTOGRAM_EQ, true, 1.0f);
            break;
            
        case PostProcessPreset::NIGHT_VISION:
            s_config.filterCount = 2;
            s_config.filters[0] = FilterParams(FilterType::GAMMA_CORRECTION, true, 1.0f);
            s_config.filters[0].param1 = 0.6f;
            s_config.filters[1] = FilterParams(FilterType::CONTRAST, true, 1.3f);
            break;
            
        case PostProcessPreset::THERMAL:
            s_config.filterCount = 2;
            s_config.filters[0] = FilterParams(FilterType::HISTOGRAM_EQ, true, 1.0f);
            s_config.filters[1] = FilterParams(FilterType::CONTRAST, true, 1.4f);
            break;
            
        default:
            break;
    }
}

} // namespace filters
} // namespace pxlcam
