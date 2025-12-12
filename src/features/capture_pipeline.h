/**
 * @file capture_pipeline.h
 * @brief Stylized Capture Pipeline for PXLcam v1.2.0
 * 
 * Complete capture pipeline:
 * 1. RGB capture (simulated)
 * 2. GameBoy-style dithering
 * 3. LUT application
 * 4. BMP encoding (internal)
 * 5. Mock storage simulation
 * 6. UI confirmation with mini preview
 * 
 * @author PXLcam Team
 * @version 1.2.0
 * @date 2024
 */

#ifndef PXLCAM_CAPTURE_PIPELINE_H
#define PXLCAM_CAPTURE_PIPELINE_H

#include <cstdint>
#include <cstddef>
#include "../core/app_context.h"

namespace pxlcam {
namespace features {
namespace capture {

// ============================================================================
// Constants
// ============================================================================

/// Default capture resolution
static constexpr uint16_t DEFAULT_WIDTH = 128;
static constexpr uint16_t DEFAULT_HEIGHT = 128;

/// Mini preview dimensions (for UI confirmation)
static constexpr uint8_t MINI_PREVIEW_SIZE = 64;

/// BMP header size (14 bytes file header + 40 bytes DIB header)
static constexpr size_t BMP_HEADER_SIZE = 54;

/// GameBoy 4-color palette (classic green tones)
static constexpr uint8_t GAMEBOY_PALETTE[4][3] = {
    {155, 188, 15},   // Lightest (0)
    {139, 172, 15},   // Light (1)
    {48,  98,  48},   // Dark (2)
    {15,  56,  15}    // Darkest (3)
};

/// CGA 4-color palette (cyan/magenta/white/black)
static constexpr uint8_t CGA_PALETTE[4][3] = {
    {255, 255, 255},  // White
    {85,  255, 255},  // Cyan
    {255, 85,  255},  // Magenta
    {0,   0,   0}     // Black
};

// ============================================================================
// Capture Result
// ============================================================================

/**
 * @brief Capture pipeline result
 */
enum class CaptureResult : uint8_t {
    SUCCESS = 0,        ///< Capture completed successfully
    ERROR_CAPTURE,      ///< RGB capture failed
    ERROR_DITHER,       ///< Dithering failed
    ERROR_ENCODE,       ///< BMP encoding failed
    ERROR_STORAGE,      ///< Storage write failed
    ERROR_MEMORY,       ///< Memory allocation failed
    ERROR_INVALID_CTX   ///< Invalid AppContext
};

/**
 * @brief Capture statistics
 */
struct CaptureStats {
    uint32_t captureTimeMs;     ///< Time for RGB capture
    uint32_t ditherTimeMs;      ///< Time for dithering
    uint32_t lutTimeMs;         ///< Time for LUT application
    uint32_t encodeTimeMs;      ///< Time for BMP encoding
    uint32_t storageTimeMs;     ///< Time for storage write
    uint32_t totalTimeMs;       ///< Total pipeline time
    size_t imageSizeBytes;      ///< Final image size in bytes
    size_t bmpSizeBytes;        ///< BMP file size
    uint16_t width;             ///< Capture width
    uint16_t height;            ///< Capture height
    
    void reset() {
        captureTimeMs = 0;
        ditherTimeMs = 0;
        lutTimeMs = 0;
        encodeTimeMs = 0;
        storageTimeMs = 0;
        totalTimeMs = 0;
        imageSizeBytes = 0;
        bmpSizeBytes = 0;
        width = 0;
        height = 0;
    }
};

/**
 * @brief Mini preview buffer (64x64 for UI confirmation)
 */
struct MiniPreview {
    uint8_t data[MINI_PREVIEW_SIZE * MINI_PREVIEW_SIZE];  ///< Grayscale preview
    uint8_t width;
    uint8_t height;
    bool valid;
    
    MiniPreview() : width(MINI_PREVIEW_SIZE), height(MINI_PREVIEW_SIZE), valid(false) {
        memset(data, 0, sizeof(data));
    }
};

// ============================================================================
// Pipeline Functions
// ============================================================================

/**
 * @brief Run the complete capture pipeline
 * 
 * Pipeline stages:
 * 1. RGB capture (simulated with test pattern)
 * 2. Convert to grayscale
 * 3. Apply dithering (GameBoy style)
 * 4. Apply LUT based on palette
 * 5. Encode to BMP format
 * 6. Save to MockStorage
 * 7. Generate mini preview for UI
 * 
 * @param ctx Application context with settings
 * @return CaptureResult Result code
 */
CaptureResult runCapture(core::AppContext& ctx);

/**
 * @brief Apply dithering to grayscale image
 * 
 * Uses the existing preview_dither module with GameBoy mode.
 * 
 * @param gray Input grayscale buffer (width * height bytes)
 * @param width Image width
 * @param height Image height
 * @param output Output dithered buffer (2-bit indexed, 4 colors)
 * @param palette Palette to use (from AppContext)
 * @return bool True on success
 */
bool applyDither(const uint8_t* gray, uint16_t width, uint16_t height,
                 uint8_t* output, core::Palette palette);

/**
 * @brief Apply color LUT to indexed image
 * 
 * Converts 2-bit indexed image to 24-bit RGB using palette.
 * 
 * @param indexed Input indexed buffer (2-bit values)
 * @param width Image width
 * @param height Image height
 * @param rgbOutput Output RGB buffer (width * height * 3 bytes)
 * @param palette Palette to use
 * @return bool True on success
 */
bool applyLUT(const uint8_t* indexed, uint16_t width, uint16_t height,
              uint8_t* rgbOutput, core::Palette palette);

/**
 * @brief Encode image data to BMP format
 * 
 * Creates a 24-bit BMP file in memory.
 * 
 * @param rgb Input RGB buffer (width * height * 3 bytes)
 * @param width Image width
 * @param height Image height
 * @param bmpOutput Output buffer (must be large enough for header + data)
 * @param bmpSize Output: actual BMP size in bytes
 * @return bool True on success
 */
bool encodeBMP(const uint8_t* rgb, uint16_t width, uint16_t height,
               uint8_t* bmpOutput, size_t& bmpSize);

/**
 * @brief Generate mini preview from captured image
 * 
 * Downscales the image to 64x64 for UI confirmation display.
 * 
 * @param indexed Source indexed buffer
 * @param width Source width
 * @param height Source height
 * @param preview Output mini preview
 * @return bool True on success
 */
bool generateMiniPreview(const uint8_t* indexed, uint16_t width, uint16_t height,
                         MiniPreview& preview);

/**
 * @brief Get last capture statistics
 * @return const CaptureStats& Reference to statistics
 */
const CaptureStats& getLastStats();

/**
 * @brief Get last mini preview
 * @return const MiniPreview& Reference to mini preview
 */
const MiniPreview& getLastPreview();

/**
 * @brief Get capture result as string
 * @param result Result code
 * @return const char* Human-readable string
 */
const char* resultToString(CaptureResult result);

/**
 * @brief Log capture statistics to Serial
 * @param stats Statistics to log
 */
void logStats(const CaptureStats& stats);

} // namespace capture
} // namespace features
} // namespace pxlcam

#endif // PXLCAM_CAPTURE_PIPELINE_H
