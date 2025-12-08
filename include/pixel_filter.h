#pragma once

#include <cstddef>

#include <esp_camera.h>

namespace pxlcam::filter {

// Image filtering pipeline for optional frame post-processing.
struct FilterConfig {
    bool enabled;
    uint8_t blockSize;         // Size of pixel blocks; defaults to 8 for ESP32-friendly workload.
    uint8_t brightnessOffset;  // Optional post-adjustment applied to the averaged RGB values.
};

void init(const FilterConfig &config);
void apply(camera_fb_t *frame);
void reset();

void applyPixelFilter(uint8_t *rgbBuffer, size_t length, uint16_t width, uint16_t height, uint8_t blockSize, uint8_t brightnessOffset);

}  // namespace pxlcam::filter
