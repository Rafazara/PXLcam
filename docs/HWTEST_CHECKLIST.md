# PXLcam v1.3.0-HWTEST Hardware Validation Checklist

## Session 13: Hardware Validation Pass
**Firmware Version:** 1.3.0-HWTEST  
**Build Date:** 2024  
**Target:** ESP32-CAM (ESP32-D0WD-V3) on COM5

---

## Pre-Test Setup

### Equipment Required
- [ ] ESP32-CAM board with OV2640 camera
- [ ] MicroSD card (FAT32, 4GB+)
- [ ] USB-to-Serial adapter (CH340/CP2102)
- [ ] Computer with PlatformIO
- [ ] Smartphone/laptop for WiFi testing

### Initial Setup
1. Insert formatted SD card
2. Connect USB adapter to ESP32-CAM
3. Connect to COM5 at 115200 baud
4. Hold GPIO0 LOW during flash (if required)

---

## Test 1: Boot Diagnostics

### Steps
1. Upload firmware: `pio run -e esp32cam_hwtest -t upload`
2. Open serial monitor: `pio device monitor`
3. Reset board (EN button)

### Expected Output
```
[I] AppController begin (v1.3.0)
[I] === HWTEST DIAGNOSTIC MODE ACTIVE ===
[I] HWTEST: SD logging initialized
[I] v1.3.0: Timelapse subsystem ready
```

### Checklist
- [ ] Boot completes without crash
- [ ] OLED displays "PXLcam" logo
- [ ] PSRAM detected (~4MB)
- [ ] SD card mounted
- [ ] `/PXL/hwtest.log` created

**Result:** [ ] PASS / [ ] FAIL  
**Notes:**

---

## Test 2: Memory Baseline

### Steps
1. Observe serial output after boot
2. Note initial memory values

### Expected Values
| Metric | Minimum | Target |
|--------|---------|--------|
| Free Heap | >80KB | >100KB |
| Free PSRAM | >3MB | >3.5MB |
| Fragmentation | <20% | <10% |

### Checklist
- [ ] Free heap >80KB
- [ ] PSRAM available >3MB
- [ ] No memory allocation errors

**Result:** [ ] PASS / [ ] FAIL  
**Actual Values:**
- Heap: ______ KB
- PSRAM: ______ MB
- Frag: ______ %

---

## Test 3: Single Photo Capture

### Steps
1. Press shutter button (GPIO12)
2. Wait for "SALVO!" message
3. Check serial telemetry

### Expected Output
```
[HWLOG] EVENT: CAPTURE_OK /DCIM/PXL_X0001.xxx
[hwtest] [CAPTURE] heap=XXK psram=XXK frag=X% fps=XX
```

### Checklist
- [ ] OLED shows "Capturando..."
- [ ] File saved to SD
- [ ] Timing metrics logged
- [ ] No heap degradation

**Result:** [ ] PASS / [ ] FAIL  
**Capture Time:** ______ ms  
**Filter Time:** ______ ms  
**Save Time:** ______ ms

---

## Test 4: Rapid-Fire Capture (Stress Test)

### Steps
1. Take 10 photos in rapid succession
2. Monitor serial for errors
3. Check SD card file count

### Checklist
- [ ] All 10 photos saved
- [ ] No crashes or freezes
- [ ] Memory remains stable
- [ ] FPS counter updates

**Result:** [ ] PASS / [ ] FAIL  
**Final Heap:** ______ KB  
**Files Created:** ______ / 10

---

## Test 5: Menu Navigation

### Steps
1. Long-press shutter to open menu
2. Navigate all menu items
3. Enter/exit submenus

### Menu Items to Test
- [ ] > MODO (cycle through modes)
- [ ] > PALETA (palette selection)
- [ ] > TIMELAPSE (submenu)
- [ ] > WIFI (submenu)
- [ ] > INFO (system info)

### Checklist
- [ ] Menu opens on long-press
- [ ] UP/DOWN navigation works
- [ ] SELECT enters submenus
- [ ] BACK returns to parent
- [ ] Menu closes properly

**Result:** [ ] PASS / [ ] FAIL  
**Notes:**

---

## Test 6: WiFi Preview Mode

### Steps
1. Open Menu > WIFI
2. Select "Iniciar"
3. Connect phone to "PXLcam-XXXXX" network
4. Open http://192.168.4.1 in browser

### Expected Behavior
- [ ] AP starts successfully
- [ ] OLED shows IP address
- [ ] QR code displays correctly
- [ ] Web UI loads in browser
- [ ] Live MJPEG stream visible

