#include "preview.h"

#include <esp_camera.h>
#include <img_converters.h>
#include <esp_heap_caps.h>
#include <cstring>

#include "display.h"
#include "logging.h"
#include "pxlcam_config.h"

#if PXLCAM_DOUBLE_BUFFER_PREVIEW
#include "preview_buffer.h"
#endif

#if PXLCAM_GAMEBOY_DITHER
#include "preview_dither.h"
#endif

#include "fps_counter.h"
#include "display_ui.h"

namespace pxlcam::preview {

namespace {

constexpr int kPreviewW = 64;
constexpr int kPreviewH = 64;
constexpr int kFrameDelayMs = 50;  // ~20 FPS target (was 80ms / ~12 FPS in v1.0)
constexpr int kButtonPin = 12;
constexpr size_t kMaxRgbSize = 320 * 240 * 3;  // QVGA RGB888

// 64×64 grayscale buffer (1 byte per pixel) - used as fallback when double buffer disabled
static uint8_t s_img64[kPreviewW * kPreviewH];

// RGB buffer in PSRAM if available
static uint8_t* s_rgbBuf = nullptr;

// 1-bit packed bitmap for dithered output
static uint8_t s_bitmap1bit[(kPreviewW * kPreviewH + 7) / 8];

// FPS counter instance
static pxlcam::util::FPSCounter s_fpsCounter;

// Current preview mode
#if PXLCAM_GAMEBOY_DITHER
static pxlcam::dither::DitherMode s_ditherMode = pxlcam::dither::DitherMode::GameBoy;
#else
static int s_ditherMode = 0;
#endif


// ---------------------------------------------------------------------------
// Downscale RGB888 → 64×64 grayscale
// ---------------------------------------------------------------------------
void downscaleTo64x64(const uint8_t* rgb, int w, int h, uint8_t* outGray) {
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

            outGray[idx++] = (count > 0 ? sum / count : 0);
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
    }

#if PXLCAM_DOUBLE_BUFFER_PREVIEW
    // Initialize double buffer system
    if (!initBuffers()) {
        PXLCAM_LOGE("[PREVIEW] Double buffer init failed");
    } else {
        PXLCAM_LOGI("[PREVIEW] Double buffer initialized");
    }
#endif

#if PXLCAM_GAMEBOY_DITHER
    // Initialize dithering module
    pxlcam::dither::initDitherModule(true);
    PXLCAM_LOGI("[PREVIEW] Dither module initialized");
#endif

    // Initialize FPS counter
    s_fpsCounter.reset();
    
    // Initialize UI
    pxlcam::display::initUI();

    PXLCAM_LOGI("[PREVIEW] Module initialized OK");
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

#if PXLCAM_DOUBLE_BUFFER_PREVIEW
    // Get write buffer from double buffer system
    uint8_t* grayBuf = g_previewBuffer.getWriteBuffer();
    if (!grayBuf) {
        grayBuf = s_img64;  // Fallback
    }
#else
    uint8_t* grayBuf = s_img64;
#endif

    // Downscale to 64x64 grayscale
    downscaleTo64x64(s_rgbBuf, srcW, srcH, grayBuf);

#if PXLCAM_ENABLE_HISTEQ
    // Apply histogram equalization for better contrast
    pxlcam::dither::histogramEqualize(grayBuf, kPreviewW, kPreviewH);
#endif

#if PXLCAM_GAMEBOY_DITHER
    // Apply current dithering mode
    pxlcam::dither::processDither(grayBuf, kPreviewW, kPreviewH, s_bitmap1bit, false);
    
    // Draw 1-bit preview via UI module
    pxlcam::display::drawPreviewBitmap(s_bitmap1bit, kPreviewW, kPreviewH);
#else
    // Direct grayscale display (v1.0 style)
    pxlcam::display::drawGrayscale64x64(grayBuf);
#endif

#if PXLCAM_DOUBLE_BUFFER_PREVIEW
    // Commit write buffer (swap)
    g_previewBuffer.commitWrite();
#endif

    // Update FPS counter
    s_fpsCounter.tick();

    return true;
}



// ---------------------------------------------------------------------------
// PREVIEW MODE LOOP
// ---------------------------------------------------------------------------
void runPreviewLoop() {
    PXLCAM_LOGI("[PREVIEW] Entering PREVIEW MODE (v1.1.0)");

    pxlcam::display::printDisplay("Preview...", 1, 0, 0, true, false);
    delay(500);

    waitForButtonRelease(2000);

    // Initial UI setup
#if PXLCAM_GAMEBOY_DITHER
    pxlcam::display::PreviewMode uiMode = pxlcam::display::PreviewMode::GameBoy;
#else
    pxlcam::display::PreviewMode uiMode = pxlcam::display::PreviewMode::Auto;
#endif

    while (true) {

        // Exit preview on button press
        if (isButtonPressed()) {
            delay(50);
            if (isButtonPressed()) {
                PXLCAM_LOGI("[PREVIEW] Exit preview");
                waitForButtonRelease(300);
                return;
            }
        }

        // Capture and process frame
        frame();

#if PXLCAM_SHOW_FPS_OVERLAY
        // Draw FPS overlay
        int fps = s_fpsCounter.getFPS();
        pxlcam::display::drawStatusBar(fps, true, 100, uiMode);  // TODO: real SD/battery status
#endif

        // Swap display buffer
        pxlcam::display::swapDisplayBuffers();

        delay(kFrameDelayMs);
    }
}

}  // namespace pxlcam::preview
