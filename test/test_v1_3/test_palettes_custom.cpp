/**
 * @file test_palettes_custom.cpp
 * @brief Unit tests for PXLcam v1.3.0 Custom Palette System
 * 
 * Tests:
 * - Custom palette loading from JSON
 * - Palette selection and persistence
 * - Built-in vs custom palette distinction
 * - Palette cycling functionality
 * - JSON parsing edge cases
 * 
 * @author PXLcam Team
 * @version 1.3.0
 */

#include <unity.h>
#include <string.h>
#include "filters/palette.h"
#include "pxlcam_config.h"

using namespace pxlcam::filters;

// =============================================================================
// Test Fixtures
// =============================================================================

void setUp(void) {
    // Re-initialize palette system before each test
    // Note: In real environment, we'd need to reset state
}

void tearDown(void) {
    // Cleanup after each test
}

// =============================================================================
// Basic Palette Tests
// =============================================================================

/**
 * @brief Test that palette system initializes correctly
 */
void test_PaletteInit_Success(void) {
    palette_init();
    
    TEST_ASSERT_TRUE(palette_is_initialized());
    TEST_ASSERT_EQUAL_UINT8(TOTAL_PALETTE_COUNT, palette_get_count());
    TEST_ASSERT_EQUAL_UINT8(BUILTIN_PALETTE_COUNT, palette_get_builtin_count());
    TEST_ASSERT_EQUAL_UINT8(CUSTOM_PALETTE_COUNT, palette_get_custom_count());
}

/**
 * @brief Test that all built-in palettes have valid names
 */
void test_BuiltInPalettes_HaveValidNames(void) {
    palette_init();
    
    for (uint8_t i = 0; i < BUILTIN_PALETTE_COUNT; ++i) {
        const Palette& pal = palette_get_by_index(i);
        TEST_ASSERT_NOT_NULL(pal.name);
        TEST_ASSERT_TRUE(strlen(pal.name) > 0);
        TEST_ASSERT_TRUE(strlen(pal.name) < PALETTE_NAME_MAX_LEN);
    }
}

/**
 * @brief Test that all built-in palettes have valid tone values
 */
void test_BuiltInPalettes_HaveValidTones(void) {
    palette_init();
    
    for (uint8_t i = 0; i < BUILTIN_PALETTE_COUNT; ++i) {
        const Palette& pal = palette_get_by_index(i);
        
        // All tones should be in valid range (0-255)
        for (uint8_t t = 0; t < PALETTE_TONE_COUNT; ++t) {
            TEST_ASSERT_TRUE(pal.tones[t] <= 255);
        }
        
        // Most palettes should have tones in ascending order (dark to light)
        // Exception: HI_CONTRAST which has 0,0,255,255
        if (i != static_cast<uint8_t>(PaletteType::HI_CONTRAST)) {
            TEST_ASSERT_TRUE(pal.tones[0] <= pal.tones[1]);
            TEST_ASSERT_TRUE(pal.tones[1] <= pal.tones[2]);
            TEST_ASSERT_TRUE(pal.tones[2] <= pal.tones[3]);
        }
    }
}

// =============================================================================
// Custom Palette Type Detection
// =============================================================================

/**
 * @brief Test palette_is_custom() correctly identifies custom vs built-in
 */
void test_IsCustom_CorrectlyIdentifiesPaletteTypes(void) {
    palette_init();
    
    // Built-in palettes (0-7) should NOT be custom
    TEST_ASSERT_FALSE(palette_is_custom(PaletteType::GB_CLASSIC));
    TEST_ASSERT_FALSE(palette_is_custom(PaletteType::GB_POCKET));
    TEST_ASSERT_FALSE(palette_is_custom(PaletteType::CGA_MODE1));
    TEST_ASSERT_FALSE(palette_is_custom(PaletteType::CGA_MODE2));
    TEST_ASSERT_FALSE(palette_is_custom(PaletteType::SEPIA));
    TEST_ASSERT_FALSE(palette_is_custom(PaletteType::NIGHT));
    TEST_ASSERT_FALSE(palette_is_custom(PaletteType::THERMAL));
    TEST_ASSERT_FALSE(palette_is_custom(PaletteType::HI_CONTRAST));
    
    // Custom palettes (8-10) should be custom
    TEST_ASSERT_TRUE(palette_is_custom(PaletteType::CUSTOM_1));
    TEST_ASSERT_TRUE(palette_is_custom(PaletteType::CUSTOM_2));
    TEST_ASSERT_TRUE(palette_is_custom(PaletteType::CUSTOM_3));
}

/**
 * @brief Test that built-in and custom palettes are distinct
 */
