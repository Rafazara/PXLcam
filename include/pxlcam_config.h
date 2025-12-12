#pragma once
/**
 * @file pxlcam_config.h
 * @brief Compile-time configuration flags for PXLcam v1.3.0
 * 
 * All feature flags can be enabled via platformio.ini build_flags
 * 
 * v1.3.0 New Features:
 * - PXLCAM_FEATURE_WIFI_PREVIEW: WiFi streaming (disabled by default)
 * - PXLCAM_FEATURE_TIMELAPSE: Automated timelapse capture
 * - PXLCAM_FEATURE_STYLIZED_CAPTURE: Palette-based stylized output
 * - PXLCAM_FEATURE_CUSTOM_PALETTES: User-defined color palettes
 */

// =============================================================================
// VERSION
// =============================================================================
#ifndef PXLCAM_VERSION
#define PXLCAM_VERSION "1.3.0"
#endif

// =============================================================================
// v1.3.0 FEATURE FLAGS
// =============================================================================

/**
 * @brief Enable WiFi Preview streaming
 * 
 * When enabled, camera can stream preview over WiFi to web browser.
 * Supports AP mode (camera creates network) and STA mode (joins network).
 * 
 * Memory impact: ~40KB additional RAM when active
 * Default: 0 (disabled to save memory)
 */
#ifndef PXLCAM_FEATURE_WIFI_PREVIEW
#define PXLCAM_FEATURE_WIFI_PREVIEW 0
#endif

/**
 * @brief Enable Timelapse capture mode
 * 
 * Automated capture at configurable intervals.
 * Supports power-saving modes between captures.
 * 
 * Memory impact: ~2KB RAM
 * Default: 0 (disabled)
 */
#ifndef PXLCAM_FEATURE_TIMELAPSE
#define PXLCAM_FEATURE_TIMELAPSE 0
#endif

/**
 * @brief Enable Stylized Capture output
 * 
 * Apply palette-based dithering to captured images.
 * Creates retro/artistic style output (GameBoy, CGA, etc.)
 * 
 * Memory impact: ~4KB RAM for lookup tables
 * Default: 1 (enabled)
 */
#ifndef PXLCAM_FEATURE_STYLIZED_CAPTURE
#define PXLCAM_FEATURE_STYLIZED_CAPTURE 1
#endif

/**
 * @brief Enable Custom Palettes
 * 
 * Allow users to create and save custom color palettes.
 * Palettes stored in NVS flash.
 * 
 * Memory impact: ~1KB NVS per palette
 * Default: 1 (enabled)
 */
#ifndef PXLCAM_FEATURE_CUSTOM_PALETTES
#define PXLCAM_FEATURE_CUSTOM_PALETTES 1
#endif

// =============================================================================
// LEGACY FEATURE FLAGS (v1.2.0 and earlier)
// =============================================================================

/// Enable histogram equalization in preview pipeline
#ifndef PXLCAM_ENABLE_HISTEQ
#define PXLCAM_ENABLE_HISTEQ 0
#endif

/// Enable night vision mode
#ifndef PXLCAM_ENABLE_NIGHT
#define PXLCAM_ENABLE_NIGHT 1
#endif

/// Enable GameBoy-style dithering
#ifndef PXLCAM_GAMEBOY_DITHER
#define PXLCAM_GAMEBOY_DITHER 1
#endif

/// Enable double buffering for preview (requires more PSRAM)
#ifndef PXLCAM_DOUBLE_BUFFER_PREVIEW
#define PXLCAM_DOUBLE_BUFFER_PREVIEW 1
#endif

/// Enable automatic exposure quick-tuning
#ifndef PXLCAM_AUTO_EXPOSURE
#define PXLCAM_AUTO_EXPOSURE 1
#endif

/// Enable UI overlay (status bar, icons)
#ifndef PXLCAM_UI_OVERLAY
#define PXLCAM_UI_OVERLAY 1
#endif

/// Show FPS counter in overlay
#ifndef PXLCAM_SHOW_FPS_OVERLAY
#define PXLCAM_SHOW_FPS_OVERLAY 1
#endif

/// Enable extended self-tests (buffer allocation, dither checksum)
#ifndef PXLCAM_SELFTEST_EXT
#define PXLCAM_SELFTEST_EXT 0
#endif

// =============================================================================
// PREVIEW CONFIGURATION
// =============================================================================

/// Preview dimensions
#define PXLCAM_PREVIEW_W 64
#define PXLCAM_PREVIEW_H 64

/// Target preview FPS
#define PXLCAM_TARGET_FPS 20

/// Frame delay for target FPS (ms)
#define PXLCAM_FRAME_DELAY_MS (1000 / PXLCAM_TARGET_FPS)

/// Maximum RGB buffer size (QVGA)
#define PXLCAM_MAX_RGB_SIZE (320 * 240 * 3)

// =============================================================================
// DITHERING CONFIGURATION
// =============================================================================

/// Default dither mode (0=Threshold, 1=GameBoy, 2=FloydSteinberg, 3=Night)
#define PXLCAM_DEFAULT_DITHER_MODE 1

/// Histogram equalization default state
#define PXLCAM_HISTEQ_DEFAULT false

// =============================================================================
// UI CONFIGURATION
// =============================================================================

/// Show FPS on status bar
#define PXLCAM_SHOW_FPS true

/// Status bar update interval (ms)
#define PXLCAM_STATUS_UPDATE_MS 500

// =============================================================================
// HARDWARE CONFIGURATION
// =============================================================================

/// Button GPIO
#define PXLCAM_BUTTON_PIN 12

/// Long press threshold (ms)
#define PXLCAM_LONG_PRESS_MS 1000

/// Menu hold threshold (ms)
#define PXLCAM_MENU_HOLD_MS 2000

/// Button debounce time (ms)
#define PXLCAM_DEBOUNCE_MS 50

// =============================================================================
// NVS CONFIGURATION (v1.2.0)
// =============================================================================

/// Enable NVS persistence
#ifndef PXLCAM_ENABLE_NVS
#define PXLCAM_ENABLE_NVS 1
#endif

/// Enable stylized capture (apply dithering to saved images)
#ifndef PXLCAM_STYLIZED_CAPTURE
#define PXLCAM_STYLIZED_CAPTURE 1
#endif

/// Enable OLED menu system
#ifndef PXLCAM_ENABLE_MENU
#define PXLCAM_ENABLE_MENU 1
#endif

// =============================================================================
// PSRAM CONFIGURATION
// =============================================================================

/// Minimum PSRAM required for full features (bytes)
#define PXLCAM_MIN_PSRAM (2 * 1024 * 1024)

/// Fallback to single buffer if PSRAM below this
#define PXLCAM_PSRAM_LOW_THRESHOLD (1 * 1024 * 1024)
