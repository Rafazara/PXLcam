/**
 * @file pxlcam_v13_config.h
 * @brief PXLcam v1.3.0 Configuration Header
 * 
 * Central configuration for v1.3.0 experimental features:
 * - WiFi Preview Pipeline
 * - Extended Palette System
 * - New capture modes
 * 
 * @version 1.3.0
 * @date 2024
 */

#ifndef PXLCAM_V13_CONFIG_H
#define PXLCAM_V13_CONFIG_H

// ============================================================================
// V1.3.0 Feature Flags
// ============================================================================

// WiFi Preview Feature
#ifndef PXL_WIFI_ENABLED
#define PXL_WIFI_ENABLED        1
#endif

// Extended Palette System
#ifndef PXL_PALETTE_EXTENDED
#define PXL_PALETTE_EXTENDED    1
#endif

// Custom Palette Editor
#ifndef PXL_PALETTE_EDITOR
#define PXL_PALETTE_EDITOR      0   // Disabled by default (WIP)
#endif

// ============================================================================
// WiFi Configuration Defaults
// ============================================================================

#define PXL_WIFI_AP_SSID        "PXLcam"
#define PXL_WIFI_AP_PASS        "pxlcam1234"
#define PXL_WIFI_AP_CHANNEL     1
#define PXL_WIFI_HTTP_PORT      80
#define PXL_WIFI_WS_PORT        81
#define PXL_WIFI_MAX_CLIENTS    4
#define PXL_WIFI_STREAM_QUALITY 12      // JPEG quality (0-63, lower = better)
#define PXL_WIFI_STREAM_FPS     15      // Target FPS for streaming

// ============================================================================
// Palette System Configuration
// ============================================================================

#define PXL_PALETTE_COUNT       12      // Total palettes (built-in + custom)
#define PXL_PALETTE_CUSTOM_MAX  3       // Maximum custom palette slots
#define PXL_PALETTE_COLORS_MAX  4       // Colors per palette
#define PXL_PALETTE_NAME_LEN    16      // Max palette name length

// ============================================================================
// Memory Configuration for v1.3.0 Features
// ============================================================================

// WiFi streaming buffer size
#define PXL_WIFI_BUFFER_SIZE    (32 * 1024)     // 32KB frame buffer

// Palette LUT size (for fast color mapping)
#define PXL_PALETTE_LUT_SIZE    256             // 256-entry lookup table

// ============================================================================
// Debug Configuration
// ============================================================================

#ifdef PXL_V13_EXPERIMENTAL
    #define PXL_LOG_WIFI        1
    #define PXL_LOG_PALETTE     1
#else
    #define PXL_LOG_WIFI        0
    #define PXL_LOG_PALETTE     0
#endif

#endif // PXLCAM_V13_CONFIG_H
