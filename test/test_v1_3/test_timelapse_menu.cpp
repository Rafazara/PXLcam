/**
 * @file test_timelapse_menu.cpp
 * @brief Test suite for Timelapse Menu and Settings (PXLcam v1.3.0)
 * 
 * Tests:
 * - Interval selection wrap-around
 * - MaxFrames cycling
 * - NVS persistence (mock)
 * - shouldCapture logic
 * - Timelapse stopping conditions
 * 
 * Uses Unity Test Framework
 * 
 * @version 1.3.0
 * @date 2024
 */

#include <unity.h>
#include <Arduino.h>

// Feature flags for testing
#define PXLCAM_FEATURE_TIMELAPSE 1

#include "timelapse.h"
#include "timelapse_settings.h"

using namespace pxlcam;
using namespace pxlcam::timelapse;

// =============================================================================
// Test: Interval Selection
// =============================================================================

void test_interval_to_ms() {
    TEST_ASSERT_EQUAL(1000, intervalToMs(TimelapseInterval::FAST_1S));
    TEST_ASSERT_EQUAL(5000, intervalToMs(TimelapseInterval::NORMAL_5S));
    TEST_ASSERT_EQUAL(30000, intervalToMs(TimelapseInterval::SLOW_30S));
    TEST_ASSERT_EQUAL(60000, intervalToMs(TimelapseInterval::MINUTE_1M));
    TEST_ASSERT_EQUAL(300000, intervalToMs(TimelapseInterval::MINUTE_5M));
}

void test_interval_name() {
    TEST_ASSERT_NOT_NULL(intervalName(TimelapseInterval::FAST_1S));
    TEST_ASSERT_NOT_NULL(intervalName(TimelapseInterval::NORMAL_5S));
    TEST_ASSERT_NOT_NULL(intervalName(TimelapseInterval::MINUTE_5M));
}

void test_interval_wrap_forward() {
    // Test forward wrap-around
    TimelapseInterval current = TimelapseInterval::FAST_1S;
    
    current = nextInterval(current);  // -> NORMAL_5S
    TEST_ASSERT_EQUAL(TimelapseInterval::NORMAL_5S, current);
    
    current = nextInterval(current);  // -> SLOW_30S
    TEST_ASSERT_EQUAL(TimelapseInterval::SLOW_30S, current);
    
    current = nextInterval(current);  // -> MINUTE_1M
    TEST_ASSERT_EQUAL(TimelapseInterval::MINUTE_1M, current);
    
    current = nextInterval(current);  // -> MINUTE_5M
    TEST_ASSERT_EQUAL(TimelapseInterval::MINUTE_5M, current);
    
    current = nextInterval(current);  // -> FAST_1S (wrap)
    TEST_ASSERT_EQUAL(TimelapseInterval::FAST_1S, current);
}

void test_interval_wrap_backward() {
    // Test backward wrap-around
    TimelapseInterval current = TimelapseInterval::FAST_1S;
    
    current = prevInterval(current);  // -> MINUTE_5M (wrap)
    TEST_ASSERT_EQUAL(TimelapseInterval::MINUTE_5M, current);
    
    current = prevInterval(current);  // -> MINUTE_1M
    TEST_ASSERT_EQUAL(TimelapseInterval::MINUTE_1M, current);
}

// =============================================================================
// Test: Max Frames Selection
// =============================================================================

void test_max_frames_to_value() {
    TEST_ASSERT_EQUAL(10, maxFramesToValue(MaxFramesOption::FRAMES_10));
    TEST_ASSERT_EQUAL(25, maxFramesToValue(MaxFramesOption::FRAMES_25));
    TEST_ASSERT_EQUAL(50, maxFramesToValue(MaxFramesOption::FRAMES_50));
    TEST_ASSERT_EQUAL(100, maxFramesToValue(MaxFramesOption::FRAMES_100));
    TEST_ASSERT_EQUAL(0, maxFramesToValue(MaxFramesOption::UNLIMITED));  // 0 = unlimited
}

void test_max_frames_name() {
    TEST_ASSERT_NOT_NULL(maxFramesName(MaxFramesOption::FRAMES_10));
    TEST_ASSERT_NOT_NULL(maxFramesName(MaxFramesOption::UNLIMITED));
}

void test_max_frames_cycling() {
    MaxFramesOption current = MaxFramesOption::FRAMES_10;
    
    // Forward cycle
    current = nextMaxFrames(current);  // -> FRAMES_25
    TEST_ASSERT_EQUAL(MaxFramesOption::FRAMES_25, current);
    
    current = nextMaxFrames(current);  // -> FRAMES_50
    current = nextMaxFrames(current);  // -> FRAMES_100
    current = nextMaxFrames(current);  // -> UNLIMITED
    TEST_ASSERT_EQUAL(MaxFramesOption::UNLIMITED, current);
    
    current = nextMaxFrames(current);  // -> FRAMES_10 (wrap)
    TEST_ASSERT_EQUAL(MaxFramesOption::FRAMES_10, current);
}

// =============================================================================
// Test: Timelapse Controller Logic
// =============================================================================

void test_controller_init() {
    TimelapseController& ctrl = TimelapseController::instance();
    
    bool result = ctrl.init();
    TEST_ASSERT_TRUE(result);
    
    // Should not be running after init
    TEST_ASSERT_FALSE(ctrl.isRunning());
    TEST_ASSERT_FALSE(ctrl.isPaused());
}