void test_BuiltInAndCustom_AreDistinct(void) {
    palette_init();
    
    // Get GB_CLASSIC and CUSTOM_1
    const Palette& builtin = palette_get(PaletteType::GB_CLASSIC);
    const Palette& custom = palette_get(PaletteType::CUSTOM_1);
    
    // They should have different names
    TEST_ASSERT_NOT_EQUAL_STRING(builtin.name, custom.name);
    
    // Custom should have "Custom" in name by default
    TEST_ASSERT_NOT_NULL(strstr(custom.name, "Custom"));
}

// =============================================================================
// Custom Palette Modification
// =============================================================================

/**
 * @brief Test setting custom palette tones
 */
void test_SetCustom_ModifiesTones(void) {
    palette_init();
    
    uint8_t newTones[PALETTE_TONE_COUNT] = { 10, 50, 100, 200 };
    
    bool result = palette_set_custom(PaletteType::CUSTOM_1, newTones, "TestPal");
    TEST_ASSERT_TRUE(result);
    
    const Palette& pal = palette_get(PaletteType::CUSTOM_1);
    TEST_ASSERT_EQUAL_STRING("TestPal", pal.name);
    TEST_ASSERT_EQUAL_UINT8(10, pal.tones[0]);
    TEST_ASSERT_EQUAL_UINT8(50, pal.tones[1]);
    TEST_ASSERT_EQUAL_UINT8(100, pal.tones[2]);
    TEST_ASSERT_EQUAL_UINT8(200, pal.tones[3]);
}

/**
 * @brief Test that built-in palettes cannot be modified
 */
void test_SetCustom_RejectsBuiltIn(void) {
    palette_init();
    
    uint8_t newTones[PALETTE_TONE_COUNT] = { 0, 0, 0, 0 };
    
    // Attempting to set built-in palette should fail
    bool result = palette_set_custom(PaletteType::GB_CLASSIC, newTones, "Hacked");
    TEST_ASSERT_FALSE(result);
    
    // Original palette should be unchanged
    const Palette& pal = palette_get(PaletteType::GB_CLASSIC);
    TEST_ASSERT_EQUAL_STRING("GB Classic", pal.name);
    TEST_ASSERT_NOT_EQUAL_UINT8(0, pal.tones[0]);  // Should still have original values
}

/**
 * @brief Test resetting custom palette to defaults
 */
void test_ResetCustom_RestoresDefaults(void) {
    palette_init();
    
    // First modify a custom palette
    uint8_t newTones[PALETTE_TONE_COUNT] = { 1, 2, 3, 4 };
    palette_set_custom(PaletteType::CUSTOM_1, newTones, "Modified");
    
    // Now reset it
    bool result = palette_reset_custom(PaletteType::CUSTOM_1);
    TEST_ASSERT_TRUE(result);
    
    // Check it has default name
    const Palette& pal = palette_get(PaletteType::CUSTOM_1);
    TEST_ASSERT_EQUAL_STRING("Custom 1", pal.name);
}

// =============================================================================
// Palette Cycling Tests
// =============================================================================

/**
 * @brief Test palette cycling forward through built-ins only
 */
void test_CycleNext_BuiltinOnly(void) {
    palette_init();
    
    PaletteType current = PaletteType::GB_CLASSIC;
    
    // Cycle through all built-in palettes
    for (uint8_t i = 0; i < BUILTIN_PALETTE_COUNT; ++i) {
        PaletteType next = palette_cycle_next(current, false);  // Built-in only
        
        // Should wrap around at end
        if (current == PaletteType::HI_CONTRAST) {
            TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PaletteType::GB_CLASSIC), 
                                     static_cast<uint8_t>(next));
        } else {
            TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(current) + 1, 
                                     static_cast<uint8_t>(next));
        }
        
        current = next;
    }
}

/**
 * @brief Test palette cycling backward
 */
void test_CyclePrev_WrapsAround(void) {
    palette_init();
    
    // Starting from GB_CLASSIC (0), cycling prev should go to HI_CONTRAST (7)
    PaletteType prev = palette_cycle_prev(PaletteType::GB_CLASSIC, false);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PaletteType::HI_CONTRAST), 
                             static_cast<uint8_t>(prev));
}

// =============================================================================
// Palette Selection Tests (when PXLCAM_FEATURE_CUSTOM_PALETTES is enabled)
// =============================================================================

#if PXLCAM_FEATURE_CUSTOM_PALETTES

/**
 * @brief Test palette selection
 */
