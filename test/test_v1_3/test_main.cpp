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
// Test: Palette Basics
// =============================================================================

void test_palette_default_constructor() {
    pxlcam::filters::Palette pal;
    
    TEST_ASSERT_EQUAL(4, pal.toneCount);
    TEST_ASSERT_EQUAL_STRING("Default", pal.name);
    TEST_ASSERT_FALSE(pal.isCustom);
    
    // Verify tones are in ascending order
    TEST_ASSERT_LESS_THAN(pal.tones[1], pal.tones[0]);  // Dark to light
}

void test_palette_custom_constructor() {
    pxlcam::filters::Palette pal("TestPal", 0, 85, 170, 255);
    
    TEST_ASSERT_EQUAL(4, pal.toneCount);
    TEST_ASSERT_EQUAL_STRING("TestPal", pal.name);
    TEST_ASSERT_EQUAL(0, pal.tones[0]);
    TEST_ASSERT_EQUAL(85, pal.tones[1]);
    TEST_ASSERT_EQUAL(170, pal.tones[2]);
    TEST_ASSERT_EQUAL(255, pal.tones[3]);
}

void test_palette_type_enum_count() {
    // Verify built-in palette count matches enum
    TEST_ASSERT_EQUAL(8, static_cast<uint8_t>(pxlcam::filters::PaletteType::COUNT));
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
// Test: Dither Pipeline Init
// =============================================================================

void test_dither_algorithm_count() {
    TEST_ASSERT_EQUAL(7, static_cast<uint8_t>(pxlcam::filters::DitherAlgorithm::COUNT));
}

void test_dither_config_defaults() {
    pxlcam::filters::DitherConfig config;
    
    TEST_ASSERT_EQUAL(pxlcam::filters::DitherAlgorithm::ORDERED_4X4, config.algorithm);
    TEST_ASSERT_EQUAL(128, config.strength);
    TEST_ASSERT_FALSE(config.applyGamma);
    TEST_ASSERT_TRUE(config.preserveContrast);
}

void test_dither_algorithm_names() {
    const char* name = pxlcam::filters::dither_get_algorithm_name(
        pxlcam::filters::DitherAlgorithm::ORDERED_4X4);
    
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_EQUAL_STRING("Ordered 4x4", name);
}

// =============================================================================
// Test: PostProcess Chain
// =============================================================================

void test_postprocess_filter_names() {
    const char* name = pxlcam::filters::postprocess_get_filter_name(
        pxlcam::filters::FilterType::GAMMA_CORRECTION);
    
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_EQUAL_STRING("Gamma", name);
}

void test_postprocess_preset_count() {
    TEST_ASSERT_EQUAL(6, static_cast<uint8_t>(pxlcam::filters::PostProcessPreset::COUNT));
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
    
    // Palette tests
    RUN_TEST(test_palette_default_constructor);
    RUN_TEST(test_palette_custom_constructor);
    RUN_TEST(test_palette_type_enum_count);
    
    // Timelapse tests
#if PXLCAM_FEATURE_TIMELAPSE
    RUN_TEST(test_timelapse_config_defaults);
    RUN_TEST(test_timelapse_set_interval);
    RUN_TEST(test_timelapse_not_running_by_default);
    RUN_TEST(test_timelapse_presets);
#endif
    
    // Dither tests
    RUN_TEST(test_dither_algorithm_count);
    RUN_TEST(test_dither_config_defaults);
    RUN_TEST(test_dither_algorithm_names);
    
    // PostProcess tests
    RUN_TEST(test_postprocess_filter_names);
    RUN_TEST(test_postprocess_preset_count);
    
    UNITY_END();
}

void loop() {
    // Nothing to do in loop for tests
}
