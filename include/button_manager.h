#pragma once

#include <Arduino.h>

namespace pxlcam {

/// Button event types for v1.2.0
enum class ButtonEvent : uint8_t {
    None = 0,
    ShortPress,     ///< < 500ms tap
    LongPress,      ///< 500ms - 2000ms hold
    VeryLongPress   ///< >= 2000ms hold (menu/mode cycle)
};

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
    
    /// Get button event type (v1.2.0)
    /// @return Event type based on hold duration
    ButtonEvent getEvent() const;
    
    /// Consume and return button event
    /// @return Event type, then clears pending event
    ButtonEvent consumeEvent();
    
    /// Check if button is currently pressed
    bool isPressed() const { return latched_; }
    
    /// Get current hold duration in ms (0 if not held)
    uint32_t getHoldDuration() const;

   private:
    gpio_num_t pin_;
    uint8_t activeLevel_;
    uint32_t debounceMs_;
    bool latched_ = false;
    bool pendingPress_ = false;
    uint32_t lastTransitionMs_ = 0;
    uint32_t pressStartMs_ = 0;
    ButtonEvent pendingEvent_ = ButtonEvent::None;
};

}  // namespace pxlcam