void test_Select_SetsCurrent(void) {
    palette_init();
    
    // Select a different palette
    bool result = palette_select(PaletteType::SEPIA);
    TEST_ASSERT_TRUE(result);
    
    // Current should now be SEPIA
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PaletteType::SEPIA),
                             static_cast<uint8_t>(palette_current_type()));
    
    // palette_current() should return SEPIA
    const Palette& current = palette_current();
    TEST_ASSERT_EQUAL_STRING("Sepia", current.name);
}

/**
 * @brief Test palette selection rejects invalid types
 */
void test_Select_RejectsInvalid(void) {
    palette_init();
    
    // Save current
    PaletteType before = palette_current_type();
    
    // Try to select invalid type (out of range)
    bool result = palette_select(static_cast<PaletteType>(255));
    TEST_ASSERT_FALSE(result);
    
    // Current should be unchanged
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(before),
                             static_cast<uint8_t>(palette_current_type()));
}

/**
 * @brief Test custom slot loading status
 */
void test_CustomSlots_TrackLoadedStatus(void) {
    palette_init();
    
    // By default, custom slots should not be loaded
    TEST_ASSERT_FALSE(palette_custom_is_loaded(PaletteType::CUSTOM_1));
    TEST_ASSERT_FALSE(palette_custom_is_loaded(PaletteType::CUSTOM_2));
    TEST_ASSERT_FALSE(palette_custom_is_loaded(PaletteType::CUSTOM_3));
    
    // Built-in should return false (not custom)
    TEST_ASSERT_FALSE(palette_custom_is_loaded(PaletteType::GB_CLASSIC));
}

/**
 * @brief Test setting custom slot marks it as loaded
 */
void test_SetCustomSlot_MarksLoaded(void) {
    palette_init();
    
    Palette testPal = {
        .tones = { 20, 60, 120, 220 },
        .name = "TestPalette"
    };
    
    bool result = palette_set_custom_slot(PaletteType::CUSTOM_2, testPal);
    TEST_ASSERT_TRUE(result);
    
    // Should now be marked as loaded
    TEST_ASSERT_TRUE(palette_custom_is_loaded(PaletteType::CUSTOM_2));
    
    // Other slots should still be unloaded
    TEST_ASSERT_FALSE(palette_custom_is_loaded(PaletteType::CUSTOM_1));
    TEST_ASSERT_FALSE(palette_custom_is_loaded(PaletteType::CUSTOM_3));
}

/**
 * @brief Test listing all palettes with metadata
 */
void test_ListAll_ReturnsCompleteInfo(void) {
    palette_init();
    
    PaletteInfo list[TOTAL_PALETTE_COUNT];
    uint8_t count = palette_list_all(list);
    
    TEST_ASSERT_EQUAL_UINT8(TOTAL_PALETTE_COUNT, count);
    
    // Check first entry (GB_CLASSIC)
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PaletteType::GB_CLASSIC),
                             static_cast<uint8_t>(list[0].type));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PaletteSource::BUILTIN),
                             static_cast<uint8_t>(list[0].source));
    TEST_ASSERT_TRUE(list[0].loaded);
    TEST_ASSERT_NOT_NULL(list[0].palette);
    
    // Check a custom entry (CUSTOM_1 at index 8)
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PaletteType::CUSTOM_1),
                             static_cast<uint8_t>(list[8].type));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PaletteSource::CUSTOM),
                             static_cast<uint8_t>(list[8].source));
    // Loaded status depends on whether slot was set
}

/**
 * @brief Test custom slots array access
 */
void test_CustomSlots_ArrayAccess(void) {
    palette_init();
    
    const CustomPaletteSlot* slots = palette_custom_slots();
    TEST_ASSERT_NOT_NULL(slots);
    
    // Should have 3 slots
    for (uint8_t i = 0; i < CUSTOM_PALETTE_COUNT; ++i) {
        // Each slot should have a valid palette pointer
        TEST_ASSERT_NOT_NULL(slots[i].data.name);
    }
}

#endif // PXLCAM_FEATURE_CUSTOM_PALETTES

// =============================================================================
// Tone Mapping Tests
// =============================================================================

/**
 * @brief Test palette_map_value finds closest tone
 */
void test_MapValue_FindsClosestTone(void) {
    palette_init();
    
    const Palette& pal = palette_get(PaletteType::GB_POCKET);
    // GB_POCKET tones: 0x00, 0x55, 0xAA, 0xFF
    
    // Value 0 should map to tone[0] (0x00)
    TEST_ASSERT_EQUAL_UINT8(0x00, palette_map_value(0, pal));
    
    // Value 255 should map to tone[3] (0xFF)
    TEST_ASSERT_EQUAL_UINT8(0xFF, palette_map_value(255, pal));
    
    // Value 128 should map to tone[2] (0xAA = 170, closer than 0x55 = 85)
    TEST_ASSERT_EQUAL_UINT8(0xAA, palette_map_value(128, pal));
    
    // Value 42 should map to tone[1] (0x55 = 85)
    TEST_ASSERT_EQUAL_UINT8(0x55, palette_map_value(42, pal));
}

