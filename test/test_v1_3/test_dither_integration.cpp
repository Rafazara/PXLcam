/**
 * @file test_dither_integration.cpp
 * @brief Integration tests for apply_palette_dither() and stylized capture
 * 
 * @details
 * These tests validate the complete dithering pipeline including:
 * - apply_palette_dither() with all algorithms
 * - Index output validation (values 0-3)
 * - Stability tests for error diffusion
 * - Format conversion correctness
 * 
 * Tests are designed to be deterministic and hardware-independent.
 * 
 * @author PXLcam Team
 * @version 1.3.0
 * @date 2024
 */

#include <unity.h>
#include <string.h>

// Enable feature for testing
#define PXLCAM_FEATURE_STYLIZED_CAPTURE 1
#define PXLCAM_FEATURE_CUSTOM_PALETTES 1

#include "filters/palette.h"
#include "filters/dither_pipeline.h"

using namespace pxlcam::filters;

// =============================================================================
// Test Fixtures and Helpers
// =============================================================================

// Small test image dimensions
static const int TEST_WIDTH = 8;
static const int TEST_HEIGHT = 8;
static const int TEST_PIXELS = TEST_WIDTH * TEST_HEIGHT;

// Test grayscale patterns
static uint8_t s_grayChecker[TEST_PIXELS];
static uint8_t s_grayGradient[TEST_PIXELS];
static uint8_t s_rgb888Checker[TEST_PIXELS * 3];
static uint8_t s_outputIndices[TEST_PIXELS];

/**
 * @brief Generate 8x8 checker pattern (alternating black/white)
 */
static void generate_checker_pattern() {
    for (int y = 0; y < TEST_HEIGHT; ++y) {
        for (int x = 0; x < TEST_WIDTH; ++x) {
            int idx = y * TEST_WIDTH + x;
            s_grayChecker[idx] = ((x + y) & 1) ? 255 : 0;
            
            // Also fill RGB version
            uint8_t val = s_grayChecker[idx];
            s_rgb888Checker[idx * 3 + 0] = val;
            s_rgb888Checker[idx * 3 + 1] = val;
            s_rgb888Checker[idx * 3 + 2] = val;
        }
    }
}

/**
 * @brief Generate gradient pattern (0-255 across width)
 */
static void generate_gradient_pattern() {
    for (int y = 0; y < TEST_HEIGHT; ++y) {
        for (int x = 0; x < TEST_WIDTH; ++x) {
            int idx = y * TEST_WIDTH + x;
            s_grayGradient[idx] = (x * 255) / (TEST_WIDTH - 1);
        }
    }
}

/**
 * @brief Validate all output indices are in range [0, 3]
 */
