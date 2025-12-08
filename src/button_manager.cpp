#include "button_manager.h"

namespace pxlcam {

ButtonManager::ButtonManager(gpio_num_t pin, uint8_t activeLevel, uint32_t debounceMs)
    : pin_(pin), activeLevel_(activeLevel), debounceMs_(debounceMs) {}

void ButtonManager::begin() {
    pinMode(static_cast<uint8_t>(pin_), activeLevel_ == LOW ? INPUT_PULLUP : INPUT_PULLDOWN);
    latched_ = false;
    pendingPress_ = false;
    lastTransitionMs_ = millis();
}

void ButtonManager::update(uint32_t nowMs) {
    const uint8_t state = digitalRead(static_cast<uint8_t>(pin_));
    if (state == activeLevel_) {
        if (!latched_ && (nowMs - lastTransitionMs_) >= debounceMs_) {
            latched_ = true;
            pendingPress_ = true;
            lastTransitionMs_ = nowMs;
        }
    } else {
        if (latched_ && (nowMs - lastTransitionMs_) >= debounceMs_) {
            latched_ = false;
            lastTransitionMs_ = nowMs;
        }
    }
}

bool ButtonManager::consumePressed() {
    const bool wasPressed = pendingPress_;
    pendingPress_ = false;
    return wasPressed;
}

}  // namespace pxlcam
