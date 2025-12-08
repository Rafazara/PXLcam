#include "pixel_filter.h"

#include <algorithm>

#include <esp_log.h>

#include "logging.h"

namespace pxlcam::filter {

namespace {

constexpr const char *kLogTag = "pxlcam-filter";
constexpr uint8_t kDefaultBlockSize = 8;

FilterConfig g_config{false, kDefaultBlockSize, 0};
bool g_initialized = false;

uint8_t clampByte(int value) {
    if (value < 0) return 0;
    if (value > 255) return 255;
    return static_cast<uint8_t>(value);
}

bool isRgbFramebuffer(const camera_fb_t *frame) {
    return frame != nullptr && frame->format == PIXFORMAT_RGB888;
}

}  // namespace

void init(const FilterConfig &config) {
    g_config = config;
    if (g_config.blockSize == 0) {
        g_config.blockSize = kDefaultBlockSize;
    }
    g_initialized = true;
}

void applyPixelFilter(uint8_t *rgbBuffer, size_t length, uint16_t width, uint16_t height, uint8_t blockSize, uint8_t brightnessOffset) {
    if (rgbBuffer == nullptr || length == 0 || width == 0 || height == 0) {
        return;
    }

    const uint8_t block = blockSize == 0 ? kDefaultBlockSize : blockSize;
    const uint32_t stride = static_cast<uint32_t>(width) * 3;

    for (uint16_t y = 0; y < height; y += block) {
        const uint16_t blockHeight = std::min<uint16_t>(block, height - y);

        for (uint16_t x = 0; x < width; x += block) {
            const uint16_t blockWidth = std::min<uint16_t>(block, width - x);

            uint32_t sumR = 0;
            uint32_t sumG = 0;
            uint32_t sumB = 0;
            uint32_t pixelCount = 0;

            for (uint16_t by = 0; by < blockHeight; ++by) {
                const uint32_t rowOffset = static_cast<uint32_t>(y + by) * stride;
                const uint32_t colOffset = static_cast<uint32_t>(x) * 3;
                const uint8_t *rowPtr = rgbBuffer + rowOffset + colOffset;

                for (uint16_t bx = 0; bx < blockWidth; ++bx) {
                    const uint8_t *pixel = rowPtr + (bx * 3);
                    sumR += pixel[0];
                    sumG += pixel[1];
                    sumB += pixel[2];
                    ++pixelCount;
                }
            }

            if (pixelCount == 0) {
                continue;
            }

            const uint8_t avgR = clampByte(static_cast<int>(sumR / pixelCount) + brightnessOffset);
            const uint8_t avgG = clampByte(static_cast<int>(sumG / pixelCount) + brightnessOffset);
            const uint8_t avgB = clampByte(static_cast<int>(sumB / pixelCount) + brightnessOffset);

            for (uint16_t by = 0; by < blockHeight; ++by) {
                const uint32_t rowOffset = static_cast<uint32_t>(y + by) * stride;
                const uint32_t colOffset = static_cast<uint32_t>(x) * 3;
                uint8_t *rowPtr = rgbBuffer + rowOffset + colOffset;

                for (uint16_t bx = 0; bx < blockWidth; ++bx) {
                    uint8_t *pixel = rowPtr + (bx * 3);
                    pixel[0] = avgR;
                    pixel[1] = avgG;
                    pixel[2] = avgB;
                }
            }
        }
    }
}

void apply(camera_fb_t *frame) {
    if (!g_initialized || !g_config.enabled) {
        return;
    }

    if (!isRgbFramebuffer(frame)) {
        PXLCAM_LOGW_TAG(kLogTag, "apply called with unsupported framebuffer format: %d", frame ? frame->format : -1);
        return;
    }

    applyPixelFilter(frame->buf, frame->len, frame->width, frame->height, g_config.blockSize, g_config.brightnessOffset);
}

void reset() {
    g_config = FilterConfig{false, kDefaultBlockSize, 0};
    g_initialized = false;
}

}  // namespace pxlcam::filter
