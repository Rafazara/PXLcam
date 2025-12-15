/**
 * @file test_main.cpp
 * @brief PXLcam v1.3.0 Test Suite
 * 
 * Minimal tests for new v1.3.0 features:
 * - Palette system basics
 * - Timelapse controller setup
 * - Dither pipeline initialization
 * 
 * Uses Unity Test Framework (PlatformIO native)
 * 
 * @note Tests updated for new v1.3.0 API:
 * - Palette struct: tones[4] + name only
 * - DitherConfig: algorithm + strength + serpentine only
 */

#include <unity.h>
#include <Arduino.h>

// Feature flags for testing
#define PXLCAM_FEATURE_STYLIZED_CAPTURE 1
#define PXLCAM_FEATURE_CUSTOM_PALETTES 1
#define PXLCAM_FEATURE_TIMELAPSE 1

// Include modules under test
#include "filters/palette.h"
#include "filters/dither_pipeline.h"
#include "filters/postprocess.h"
#include "timelapse.h"

// =============================================================================
// Test: Palette Basics (Updated for v1.3.0 API)
// =============================================================================

void test_palette_get_default() {
    // Initialize palette system
    pxlcam::filters::palette_init();
    
    // Get default palette (GB_CLASSIC)
    const pxlcam::filters::Palette& pal = pxlcam::filters::palette_get(
        pxlcam::filters::PaletteType::GB_CLASSIC);
    
    // Verify palette has 4 tones (via PALETTE_TONE_COUNT constant)
    TEST_ASSERT_EQUAL(4, pxlcam::filters::PALETTE_TONE_COUNT);
    
    // Verify tones are ordered darkest to lightest
    TEST_ASSERT_LESS_OR_EQUAL(pal.tones[0], pal.tones[1]);
    TEST_ASSERT_LESS_OR_EQUAL(pal.tones[1], pal.tones[2]);
    TEST_ASSERT_LESS_OR_EQUAL(pal.tones[2], pal.tones[3]);
    
    // Verify name is set
    TEST_ASSERT_NOT_NULL(pal.name);
}

void test_palette_select_and_current() {
    pxlcam::filters::palette_init();
    
    // Select SEPIA palette
    bool selected = pxlcam::filters::palette_select(pxlcam::filters::PaletteType::SEPIA);
    TEST_ASSERT_TRUE(selected);
    
    // Verify current returns SEPIA
    pxlcam::filters::PaletteType current = pxlcam::filters::palette_current_type();
    TEST_ASSERT_EQUAL(pxlcam::filters::PaletteType::SEPIA, current);
    
    // Get palette via palette_current()
    const pxlcam::filters::Palette& pal = pxlcam::filters::palette_current();
    TEST_ASSERT_NOT_NULL(pal.name);
}

void test_palette_type_enum_count() {
    // Verify total palette count (8 built-in + 3 custom = 11)
    TEST_ASSERT_EQUAL(11, static_cast<uint8_t>(pxlcam::filters::PaletteType::COUNT));
}

// =============================================================================
// Test: Timelapse Controller Setup
// =============================================================================

#if PXLCAM_FEATURE_TIMELAPSE

void test_timelapse_config_defaults() {
    pxlcam::TimelapseConfig config;
    
    TEST_ASSERT_EQUAL(pxlcam::TimelapseMode::INTERVAL, config.mode);
    TEST_ASSERT_EQUAL(5000, config.intervalMs);  // 5 seconds default
    TEST_ASSERT_EQUAL(0, config.maxFrames);      // Unlimited
    TEST_ASSERT_TRUE(config.applyStyleFilter);
    TEST_ASSERT_TRUE(config.showCountdown);
}

void test_timelapse_set_interval() {
    pxlcam::TimelapseController& ctrl = pxlcam::TimelapseController::instance();
    
    ctrl.setInterval(10000);
    TEST_ASSERT_EQUAL(10000, ctrl.getInterval());
    
    // Test minimum enforcement (1 second)
    ctrl.setInterval(500);
    TEST_ASSERT_GREATER_OR_EQUAL(1000, ctrl.getInterval());
}

void test_timelapse_not_running_by_default() {
    pxlcam::TimelapseController& ctrl = pxlcam::TimelapseController::instance();
    
    TEST_ASSERT_FALSE(ctrl.isRunning());
    TEST_ASSERT_FALSE(ctrl.isPaused());
}

void test_timelapse_presets() {
    TEST_ASSERT_EQUAL(1000, pxlcam::TimelapsePresets::FAST_1S);
    TEST_ASSERT_EQUAL(5000, pxlcam::TimelapsePresets::NORMAL_5S);
    TEST_ASSERT_EQUAL(60000, pxlcam::TimelapsePresets::MINUTE_1M);
    TEST_ASSERT_EQUAL(3600000, pxlcam::TimelapsePresets::HOUR_1H);
}

#endif // PXLCAM_FEATURE_TIMELAPSE

// =============================================================================
// Test: Dither Pipeline Init (Updated for v1.3.0 API)
// =============================================================================

void test_dither_algorithm_count() {
    // DitherAlgorithm enum: ORDERED_8X8, ORDERED_4X4, FLOYD_STEINBERG, ATKINSON, COUNT=4
    TEST_ASSERT_EQUAL(4, static_cast<uint8_t>(pxlcam::filters::DitherAlgorithm::COUNT));
}

void test_dither_config_defaults() {
    pxlcam::filters::DitherConfig config;
    
    // New API: algorithm + strength + serpentine only
    TEST_ASSERT_EQUAL(pxlcam::filters::DitherAlgorithm::ORDERED_4X4, config.algorithm);
    TEST_ASSERT_EQUAL(128, config.strength);
    TEST_ASSERT_TRUE(config.serpentine);
}

void test_dither_init_and_status() {
    pxlcam::filters::dither_init();
    TEST_ASSERT_TRUE(pxlcam::filters::dither_is_initialized());
}

// =============================================================================
// Test: PostProcess Chain
// =============================================================================

void test_postprocess_init() {
    // Test that postprocess_init returns true on success
    bool result = pxlcam::filters::postprocess_init();
    TEST_ASSERT_TRUE(result);
}

void test_postprocess_filter_name() {
    pxlcam::filters::postprocess_init();
    
    const char* name = pxlcam::filters::postprocess_get_filter_name(
        pxlcam::filters::FilterType::GAMMA_CORRECTION);
    
    TEST_ASSERT_NOT_NULL(name);
}

// =============================================================================
// Test Runner
// =============================================================================

void setUp() {
    // Called before each test
}

void tearDown() {
    // Called after each test
}

void setup() {
    delay(2000);  // Wait for serial
    
    UNITY_BEGIN();
    
    // Palette tests (v1.3.0 API)
    RUN_TEST(test_palette_get_default);
    RUN_TEST(test_palette_select_and_current);
    RUN_TEST(test_palette_type_enum_count);
    
    // Timelapse tests
#if PXLCAM_FEATURE_TIMELAPSE
    RUN_TEST(test_timelapse_config_defaults);
    RUN_TEST(test_timelapse_set_interval);
    RUN_TEST(test_timelapse_not_running_by_default);
    RUN_TEST(test_timelapse_presets);
#endif
    
    // Dither tests (v1.3.0 API)
    RUN_TEST(test_dither_algorithm_count);
    RUN_TEST(test_dither_config_defaults);
    RUN_TEST(test_dither_init_and_status);
    
    // PostProcess tests
    RUN_TEST(test_postprocess_init);
    RUN_TEST(test_postprocess_filter_name);
    
    UNITY_END();
}

void loop() {
    // Nothing to do in loop for tests
}
