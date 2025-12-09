#pragma once

#include <Arduino.h>

namespace pxlcam::preview {

/// Initializes preview system (buffers only)
void begin();

/// Runs one preview frame: capture, convert, draw
/// Returns true on success
bool frame();

/// Continuous preview loop (blocking until button press)
void runPreviewLoop();

}  // namespace pxlcam::preview
