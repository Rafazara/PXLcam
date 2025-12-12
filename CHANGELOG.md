# Changelog

All notable changes to PXLcam firmware will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.2.0] - 2025-01-XX

### Added

#### Simplified Menu System
- **3-item main menu**: Style Mode, Settings, About
- **Style Mode submenu**: Normal, GameBoy, Night mode selection
- **Single-button navigation**: Short press → next, long press → select, 2s hold → back

#### PxlcamSettings Struct
- **Simplified persistence**: 3-field struct (`styleMode`, `nightMode`, `autoExposure`)
- **NVS API**: `loadPxlcamSettings()`, `savePxlcamSettings()`, individual field save functions
- **Serialize/deserialize**: Efficient 3-byte serialization for storage

#### UX Improvements
- **Status indicator**: Visual indicator in top-right corner (Ready/Busy/Error/Recording states)
- **Quick feedback**: `showQuickFeedback()`, `showModeFeedback()`, `showSavedFeedback()`
- **Auto-clear**: Quick feedback messages auto-clear after configurable timeout

#### Expanded Test Suite
- **PxlcamSettings tests**: serialize/deserialize roundtrip, boundary conditions
- **StyleMode validation**: All style modes serialize correctly
- **Boolean combinations**: All nightMode/autoExposure combinations tested

### Changed
- **Menu structure**: Reorganized from detailed mode/palette menus to simplified 3-item layout
- **StyleMode enum**: Added `STYLE_MODE_MENU_ID = 4` for submenu navigation
- **StatusIndicator enum**: New visual states (None, Ready, Busy, Error, Recording)

### Technical Details
- **RAM usage**: 8.4% (27KB of 327KB)
- **Flash usage**: 11.2% (352KB of 3MB)
- **Build**: Zero warnings, zero errors
- **Test count**: 7 new PxlcamSettings tests added to test suite

---

## [1.1.0] - 2025-01-XX

### Added

#### Centralized Mode Management
- **ModeManager module**: Unified mode state across all modules (`mode_manager.h/.cpp`)
- **Mode persistence**: Automatic save/restore of capture mode via NVS
- **Mode cycling API**: `cycleMode()` with automatic persistence
- **Change callbacks**: Register for mode change notifications

#### Stylized Capture Pipeline
- **Full capture pipeline**: Unified capture → process → encode flow (`capture_pipeline.h/.cpp`)
- **GameBoy dithering**: Bayer 8x8 ordered dithering with 4-tone palette (0x0F, 0x56, 0x9B, 0xCF)
- **Night mode enhancement**: Gamma correction (0.6) + contrast boost (1.4x)
- **BMP output**: 8-bit grayscale BMP format with 256-entry palette
- **Debug diagnostics**: Histogram logging and 9-point tone sampling

#### Modal Menu System
- **display_menu module**: Full modal menu for mode selection
- **Visual design**: Vertical list `[ GameBoy ]`, `[ Night ]`, `[ Normal ]` with `>` indicator
- **Navigation**: Short press → next item, long press (1s) → select
- **Fade-in animation**: Configurable animation on menu open
- **Auto-timeout**: Configurable auto-close (default 15s)

#### Professional UX Layer
- **Error screens**: `showError("message")` with auto-timeout (3s)
- **Success animations**: Quick blink effect (100ms × 3) on capture success
- **Menu transitions**: `transitionFadeIn()` / `transitionFadeOut()` for smooth UX
- **Enhanced status bar**: Mode indicator (G/N/X), SD icon, memory indicator, FPS
- **Toast notifications**: `showToast()` with type-based styling (Info/Success/Warning/Error)
- **Progress display**: `showProgress()` with title and percentage bar
- **Loading spinner**: `showLoading()` with animated dots

#### NVS Storage (Simplified)
- **Arduino Preferences API**: Replaced raw ESP-IDF NVS with simpler Preferences wrapper
- **Simplified API**: `nvsStoreInit()`, `saveMode()`, `loadModeOrDefault()`
- **Automatic namespace**: "pxlcam" namespace for all stored values
- **Extensible keys**: Support for future settings (brightness, contrast, file_num)

### Changed
- **PreviewMode enum**: Renamed `Auto` → `Normal` for consistency
- **Status bar signature**: Now takes `PreviewMode` + `freeHeapKb` instead of battery percentage
- **Icons**: Added memory indicator icons (MemOk/MemLow) with PROGMEM storage
- **Mode characters**: N=Normal, G=GameBoy, X=Night (X for eXtreme low-light)

### Technical Details
- **RAM usage**: 17.4% (57KB of 320KB)
- **Flash usage**: 15.7% (493KB of 3MB)
- **PSRAM allocation**: ~308KB for capture buffers (RGB + BMP)
- **New modules**: `mode_manager`, `nvs_store`, `capture_pipeline`, `display_menu`, enhanced `display_ui`
- **Build**: Zero warnings, zero errors

### Memory Management
- **PSRAM-first allocation**: All large buffers allocated in PSRAM with heap fallback
- **Proper cleanup**: `releaseFrame()` ensures camera framebuffer is returned
- **No leaks**: Static buffers reused across captures

---

## [1.0.0] - 2025-01-XX

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
