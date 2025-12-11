# Changelog

All notable changes to PXLcam firmware will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.1.0] - 2025-01-XX

### Added

#### Preview Mode Enhancements
- **GameBoy-style dithering**: 4-tone LUT with Bayer 8x8 ordered dithering for authentic retro aesthetics
- **Night vision mode**: Gamma boost (0.6) + contrast enhancement for low-light preview
- **Mode cycling**: Hold button for 2 seconds to cycle through modes (GameBoy → Night → Auto)
- **Double buffering**: Lock-free producer/consumer pattern for smoother preview (~20 FPS target)
- **FPS counter**: Rolling average FPS calculation displayed in status bar

#### UI Overlay System
- **Status bar**: SD card icon, battery indicator, mode character (A/G/N), real-time FPS
- **Hint bar**: Context-sensitive button hints ("Hold 2s: mode | Tap: exit")
- **1-bit preview rendering**: Direct bitmap display support for dithered output

#### Exposure Control
- **Auto exposure quick-tune**: Non-blocking exposure adjustment during boot and preview
- **Night mode exposure**: Maximized AEC value and gain for low-light scenarios
- **Manual exposure API**: Set exposure level, brightness, contrast via sensor registers

#### Image Processing
- **Histogram equalization**: Optional contrast enhancement (compile-time flag `PXLCAM_ENABLE_HISTEQ`)
- **Floyd-Steinberg dithering**: Error diffusion fallback (slower but better quality)
- **Threshold dithering**: Simple binary threshold for fastest processing

#### Configuration
- **Feature flags**: All v1.1.0 features are compile-time configurable in `pxlcam_config.h`
- **Build-time version**: `PXLCAM_VERSION` macro for firmware identification

### Changed
- Frame delay reduced from 80ms to 50ms for smoother preview
- Preview module now uses parameterized `downscaleTo64x64()` with output buffer
- Button handling improved: short press (<500ms) exits, long hold (2s) cycles modes

### Technical Details
- RAM usage: ~17% (56KB of 320KB)
- Flash usage: ~15% (475KB of 3MB)
- New modules: `preview_dither`, `preview_buffer`, `display_ui`, `exposure_ctrl`, `fps_counter`
- All new code under `pxlcam::` namespace

---

## [1.0.0] - 2025-01-XX

### Added
- Initial release with complete state machine architecture
- ESP32-CAM AI Thinker board support
- OV2640 camera with JPEG/RGB modes
- SSD1306 OLED display (128x64, I2C)
- SD_MMC storage with DCIM/PXL_xxxx naming
- Basic preview mode (64x64 grayscale, ~12 FPS)
- Pixel art filter pipeline
- Button manager with press/held detection
- Comprehensive logging system
- Self-test module (compile-time flag `PXLCAM_SELFTEST`)
- Camera bringup diagnostics

### Hardware Support
- GPIO12 capture button with boot-strap protection
- I2C display on GPIO14 (SCL) / GPIO15 (SDA)
- PSRAM detection and automatic fallback
- SD card optional (graceful degradation to preview-only mode)

---

## Version Naming

- **Major (X.0.0)**: Breaking changes or complete rewrites
- **Minor (0.X.0)**: New features, backward compatible
- **Patch (0.0.X)**: Bug fixes, minor improvements