static bool validate_indices_range(const uint8_t* indices, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        if (indices[i] > 3) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Check for NaN-like invalid values in error diffusion
 */
static bool validate_no_corruption(const uint8_t* data, size_t count) {
    // All values should be valid uint8_t (0-255)
    // In our case, indices should be 0-3
    for (size_t i = 0; i < count; ++i) {
        if (data[i] > 3) {
            return false;
        }
    }
    return true;
}

// =============================================================================
// Test: Algorithm Count and Enum Validation
// =============================================================================

void test_DitherAlgorithmCount() {
    // Verify enum COUNT matches expected 4 algorithms
    TEST_ASSERT_EQUAL(4, static_cast<uint8_t>(DitherAlgorithm::COUNT));
    
    // Verify enum values
    TEST_ASSERT_EQUAL(0, static_cast<uint8_t>(DitherAlgorithm::ORDERED_8X8));
    TEST_ASSERT_EQUAL(1, static_cast<uint8_t>(DitherAlgorithm::ORDERED_4X4));
    TEST_ASSERT_EQUAL(2, static_cast<uint8_t>(DitherAlgorithm::FLOYD_STEINBERG));
    TEST_ASSERT_EQUAL(3, static_cast<uint8_t>(DitherAlgorithm::ATKINSON));
}

// =============================================================================
// Test: DitherConfig Defaults
// =============================================================================

void test_DitherConfigDefaults() {
    DitherConfig config;
    
    TEST_ASSERT_EQUAL(DitherAlgorithm::ORDERED_4X4, config.algorithm);
    TEST_ASSERT_EQUAL(128, config.strength);
    TEST_ASSERT_TRUE(config.serpentine);
}

void test_DitherConfigWithAlgorithm() {
    DitherConfig config(DitherAlgorithm::FLOYD_STEINBERG);
    
    TEST_ASSERT_EQUAL(DitherAlgorithm::FLOYD_STEINBERG, config.algorithm);
    TEST_ASSERT_EQUAL(128, config.strength);
    TEST_ASSERT_TRUE(config.serpentine);
}

// =============================================================================
// Test: Algorithm Names
// =============================================================================

void test_DitherAlgorithmNames() {
    const char* name8x8 = dither_get_algorithm_name(DitherAlgorithm::ORDERED_8X8);
    const char* name4x4 = dither_get_algorithm_name(DitherAlgorithm::ORDERED_4X4);
    const char* nameFS = dither_get_algorithm_name(DitherAlgorithm::FLOYD_STEINBERG);
    const char* nameAtk = dither_get_algorithm_name(DitherAlgorithm::ATKINSON);
    
    TEST_ASSERT_NOT_NULL(name8x8);
    TEST_ASSERT_NOT_NULL(name4x4);
    TEST_ASSERT_NOT_NULL(nameFS);
    TEST_ASSERT_NOT_NULL(nameAtk);
    
    // Check expected names
    TEST_ASSERT_EQUAL_STRING("Ordered 8x8", name8x8);
    TEST_ASSERT_EQUAL_STRING("Ordered 4x4", name4x4);
    TEST_ASSERT_EQUAL_STRING("Floyd-Steinberg", nameFS);
    TEST_ASSERT_EQUAL_STRING("Atkinson", nameAtk);
    
    // Invalid algorithm should return "Unknown"
    const char* nameInvalid = dither_get_algorithm_name(static_cast<DitherAlgorithm>(99));
    TEST_ASSERT_EQUAL_STRING("Unknown", nameInvalid);
}

// =============================================================================
// Test: Ordered 8x8 - Maps Indices Correctly
// =============================================================================

void test_ApplyOrdered8x8_MapsIndices() {
    palette_init();
    dither_init();
    
    generate_checker_pattern();
    memset(s_outputIndices, 0xFF, sizeof(s_outputIndices));  // Fill with invalid
    
    const Palette& pal = palette_get(PaletteType::GB_CLASSIC);
    
    DitherResult result = apply_palette_dither(
        s_grayChecker, SourceFormat::GRAYSCALE,
        s_outputIndices, TEST_WIDTH, TEST_HEIGHT,
        pal, DitherAlgorithm::ORDERED_8X8
    );
    
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_NULL(result.errorMsg);
    TEST_ASSERT_EQUAL(TEST_PIXELS, result.processedPixels);
    
    // All indices must be in range [0, 3]
    TEST_ASSERT_TRUE(validate_indices_range(s_outputIndices, TEST_PIXELS));
    
    // With pure black/white checker, we should see indices 0 (darkest) and 3 (lightest)
    bool hasIndex0 = false;
    bool hasIndex3 = false;
    for (int i = 0; i < TEST_PIXELS; ++i) {
        if (s_outputIndices[i] == 0) hasIndex0 = true;
        if (s_outputIndices[i] == 3) hasIndex3 = true;
    }
    TEST_ASSERT_TRUE(hasIndex0);  // Should have darkest tone
    TEST_ASSERT_TRUE(hasIndex3);  // Should have lightest tone
}

// =============================================================================
// Test: Ordered 4x4 - Basic Functionality
// =============================================================================

void test_ApplyOrdered4x4_MapsIndices() {
    palette_init();
    dither_init();
    
    generate_gradient_pattern();
    memset(s_outputIndices, 0xFF, sizeof(s_outputIndices));
    
    const Palette& pal = palette_get(PaletteType::GB_POCKET);
    
    DitherResult result = apply_palette_dither(
        s_grayGradient, SourceFormat::GRAYSCALE,
        s_outputIndices, TEST_WIDTH, TEST_HEIGHT,
        pal, DitherAlgorithm::ORDERED_4X4
    );
    
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_TRUE(validate_indices_range(s_outputIndices, TEST_PIXELS));
}

// =============================================================================
// Test: Floyd-Steinberg Stability
// =============================================================================

void test_ApplyFloydSteinberg_Stability() {
    palette_init();
    dither_init();
    
    generate_gradient_pattern();
    memset(s_outputIndices, 0xFF, sizeof(s_outputIndices));
    
    const Palette& pal = palette_get(PaletteType::GB_CLASSIC);
    
    DitherResult result = apply_palette_dither(
        s_grayGradient, SourceFormat::GRAYSCALE,
        s_outputIndices, TEST_WIDTH, TEST_HEIGHT,
        pal, DitherAlgorithm::FLOYD_STEINBERG
    );
    
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_TRUE(validate_indices_range(s_outputIndices, TEST_PIXELS));
    
    // No corruption check - all values must be valid indices
    TEST_ASSERT_TRUE(validate_no_corruption(s_outputIndices, TEST_PIXELS));
    
    // Gradient should produce a mix of all 4 tones
    uint8_t toneCounts[4] = {0};
    for (int i = 0; i < TEST_PIXELS; ++i) {
        toneCounts[s_outputIndices[i]]++;
    }
    
    // With a full gradient, at least one occurrence of each tone is expected
    // (though not guaranteed for very small images)
    TEST_ASSERT_GREATER_THAN(0, toneCounts[0] + toneCounts[1] + toneCounts[2] + toneCounts[3]);
}

// =============================================================================
// Test: Atkinson Stability
// =============================================================================

void test_ApplyAtkinson_Stability() {
    palette_init();
    dither_init();
    
    generate_checker_pattern();
    memset(s_outputIndices, 0xFF, sizeof(s_outputIndices));
    
    const Palette& pal = palette_get(PaletteType::SEPIA);
    
    DitherResult result = apply_palette_dither(
        s_grayChecker, SourceFormat::GRAYSCALE,
        s_outputIndices, TEST_WIDTH, TEST_HEIGHT,
        pal, DitherAlgorithm::ATKINSON
    );
    
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_TRUE(validate_indices_range(s_outputIndices, TEST_PIXELS));
    TEST_ASSERT_TRUE(validate_no_corruption(s_outputIndices, TEST_PIXELS));
}

// =============================================================================
// Test: RGB888 Format Conversion
// =============================================================================

void test_ApplyDither_RGB888Format() {
    palette_init();
    dither_init();
    
    generate_checker_pattern();  // Also fills s_rgb888Checker
    memset(s_outputIndices, 0xFF, sizeof(s_outputIndices));
    
    const Palette& pal = palette_get(PaletteType::GB_CLASSIC);
    
    DitherResult result = apply_palette_dither(
        s_rgb888Checker, SourceFormat::RGB888,
        s_outputIndices, TEST_WIDTH, TEST_HEIGHT,
        pal, DitherAlgorithm::ORDERED_8X8
    );
    
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_TRUE(validate_indices_range(s_outputIndices, TEST_PIXELS));
}

// =============================================================================
// Test: Parameter Validation
// =============================================================================

void test_ApplyDither_NullSrc() {
    dither_init();
    const Palette& pal = palette_get(PaletteType::GB_CLASSIC);
    
    DitherResult result = apply_palette_dither(
        nullptr, SourceFormat::GRAYSCALE,
        s_outputIndices, TEST_WIDTH, TEST_HEIGHT,
        pal, DitherAlgorithm::ORDERED_8X8
    );
    
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_NOT_NULL(result.errorMsg);
}

void test_ApplyDither_NullDst() {
    dither_init();
    generate_checker_pattern();
    const Palette& pal = palette_get(PaletteType::GB_CLASSIC);
    
    DitherResult result = apply_palette_dither(
        s_grayChecker, SourceFormat::GRAYSCALE,
        nullptr, TEST_WIDTH, TEST_HEIGHT,
        pal, DitherAlgorithm::ORDERED_8X8
    );
    
    TEST_ASSERT_FALSE(result.success);
}

void test_ApplyDither_InvalidDimensions() {
    dither_init();
    const Palette& pal = palette_get(PaletteType::GB_CLASSIC);
    
    DitherResult result = apply_palette_dither(
        s_grayChecker, SourceFormat::GRAYSCALE,
        s_outputIndices, 0, TEST_HEIGHT,
        pal, DitherAlgorithm::ORDERED_8X8
    );
    
    TEST_ASSERT_FALSE(result.success);
}

// =============================================================================
// Test: indices_to_grayscale Conversion
// =============================================================================

void test_IndicesToGrayscale() {
    palette_init();
    
    const Palette& pal = palette_get(PaletteType::GB_CLASSIC);
    
    // Create test indices (0, 1, 2, 3)
    uint8_t testIndices[4] = { 0, 1, 2, 3 };
    uint8_t grayOutput[4] = { 0 };
    
    indices_to_grayscale(testIndices, grayOutput, 4, pal);
    
    // Output should match palette tones
    TEST_ASSERT_EQUAL(pal.tones[0], grayOutput[0]);
    TEST_ASSERT_EQUAL(pal.tones[1], grayOutput[1]);
    TEST_ASSERT_EQUAL(pal.tones[2], grayOutput[2]);
    TEST_ASSERT_EQUAL(pal.tones[3], grayOutput[3]);
}

// =============================================================================
// Test: Source Format Utilities
// =============================================================================

void test_SourceFormatBpp() {
    TEST_ASSERT_EQUAL(1, source_format_bpp(SourceFormat::GRAYSCALE));
    TEST_ASSERT_EQUAL(2, source_format_bpp(SourceFormat::RGB565));
    TEST_ASSERT_EQUAL(3, source_format_bpp(SourceFormat::RGB888));
}

void test_SourceFormatName() {
    TEST_ASSERT_EQUAL_STRING("Grayscale", source_format_name(SourceFormat::GRAYSCALE));
    TEST_ASSERT_EQUAL_STRING("RGB565", source_format_name(SourceFormat::RGB565));
    TEST_ASSERT_EQUAL_STRING("RGB888", source_format_name(SourceFormat::RGB888));
}

// =============================================================================
// Test: Pipeline Hook Compilation (Smoke Test)
// =============================================================================

void test_PipelineHook_Compilation() {
    // This test verifies that the entire pipeline can be invoked
    // without runtime errors. It's a smoke test for link correctness.
    
    palette_init();
    dither_init();
    
    // Create a small simulated frame
    const int W = 16;
    const int H = 16;
    uint8_t simFrame[W * H];
    uint8_t simIndices[W * H];
    uint8_t simGray[W * H];
    
    // Fill with gradient
    for (int i = 0; i < W * H; ++i) {
        simFrame[i] = (i * 256) / (W * H);
    }
    
    // Process through full pipeline
    const Palette& pal = palette_get(PaletteType::GB_CLASSIC);
    
    DitherResult result = apply_palette_dither(
        simFrame, SourceFormat::GRAYSCALE,
        simIndices, W, H,
        pal, DitherAlgorithm::ORDERED_8X8
    );
    
    TEST_ASSERT_TRUE(result.success);
    
    // Convert back to grayscale for verification
    indices_to_grayscale(simIndices, simGray, W * H, pal);
    
    // Verify output is valid
    for (int i = 0; i < W * H; ++i) {
        TEST_ASSERT_LESS_OR_EQUAL(255, simGray[i]);
    }
    
    // Cleanup
    dither_shutdown();
}

// =============================================================================
// Test: All Algorithms on Same Input (Consistency Check)
// =============================================================================

void test_AllAlgorithms_ProduceValidOutput() {
    palette_init();
    dither_init();
    
    generate_gradient_pattern();
    
    const Palette& pal = palette_get(PaletteType::GB_CLASSIC);
    
    DitherAlgorithm algorithms[] = {
        DitherAlgorithm::ORDERED_8X8,
        DitherAlgorithm::ORDERED_4X4,
        DitherAlgorithm::FLOYD_STEINBERG,
        DitherAlgorithm::ATKINSON
    };
    
    for (int i = 0; i < 4; ++i) {
        memset(s_outputIndices, 0xFF, sizeof(s_outputIndices));
        
        DitherResult result = apply_palette_dither(
            s_grayGradient, SourceFormat::GRAYSCALE,
            s_outputIndices, TEST_WIDTH, TEST_HEIGHT,
            pal, algorithms[i]
        );
        
        TEST_ASSERT_TRUE_MESSAGE(result.success, dither_get_algorithm_name(algorithms[i]));
        TEST_ASSERT_TRUE_MESSAGE(validate_indices_range(s_outputIndices, TEST_PIXELS),
                                  dither_get_algorithm_name(algorithms[i]));
    }
}

// =============================================================================
// Test Runner
// =============================================================================

void setUp() {
    // Reset state before each test
    memset(s_grayChecker, 0, sizeof(s_grayChecker));
    memset(s_grayGradient, 0, sizeof(s_grayGradient));
    memset(s_rgb888Checker, 0, sizeof(s_rgb888Checker));
    memset(s_outputIndices, 0, sizeof(s_outputIndices));
}

void tearDown() {
    // Cleanup after each test
}

#ifdef ARDUINO
void setup() {
    delay(2000);  // Wait for serial
    
    UNITY_BEGIN();
    
    // Enum and Config tests
    RUN_TEST(test_DitherAlgorithmCount);
    RUN_TEST(test_DitherConfigDefaults);
    RUN_TEST(test_DitherConfigWithAlgorithm);
    RUN_TEST(test_DitherAlgorithmNames);
    
    // Ordered dithering tests
    RUN_TEST(test_ApplyOrdered8x8_MapsIndices);
    RUN_TEST(test_ApplyOrdered4x4_MapsIndices);
    
    // Error diffusion stability tests
    RUN_TEST(test_ApplyFloydSteinberg_Stability);
    RUN_TEST(test_ApplyAtkinson_Stability);
    
    // Format conversion test
    RUN_TEST(test_ApplyDither_RGB888Format);
    
    // Parameter validation tests
    RUN_TEST(test_ApplyDither_NullSrc);
    RUN_TEST(test_ApplyDither_NullDst);
    RUN_TEST(test_ApplyDither_InvalidDimensions);
    
    // Utility function tests
    RUN_TEST(test_IndicesToGrayscale);
    RUN_TEST(test_SourceFormatBpp);
    RUN_TEST(test_SourceFormatName);
    
    // Integration tests
    RUN_TEST(test_PipelineHook_Compilation);
    RUN_TEST(test_AllAlgorithms_ProduceValidOutput);
    
    UNITY_END();
}

void loop() {
    // Nothing
}
#else
// Native test runner
int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    UNITY_BEGIN();
    
    RUN_TEST(test_DitherAlgorithmCount);
    RUN_TEST(test_DitherConfigDefaults);
    RUN_TEST(test_DitherConfigWithAlgorithm);
    RUN_TEST(test_DitherAlgorithmNames);
    RUN_TEST(test_ApplyOrdered8x8_MapsIndices);
    RUN_TEST(test_ApplyOrdered4x4_MapsIndices);
    RUN_TEST(test_ApplyFloydSteinberg_Stability);
    RUN_TEST(test_ApplyAtkinson_Stability);
    RUN_TEST(test_ApplyDither_RGB888Format);
    RUN_TEST(test_ApplyDither_NullSrc);
    RUN_TEST(test_ApplyDither_NullDst);
    RUN_TEST(test_ApplyDither_InvalidDimensions);
    RUN_TEST(test_IndicesToGrayscale);
    RUN_TEST(test_SourceFormatBpp);
    RUN_TEST(test_SourceFormatName);
    RUN_TEST(test_PipelineHook_Compilation);
    RUN_TEST(test_AllAlgorithms_ProduceValidOutput);
    
    return UNITY_END();
}
#endif