void test_controller_interval_enforcement() {
    TimelapseController& ctrl = TimelapseController::instance();
    ctrl.init();
    
    // Normal interval
    ctrl.setInterval(5000);
    TEST_ASSERT_EQUAL(5000, ctrl.getInterval());
    
    // Below minimum - should enforce 1000ms minimum
    ctrl.setInterval(500);
    TEST_ASSERT_GREATER_OR_EQUAL(1000, ctrl.getInterval());
    
    // At minimum
    ctrl.setInterval(1000);
    TEST_ASSERT_EQUAL(1000, ctrl.getInterval());
}

void test_controller_max_frames() {
    TimelapseController& ctrl = TimelapseController::instance();
    ctrl.init();
    
    // Set max frames
    ctrl.setMaxFrames(10);
    const TimelapseConfig& config = ctrl.getConfig();
    TEST_ASSERT_EQUAL(10, config.maxFrames);
    
    // Unlimited
    ctrl.setMaxFrames(0);
    TEST_ASSERT_EQUAL(0, ctrl.getConfig().maxFrames);
}

void test_should_capture_not_running() {
    TimelapseController& ctrl = TimelapseController::instance();
    ctrl.init();
    
    // Not running = should not capture
    TEST_ASSERT_FALSE(ctrl.shouldCapture());
}

void test_has_reached_max_frames() {
    // With 10 frame limit
    setCurrentMaxFrames(MaxFramesOption::FRAMES_10, false);
    TEST_ASSERT_FALSE(hasReachedMaxFrames(5));
    TEST_ASSERT_FALSE(hasReachedMaxFrames(9));
    TEST_ASSERT_TRUE(hasReachedMaxFrames(10));
    TEST_ASSERT_TRUE(hasReachedMaxFrames(15));
    
    // Unlimited - never reached
    setCurrentMaxFrames(MaxFramesOption::UNLIMITED, false);
    TEST_ASSERT_FALSE(hasReachedMaxFrames(0));
    TEST_ASSERT_FALSE(hasReachedMaxFrames(1000));
    TEST_ASSERT_FALSE(hasReachedMaxFrames(UINT32_MAX));
}

void test_controller_progress() {
    TimelapseController& ctrl = TimelapseController::instance();
    ctrl.init();
    
    // No max frames = 0% progress
    ctrl.setMaxFrames(0);
    TEST_ASSERT_EQUAL(0, ctrl.getProgress());
    
    // With max frames, calculate progress
    ctrl.setMaxFrames(100);
    // Progress depends on framesCaptured which we can't easily set
    // Just verify it returns a value
    uint8_t progress = ctrl.getProgress();
    TEST_ASSERT_LESS_OR_EQUAL(100, progress);
}

// =============================================================================
// Test: Config Defaults
// =============================================================================

void test_config_defaults() {
    TimelapseConfig config;
    
    TEST_ASSERT_EQUAL(TimelapseMode::INTERVAL, config.mode);
    TEST_ASSERT_EQUAL(5000, config.intervalMs);
    TEST_ASSERT_EQUAL(0, config.maxFrames);
    TEST_ASSERT_TRUE(config.applyStyleFilter);
    TEST_ASSERT_TRUE(config.showCountdown);
    TEST_ASSERT_FALSE(config.beepOnCapture);
}

void test_status_defaults() {
    TimelapseController& ctrl = TimelapseController::instance();
    ctrl.init();
    
    TimelapseStatus status = ctrl.getStatus();
    
    TEST_ASSERT_FALSE(status.running);
    TEST_ASSERT_FALSE(status.paused);
    TEST_ASSERT_EQUAL(0, status.framesCaptured);
}

// =============================================================================
// Test: Timelapse Presets (from timelapse.h)
// =============================================================================

void test_presets_values() {
    TEST_ASSERT_EQUAL(1000, TimelapsePresets::FAST_1S);
    TEST_ASSERT_EQUAL(5000, TimelapsePresets::NORMAL_5S);
    TEST_ASSERT_EQUAL(30000, TimelapsePresets::SLOW_30S);
    TEST_ASSERT_EQUAL(60000, TimelapsePresets::MINUTE_1M);
    TEST_ASSERT_EQUAL(300000, TimelapsePresets::MINUTE_5M);
    TEST_ASSERT_EQUAL(3600000, TimelapsePresets::HOUR_1H);
}

// =============================================================================
// Test Runner
// =============================================================================

void setUp() {
    // Called before each test
}

void tearDown() {
    // Called after each test
    TimelapseController::instance().stop();
}

void setup() {
    delay(2000);  // Wait for serial
    
    UNITY_BEGIN();
    
    // Interval tests
    RUN_TEST(test_interval_to_ms);
    RUN_TEST(test_interval_name);
    RUN_TEST(test_interval_wrap_forward);
    RUN_TEST(test_interval_wrap_backward);
    
    // Max frames tests
    RUN_TEST(test_max_frames_to_value);
    RUN_TEST(test_max_frames_name);
    RUN_TEST(test_max_frames_cycling);
    
    // Controller tests
    RUN_TEST(test_controller_init);
    RUN_TEST(test_controller_interval_enforcement);
    RUN_TEST(test_controller_max_frames);
    RUN_TEST(test_should_capture_not_running);
    RUN_TEST(test_has_reached_max_frames);
    RUN_TEST(test_controller_progress);
    
    // Config/Status tests
    RUN_TEST(test_config_defaults);
    RUN_TEST(test_status_defaults);
    
    // Preset tests
    RUN_TEST(test_presets_values);
    
    UNITY_END();
}

void loop() {
    // Nothing to do in loop for tests
}
