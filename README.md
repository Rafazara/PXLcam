# PXLcam

<img width="195" height="120" alt="Untitled design" src="https://github.com/user-attachments/assets/5c5641d6-d972-49e8-86c2-8e4e9e332ee5" />


**A retro-pixel camera built with ESP32-CAM**

Transform the world into 8-bit art. Capture, filter, and save pixelated memories with a custom-designed hardware that fits in your pocket.

[![Version](https://img.shields.io/badge/version-1.2.0-blue.svg)](CHANGELOG.md)
[![Platform](https://img.shields.io/badge/platform-ESP32--CAM-orange.svg)](#hardware-architecture)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

---

## What is PXLcam?

PXLcam is a fully functional digital camera that captures images with a distinctive lo-fi, pixelated aesthetic. Built around the ESP32-CAM module, it combines modern hardware capabilities with retro-inspired image processing.

**Core Features**
- Real-time pixel art filter pipeline
- **GameBoy-style 4-tone dithering** *(v1.1.0+)*
- **Night vision mode** *(v1.1.0+)*
- **Stylized BMP capture with full post-processing** *(v1.2.0)*
- **Modal mode selection menu** *(v1.2.0)*
- **Professional UX with error screens & animations** *(v1.2.0)*
- **Persistent mode storage (NVS)** *(v1.2.0)*
- **Double-buffered preview (~20 FPS)** *(v1.1.0+)*
- MicroSD storage for captured images
- OLED display with status bar overlay
- Rechargeable LiPo battery system
- Custom 3D-printed enclosure
- Modular firmware architecture

---

## 🆕 v1.2.0 Highlights

### Stylized Capture Pipeline
Capture photos with authentic retro aesthetics:
- **GameBoy Mode**: Bayer 8x8 ordered dithering → 4-tone palette (classic DMG colors)
- **Night Mode**: Gamma boost (0.6) + contrast enhancement (1.4x) for low-light
- **Normal Mode**: Clean grayscale conversion
- **Output**: 8-bit grayscale BMP files, ready to view anywhere

### Modal Menu System
Easy mode selection without interrupting workflow:
- Beautiful vertical list: `[ GameBoy ]`, `[ Night ]`, `[ Normal ]`
- Navigation: **Tap** to move, **Hold 1s** to select
- Fade-in animation for smooth transitions
- Mode persists across power cycles (NVS storage)

### Professional UX
Polished user experience:
- Error screens with auto-dismiss (3 seconds)
- Success animation (blink effect) after capture
- Enhanced status bar: Mode (G/N/X), SD, Memory, FPS
- Toast notifications for feedback

### Button Controls
| Action | Duration | Result |
|--------|----------|--------|
| **Tap** | < 500ms | Capture photo |
| **Hold** | 500ms - 2s | Enter preview mode |
| **Long Hold** | > 2s | Open mode menu |

---

## Hardware Architecture

### Main Components

| Component | Model | Purpose |
|-----------|-------|---------|
| Microcontroller | ESP32-CAM (AI Thinker) | Processing + OV2640 sensor |
| Programmer | ESP32-CAM-MB (CH340G) | USB upload interface |
| Display | OLED 0.96" I²C (128×64) | User feedback |
| Capture Button | DS-314 (momentary) | Trigger photo capture |
| Power Switch | PBS12A (latching) | Physical on/off control |
| Battery | LiPo 3.7V 1800mAh | Power supply |
| Charge Controller | TP4056 + USB-C | Battery management |
| Storage | MicroSD Class 10+ | Image storage |

### Pinout Configuration

```
ESP32-CAM ──→ OLED Display
├─ GPIO 15 ──→ SDA
├─ GPIO 14 ──→ SCL
├─ 3.3V ─────→ VCC
└─ GND ──────→ GND

ESP32-CAM ──→ Controls
└─ GPIO 13 ──→ Capture Button

SD Card: Built-in ESP32-CAM interface (managed by SD_MMC driver)
Power Switch: Physical battery disconnect (not GPIO-controlled)
```

---

## Critical Hardware Notes

### GPIO12 Boot Configuration

**The GPIO12 pin determines ESP32 boot voltage selection.**

- If GPIO12 is LOW during power-on, the module may fail to boot
- Never connect the capture button to GPIO12
- The power switch is purely physical and does not affect GPIO pins
- Document this clearly in any hardware modifications

### Power Considerations

The ESP32-CAM draws significant current spikes during camera initialization:
- Use a stable LiPo battery with TP4056 charge controller
- Avoid powering solely through USB during development
- Ensure proper decoupling capacitors on the power rail

### Storage Requirements

- Use Class 10 or better microSD cards
- Poor quality cards will cause write failures and system freezes
- Format cards as FAT32 before first use

---

## Software Architecture

### Project Structure

```
PXLcam/
├── src/
│   ├── main.cpp              # Main loop + entry point
│   ├── app_controller.cpp    # State machine controller
│   ├── camera_config.cpp     # Camera initialization
│   ├── display_service.cpp   # OLED interface
│   ├── storage_service.cpp   # SD card management
│   └── pixel_filter.cpp      # Retro image processing
├── include/
│   └── *.h                   # Header files
└── lib/
    └── ...                   # Auxiliary services
```

### State Machine Flow

The firmware operates as a finite state machine with automatic error recovery:

```
Boot
  └─→ InitDisplay
       └─→ InitStorage
            └─→ InitCamera
                 └─→ Idle ←─────┐
                      └─→ Capture │
                           └─→ Filter │
                                └─→ Save │
                                     └─→ Feedback ─┘
                                          
Error ──→ Retry / Display Error Message
```

Key architectural decisions:
- Modular design for easy testing and modification
- Automatic PSRAM fallback if unavailable
- GPIO protection to prevent boot failures
- Comprehensive logging and user feedback

---

## Development Setup

### Prerequisites

- Visual Studio Code
- PlatformIO IDE extension
- USB cable for ESP32-CAM-MB programmer

### Dependencies

The project uses PlatformIO for dependency management:

```ini
[env:esp32cam]
platform = espressif32
board = esp32cam
framework = arduino

board_build.flash_mode = qio
monitor_speed = 115200
upload_speed = 921600

build_flags = 
    -DBOARD_HAS_PSRAM
    -mfix-esp32-psram-cache-issue

lib_deps = 
    adafruit/Adafruit GFX Library @ ^1.11.9
    adafruit/Adafruit SSD1306 @ ^2.5.9
    espressif/esp32-camera @ ^2.0.4
```

### Build and Upload

```bash
# Clone the repository
git clone https://github.com/Rafazara/PXLcam
cd PXLcam/firmware

# Open in VS Code
code .

# Connect ESP32-CAM-MB via USB
# In VS Code: PlatformIO → Upload
```

---

## Usage Guide

**Powering On**
1. Hold the power button (square, latching) until the OLED displays "PXLcam Ready"
2. Wait for initialization sequence to complete

**Capturing Photos**
1. Press the capture button (round, momentary)
2. The system will:
   - Capture a frame from the OV2640 sensor
   - Apply the pixel art filter
   - Save to microSD as `/captures/IMG_XXXXX.rgb`
   - Display confirmation on OLED

**File Storage**
- Images are saved in raw RGB format
- Filename pattern: `IMG_00001.rgb`, `IMG_00002.rgb`, etc.
- Files can be processed on a computer for conversion to standard formats

---

## Capture & Preview Modes (v1.2.0)

PXLcam features three distinct capture and preview modes. Access them via the **mode menu** (hold button 2+ seconds).

| Mode | Character | Description |
|------|-----------|-------------|
| **Normal** | `N` | Clean grayscale conversion |
| **GameBoy** | `G` | Authentic 4-tone Bayer dithering (DMG palette) |
| **Night** | `X` | Gamma-boosted + contrast enhanced for low-light |

**Mode Persistence**: Your selected mode is automatically saved and restored on power-up.

**Status Bar Indicators**
- Mode character in box: `[G]`, `[N]`, `[X]`
- SD card icon (present/missing)
- Memory indicator (OK/Low)
- Real-time FPS counter

---

## Hardware Testing Checklist

### Pre-Power Tests
- [ ] Battery fully charged
- [ ] MicroSD card inserted and formatted
- [ ] All connections secured
- [ ] Camera flex cable properly seated
- [ ] No shorts on power rails

### First Boot Verification
- [ ] OLED initializes and displays text
- [ ] SD card mount message appears
- [ ] Camera initialization succeeds
- [ ] No error messages in serial monitor

### Functionality Tests
- [ ] Capture 10 consecutive photos successfully
- [ ] System stability over 10-minute operation
- [ ] Graceful handling of missing SD card
- [ ] Button debounce working correctly
- [ ] Temperature remains within safe range

---

## Development Roadmap

| Milestone | Status |
|-----------|--------|
| v1.0.0 Core firmware | ✅ Complete |
| Modular architecture | ✅ Complete |
| v1.1.0 Advanced preview | ✅ Complete |
| GameBoy dithering | ✅ Complete |
| Night vision mode | ✅ Complete |
| Double buffering | ✅ Complete |
| UI overlay system | ✅ Complete |
| **v1.2.0 Product-ready** | ✅ **Complete** |
| Modal mode menu | ✅ Complete |
| Stylized BMP capture | ✅ Complete |
| NVS persistence | ✅ Complete |
| Professional UX | ✅ Complete |
| Hardware testing | 🔄 Pending hardware |
| 3D enclosure design | 📋 Planned |
| v2.0 Features | 📋 Planned |

---

## Technical Specifications

**Image Capture**
- Sensor: OV2640 (2MP)
- Processing: Custom pixel art filter
- Output format: Raw RGB
- Storage: MicroSD (FAT32)

**Power System**
- Battery: 3.7V LiPo, 1800mAh
- Charging: USB-C via TP4056
- Protection: Overcharge/over-discharge cutoff
- Runtime: ~2-3 hours continuous operation (estimated)

**Physical Interface**
- Display: 0.96" OLED, 128×64 resolution
- Controls: 2 buttons (capture + power)
- Indicators: On-screen status messages

---

## License

MIT License — Free for personal and commercial use.

See [LICENSE](LICENSE) for full details.

---

## Credits

Designed and developed by Rafael Zara  
Hardware: ESP32-CAM (Espressif Systems)  
Camera module: OV2640 (OmniVision)

---

**PXLcam** — Where nostalgia meets innovation.
