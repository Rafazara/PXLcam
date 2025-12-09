#pragma once

#include <Arduino.h>

namespace pxlcam {

// Simple GPIO button handler with software debounce.
// Note: avoid holding the capture button during boot (GPIO12 is a strapping pin).
class ButtonManager {
   public:
    ButtonManager(gpio_num_t pin, uint8_t activeLevel, uint32_t debounceMs);

    void begin();
    void update(uint32_t nowMs);
    bool consumePressed();
    
    /// Returns true if button has been held for at least holdMs milliseconds
    bool held(uint32_t holdMs) const;

   private:
    gpio_num_t pin_;
    uint8_t activeLevel_;
    uint32_t debounceMs_;
    bool latched_ = false;
    bool pendingPress_ = false;
    uint32_t lastTransitionMs_ = 0;
    uint32_t pressStartMs_ = 0;
};

}  // namespace pxlcam
