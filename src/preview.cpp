#include "preview.h"

#include <esp_camera.h>
#include <img_converters.h>
#include <esp_heap_caps.h>

#include "display.h"
#include "logging.h"

namespace pxlcam::preview {

namespace {

constexpr int kPreviewW = 64;
constexpr int kPreviewH = 64;
constexpr int kFrameDelayMs = 80;  // ~12 FPS
constexpr int kButtonPin = 12;
constexpr size_t kMaxRgbSize = 320 * 240 * 3;  // QVGA RGB888

// 64×64 grayscale buffer (1 byte per pixel)
static uint8_t s_img64[kPreviewW * kPreviewH];

// RGB buffer in PSRAM if available
static uint8_t* s_rgbBuf = nullptr;


// ---------------------------------------------------------------------------
// Downscale RGB888 → 64×64 grayscale
// ---------------------------------------------------------------------------
void downscaleTo64x64(const uint8_t* rgb, int w, int h) {
    const int blockW = w / kPreviewW;
    const int blockH = h / kPreviewH;

    if (blockW == 0 || blockH == 0) {
        PXLCAM_LOGE("[PREVIEW] Invalid block size (src=%dx%d)", w, h);
        return;
    }

    int idx = 0;

    for (int gy = 0; gy < kPreviewH; gy++) {
        for (int gx = 0; gx < kPreviewW; gx++) {

            uint32_t sum = 0;
            int count = 0;

            for (int by = 0; by < blockH; by++) {
                int y = gy * blockH + by;
                if (y >= h) break;

                for (int bx = 0; bx < blockW; bx++) {
                    int x = gx * blockW + bx;
                    if (x >= w) break;

                    const uint8_t* p = rgb + (y * w + x) * 3;

                    // grayscale = 0.299R + 0.587G + 0.114B
                    uint8_t gray = (77 * p[0] + 150 * p[1] + 29 * p[2]) >> 8;

                    sum += gray;
                    count++;
                }
            }

            s_img64[idx++] = (count > 0 ? sum / count : 0);
        }
    }
}


// ---------------------------------------------------------------------------
// Button helpers
// ---------------------------------------------------------------------------
bool isButtonPressed() {
    return digitalRead(kButtonPin) == LOW;
}

bool waitForButtonRelease(uint32_t timeoutMs) {
    uint32_t start = millis();
    while (isButtonPressed()) {
        if (millis() - start > timeoutMs) return false;
        delay(10);
    }
    return true;
}

}  // namespace



// ---------------------------------------------------------------------------
// PREVIEW BEGIN
// ---------------------------------------------------------------------------
void begin() {
    pinMode(kButtonPin, INPUT_PULLUP);

    PXLCAM_LOGI("[PREVIEW] begin()");

    // Try PSRAM first
    if (psramFound()) {
        s_rgbBuf = (uint8_t*)heap_caps_malloc(kMaxRgbSize, MALLOC_CAP_SPIRAM);
        if (s_rgbBuf) {
            PXLCAM_LOGI("[PREVIEW] Alloc RGB buffer in PSRAM (%d bytes)", kMaxRgbSize);
        }
    }

    // fallback
    if (!s_rgbBuf) {
        s_rgbBuf = (uint8_t*)malloc(kMaxRgbSize);
        if (s_rgbBuf) {
            PXLCAM_LOGW("[PREVIEW] Alloc RGB buffer in DRAM (%d bytes)", kMaxRgbSize);
        }
    }

    if (!s_rgbBuf) {
        PXLCAM_LOGE("[PREVIEW] FAILED to allocate RGB buffer!");
    } else {
        PXLCAM_LOGI("[PREVIEW] Module initialized OK");
    }
}



// ---------------------------------------------------------------------------
// CAPTURE + PROCESS ONE FRAME
// ---------------------------------------------------------------------------
bool frame() {
    if (!s_rgbBuf) {
        return false;
    }

    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        return false;
    }

    int srcW = fb->width;
    int srcH = fb->height;

    size_t needed = srcW * srcH * 3;
    if (needed > kMaxRgbSize) {
        esp_camera_fb_return(fb);
        return false;
    }

    bool converted = false;

    // Convert JPEG → RGB888
    if (fb->format == PIXFORMAT_JPEG) {
        converted = fmt2rgb888(fb->buf, fb->len, fb->format, s_rgbBuf);
    } else if (fb->format == PIXFORMAT_RGB888) {
        memcpy(s_rgbBuf, fb->buf, fb->len);
        converted = true;
    }

    esp_camera_fb_return(fb);

    if (!converted) {
        return false;
    }

    downscaleTo64x64(s_rgbBuf, srcW, srcH);
    pxlcam::display::drawGrayscale64x64(s_img64);

    return true;
}



// ---------------------------------------------------------------------------
// PREVIEW MODE LOOP
// ---------------------------------------------------------------------------
void runPreviewLoop() {
    PXLCAM_LOGI("[PREVIEW] Entering PREVIEW MODE");

    pxlcam::display::printDisplay("Preview...", 1, 0, 0, true, false);
    delay(500);

    waitForButtonRelease(2000);

    while (true) {

        // Exit preview
        if (isButtonPressed()) {
            delay(50);
            if (isButtonPressed()) {
                PXLCAM_LOGI("[PREVIEW] Exit preview");
                waitForButtonRelease(300);
                return;
            }
        }

        frame();
        delay(kFrameDelayMs);
    }
}

}  // namespace pxlcam::preview