/**
 * @brief Test palette_map_index returns correct index
 */
void test_MapIndex_ReturnsCorrectIndex(void) {
    palette_init();
    
    const Palette& pal = palette_get(PaletteType::GB_POCKET);
    
    // Value 0 should map to index 0
    TEST_ASSERT_EQUAL_UINT8(0, palette_map_index(0, pal));
    
    // Value 255 should map to index 3
    TEST_ASSERT_EQUAL_UINT8(3, palette_map_index(255, pal));
    
    // Value 85 (exactly 0x55) should map to index 1
    TEST_ASSERT_EQUAL_UINT8(1, palette_map_index(0x55, pal));
}

// =============================================================================
// JSON Parsing Edge Cases (simulation)
// =============================================================================

/**
 * @brief Test that palette handles edge case tone values
 */
void test_CustomPalette_EdgeCaseTones(void) {
    palette_init();
    
    // Test with extreme values
    uint8_t extremeTones[PALETTE_TONE_COUNT] = { 0, 0, 255, 255 };
    
    bool result = palette_set_custom(PaletteType::CUSTOM_1, extremeTones, "Extreme");
    TEST_ASSERT_TRUE(result);
    
    const Palette& pal = palette_get(PaletteType::CUSTOM_1);
    TEST_ASSERT_EQUAL_UINT8(0, pal.tones[0]);
    TEST_ASSERT_EQUAL_UINT8(0, pal.tones[1]);
    TEST_ASSERT_EQUAL_UINT8(255, pal.tones[2]);
    TEST_ASSERT_EQUAL_UINT8(255, pal.tones[3]);
}

/**
 * @brief Test palette name truncation
 */
void test_CustomPalette_LongNameTruncated(void) {
    palette_init();
    
    uint8_t tones[PALETTE_TONE_COUNT] = { 10, 20, 30, 40 };
    
    // Name longer than PALETTE_NAME_MAX_LEN
    const char* longName = "ThisIsAVeryLongPaletteNameThatShouldBeTruncated";
    
    bool result = palette_set_custom(PaletteType::CUSTOM_1, tones, longName);
    TEST_ASSERT_TRUE(result);
    
    const Palette& pal = palette_get(PaletteType::CUSTOM_1);
    
    // Name should be truncated
    TEST_ASSERT_TRUE(strlen(pal.name) < PALETTE_NAME_MAX_LEN);
    
    // But should start with same characters
    TEST_ASSERT_EQUAL_INT(0, strncmp(pal.name, longName, strlen(pal.name)));
}

// =============================================================================
// Test Runner
// =============================================================================

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    UNITY_BEGIN();
    
    // Basic tests
    RUN_TEST(test_PaletteInit_Success);
    RUN_TEST(test_BuiltInPalettes_HaveValidNames);
    RUN_TEST(test_BuiltInPalettes_HaveValidTones);
    
    // Custom type detection
    RUN_TEST(test_IsCustom_CorrectlyIdentifiesPaletteTypes);
    RUN_TEST(test_BuiltInAndCustom_AreDistinct);
    
    // Custom modification
    RUN_TEST(test_SetCustom_ModifiesTones);
    RUN_TEST(test_SetCustom_RejectsBuiltIn);
    RUN_TEST(test_ResetCustom_RestoresDefaults);
    
    // Cycling
    RUN_TEST(test_CycleNext_BuiltinOnly);
    RUN_TEST(test_CyclePrev_WrapsAround);
    
#if PXLCAM_FEATURE_CUSTOM_PALETTES
    // Selection & persistence
    RUN_TEST(test_Select_SetsCurrent);
    RUN_TEST(test_Select_RejectsInvalid);
    RUN_TEST(test_CustomSlots_TrackLoadedStatus);
    RUN_TEST(test_SetCustomSlot_MarksLoaded);
    RUN_TEST(test_ListAll_ReturnsCompleteInfo);
    RUN_TEST(test_CustomSlots_ArrayAccess);
#endif
    
    // Tone mapping
    RUN_TEST(test_MapValue_FindsClosestTone);
    RUN_TEST(test_MapIndex_ReturnsCorrectIndex);
    
    // Edge cases
    RUN_TEST(test_CustomPalette_EdgeCaseTones);
    RUN_TEST(test_CustomPalette_LongNameTruncated);
    
    return UNITY_END();
}
