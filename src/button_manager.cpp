#include "button_manager.h"
#include "pxlcam_config.h"

namespace pxlcam {

namespace {
constexpr uint32_t kShortPressMaxMs = 500;
constexpr uint32_t kLongPressMinMs = 500;
constexpr uint32_t kVeryLongPressMs = PXLCAM_MENU_HOLD_MS;
}

ButtonManager::ButtonManager(gpio_num_t pin, uint8_t activeLevel, uint32_t debounceMs)
    : pin_(pin), activeLevel_(activeLevel), debounceMs_(debounceMs) {}

void ButtonManager::begin() {
    pinMode(static_cast<uint8_t>(pin_), activeLevel_ == LOW ? INPUT_PULLUP : INPUT_PULLDOWN);
    latched_ = false;
    pendingPress_ = false;
    pendingEvent_ = ButtonEvent::None;
    lastTransitionMs_ = millis();
    pressStartMs_ = 0;
}

void ButtonManager::update(uint32_t nowMs) {
    const uint8_t state = digitalRead(static_cast<uint8_t>(pin_));
    if (state == activeLevel_) {
        // Button is pressed
        if (!latched_ && (nowMs - lastTransitionMs_) >= debounceMs_) {
            latched_ = true;
            pendingPress_ = true;
            pressStartMs_ = nowMs;
            lastTransitionMs_ = nowMs;
        }
    } else {
        // Button released
        if (latched_ && (nowMs - lastTransitionMs_) >= debounceMs_) {
            // Calculate hold duration and determine event type
            uint32_t holdDuration = nowMs - pressStartMs_;
            
            if (holdDuration >= kVeryLongPressMs) {
                pendingEvent_ = ButtonEvent::VeryLongPress;
            } else if (holdDuration >= kLongPressMinMs) {
                pendingEvent_ = ButtonEvent::LongPress;
            } else {
                pendingEvent_ = ButtonEvent::ShortPress;
            }
            
            latched_ = false;
            pressStartMs_ = 0;
            lastTransitionMs_ = nowMs;
        }
    }
}

bool ButtonManager::consumePressed() {
    const bool wasPressed = pendingPress_;
    pendingPress_ = false;
    return wasPressed;
}

bool ButtonManager::held(uint32_t holdMs) const {
    if (!latched_ || pressStartMs_ == 0) {
        return false;
    }
    return (millis() - pressStartMs_) >= holdMs;
}

ButtonEvent ButtonManager::getEvent() const {
    return pendingEvent_;
}

ButtonEvent ButtonManager::consumeEvent() {
    ButtonEvent event = pendingEvent_;
    pendingEvent_ = ButtonEvent::None;
    return event;
}

uint32_t ButtonManager::getHoldDuration() const {
    if (!latched_ || pressStartMs_ == 0) {
        return 0;
    }
    return millis() - pressStartMs_;
}

}  // namespace pxlcam
