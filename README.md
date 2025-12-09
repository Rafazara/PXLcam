# PXLcam

![download (2)](https://github.com/user-attachments/assets/ec0f4ab5-97f2-4c76-acee-764033478665)

**A retro-pixel camera built with ESP32-CAM**

Transform the world into 8-bit art. Capture, filter, and save pixelated memories with a custom-designed hardware that fits in your pocket.

---

## What is PXLcam?

PXLcam is a fully functional digital camera that captures images with a distinctive lo-fi, pixelated aesthetic. Built around the ESP32-CAM module, it combines modern hardware capabilities with retro-inspired image processing.

**Core Features**
- Real-time pixel art filter pipeline
- MicroSD storage for captured images
- OLED display for system feedback
- Rechargeable LiPo battery system
- Custom 3D-printed enclosure
- Modular firmware architecture

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
| Firmware starter | Complete |
| Modular architecture | Complete |
| Professional documentation | Complete |
| Hardware testing | Pending hardware arrival |
| 3D enclosure design | Planned |
| Brand identity finalization | Planned |
| v1.0 Public release | Future |

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
