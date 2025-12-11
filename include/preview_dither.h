#pragma once
/**
 * @file preview_dither.h
 * @brief GameBoy-style dithering and histogram equalization for PXLcam v1.1.0
 * 
 * Features:
 * - Histogram equalization (optional via PXLCAM_ENABLE_HISTEQ)
 * - GameBoy 4-tone LUT + Bayer 8x8 ordered dithering
 * - Floyd-Steinberg error diffusion fallback
 * - Night vision mode (gamma boost + contrast)
 */

#include <stdint.h>

namespace pxlcam::dither {

/// Dithering mode selection
enum class DitherMode : uint8_t {
    Threshold = 0,      ///< Simple threshold (fastest)
    GameBoy = 1,        ///< GB-style LUT + Bayer ordered dither
    FloydSteinberg = 2, ///< Error diffusion (best quality, slower)
    Night = 3           ///< Night vision enhanced + threshold
};

/// Initialize LUTs and PSRAM buffers. Call once in preview::begin()
/// @param useGameBoyLUT Enable GameBoy palette remapping
void initDitherModule(bool useGameBoyLUT = true);

/// Check if dither module is initialized
bool isInitialized();

/// Set current dithering mode
void setDitherMode(DitherMode mode);

/// Get current dithering mode
DitherMode getDitherMode();

/// Apply histogram equalization in-place on grayscale buffer
/// @param gray Grayscale buffer (w*h bytes)
/// @param w Width
/// @param h Height
void histogramEqualize(uint8_t* gray, int w, int h);

/// Apply GameBoy-style remap + ordered Bayer 8x8 dithering
/// @param gray Input grayscale (w*h bytes, 8-bit)
/// @param w Width
/// @param h Height  
/// @param outBitmap Output 1-bit packed bitmap ((w*h+7)/8 bytes, row-major, MSB-first)
void applyGameBoyDither(const uint8_t* gray, int w, int h, uint8_t* outBitmap);

/// Floyd-Steinberg error-diffusion fallback
/// @param gray Input grayscale (w*h bytes)
/// @param w Width
/// @param h Height
/// @param outBitmap Output 1-bit packed bitmap
void applyFloydSteinbergDither(const uint8_t* gray, int w, int h, uint8_t* outBitmap);

/// Simple threshold conversion (fastest)
/// @param gray Input grayscale (w*h bytes)
/// @param w Width
/// @param h Height
/// @param outBitmap Output 1-bit packed bitmap
/// @param threshold Threshold value (default 128)
void convertTo1bitThreshold(const uint8_t* gray, int w, int h, uint8_t* outBitmap, uint8_t threshold = 128);

/// Apply night vision enhancement (gamma boost + contrast)
/// @param gray Grayscale buffer (modified in-place)
/// @param w Width
/// @param h Height
/// @param gammaBoost Gamma value (default 0.6 for night boost)
/// @param contrastMult Contrast multiplier (default 1.4)
void applyNightVision(uint8_t* gray, int w, int h, float gammaBoost = 0.6f, float contrastMult = 1.4f);

/// Main dithering entry point - applies current mode
/// @param gray Input grayscale (w*h bytes)
/// @param w Width
/// @param h Height
/// @param outBitmap Output 1-bit packed bitmap
/// @param enableHistEq Apply histogram equalization first
void processDither(const uint8_t* gray, int w, int h, uint8_t* outBitmap, bool enableHistEq = false);

/// Self-test for dither module (returns true if passed)
bool selfTest();

}  // namespace pxlcam::dither