### Performance
- Stream FPS: ______ fps
- Client Count: ______
- Heap Impact: ______ KB

### Checklist
- [ ] WiFi AP starts
- [ ] mDNS works (pxlcam.local)
- [ ] MJPEG streams to client
- [ ] Multiple clients supported
- [ ] WiFi stops cleanly

**Result:** [ ] PASS / [ ] FAIL

---

## Test 7: Timelapse Mode

### Steps
1. Open Menu > TIMELAPSE
2. Set interval to 5 seconds
3. Set duration to "INDEF" (indefinite)
4. Select "Iniciar"
5. Let run for 30 seconds (6 captures)
6. Press button to stop

### Expected Behavior
- [ ] Countdown shows on OLED
- [ ] Captures at correct interval
- [ ] Files saved with TL prefix
- [ ] Frame counter increments
- [ ] Stops on button press

### Performance
- Captures successful: ______ / 6
- Average interval: ______ ms
- Memory stable: [ ] Yes / [ ] No

**Result:** [ ] PASS / [ ] FAIL

---

## Test 8: Deep Sleep (Timelapse)

### Steps
1. Configure timelapse with 60s interval
2. Enable "Sleep" mode
3. Start timelapse
4. Monitor current draw (if possible)
5. Verify wake and capture

### Expected Behavior
- [ ] Device enters light sleep
- [ ] Wakes on schedule
- [ ] Capture succeeds after wake
- [ ] Display re-initializes

**Result:** [ ] PASS / [ ] FAIL  
**Wake Latency:** ______ ms

---

## Test 9: Stylized Capture Modes

### Test Each Mode
| Mode | Preview OK | Capture OK | Output OK |
|------|------------|------------|-----------|
| RAW | [ ] | [ ] | [ ] |
| BW | [ ] | [ ] | [ ] |
| DITHER | [ ] | [ ] | [ ] |
| PALETTE | [ ] | [ ] | [ ] |

### Palette Test
- [ ] Change palette via menu
- [ ] Preview updates
- [ ] Captured file uses palette

**Result:** [ ] PASS / [ ] FAIL

---

## Test 10: SD Card I/O

### Steps
1. Eject SD card while idle
2. Attempt capture (should fail gracefully)
3. Reinsert SD card
4. Verify recovery

### Checklist
- [ ] Missing SD detected
- [ ] Error shown on OLED
- [ ] No crash on missing SD
- [ ] Hot-swap recovery works

**Result:** [ ] PASS / [ ] FAIL

---

## Test 11: Diagnostic Log Review

### Steps
1. Stop device
2. Remove SD card
3. Open `/PXL/hwtest.log` on computer

### Log Should Contain
- [ ] Boot event
- [ ] Metric snapshots
- [ ] Capture events
- [ ] WiFi events (if tested)
- [ ] Timelapse events (if tested)

**Result:** [ ] PASS / [ ] FAIL  
**Log Size:** ______ KB  
**Entries:** ______

---

## Test 12: Stability Soak Test

### Steps
1. Leave device running for 10 minutes
2. Perform occasional captures
3. Monitor for crashes/memory leaks

### Metrics at Start
- Heap: ______ KB
- PSRAM: ______ MB

### Metrics at End (10 min)
- Heap: ______ KB
- PSRAM: ______ MB
- Uptime: ______ seconds

### Checklist
- [ ] No crashes
- [ ] Memory stable (±5%)
- [ ] All functions remain working

**Result:** [ ] PASS / [ ] FAIL

---

## Final Summary

| Test | Result | Notes |
|------|--------|-------|
| 1. Boot Diagnostics | | |
| 2. Memory Baseline | | |
| 3. Single Capture | | |
| 4. Rapid-Fire Stress | | |
| 5. Menu Navigation | | |
| 6. WiFi Preview | | |
| 7. Timelapse Mode | | |
| 8. Deep Sleep | | |
| 9. Stylized Capture | | |
| 10. SD Card I/O | | |
| 11. Diagnostic Log | | |
| 12. Stability Soak | | |

**Overall Result:** ______ / 12 PASSED

**Tester:** ________________________  
**Date:** __________________________  

---

## Known Issues / Observations

1. 
2. 
3. 

---

## Recommendations for v1.3.0 Release

- [ ] Ready for release
- [ ] Needs fixes (list below)
- [ ] Major issues found

**Blocking Issues:**

**Non-Blocking Issues:**

---

*Generated by PXLcam HWTEST Session 13*
