/**
 * @file mock_button.cpp
 * @brief Mock Button Implementation for PXLcam v1.2.0
 * 
 * @author PXLcam Team
 * @version 1.2.0
 * @date 2024
 */

#include "mock_button.h"
#include <Arduino.h>

namespace pxlcam {
namespace mocks {

MockButton::MockButton()
    : initialized_(false)
    , longPressThreshold_(500)
    , doublePressThreshold_(300)
{
    events_.fill(hal::ButtonEvent::NONE);
    pendingEvents_.fill(hal::ButtonEvent::NONE);
    pressedState_.fill(false);
}

bool MockButton::init() {
    Serial.println("[MockButton] Initialized");
    initialized_ = true;
    reset();
    return true;
}

void MockButton::update() {
    if (!initialized_) return;

    // Transfer pending events to current events
    for (size_t i = 0; i < BUTTON_COUNT; ++i) {
        events_[i] = pendingEvents_[i];
        pendingEvents_[i] = hal::ButtonEvent::NONE;
    }
}

hal::ButtonEvent MockButton::getEvent(hal::ButtonId id) {
    if (!initialized_) return hal::ButtonEvent::NONE;
    
    size_t idx = static_cast<size_t>(id);
    if (idx >= BUTTON_COUNT) return hal::ButtonEvent::NONE;

    hal::ButtonEvent event = events_[idx];
    events_[idx] = hal::ButtonEvent::NONE;  // Clear after reading
    return event;
}

bool MockButton::isPressed(hal::ButtonId id) {
    if (!initialized_) return false;
    
    size_t idx = static_cast<size_t>(id);
    if (idx >= BUTTON_COUNT) return false;
    
    return pressedState_[idx];
}

void MockButton::clearEvents() {
    events_.fill(hal::ButtonEvent::NONE);
    pendingEvents_.fill(hal::ButtonEvent::NONE);
}

void MockButton::setLongPressThreshold(uint32_t ms) {
    longPressThreshold_ = ms;
}

void MockButton::setDoublePressThreshold(uint32_t ms) {
    doublePressThreshold_ = ms;
}

void MockButton::simulateEvent(hal::ButtonId id, hal::ButtonEvent event) {
    size_t idx = static_cast<size_t>(id);
    if (idx >= BUTTON_COUNT) return;

    pendingEvents_[idx] = event;
    Serial.printf("[MockButton] Simulated event: Button %d -> %d\n", idx, static_cast<int>(event));
}

void MockButton::simulatePress(hal::ButtonId id, bool pressed) {
    size_t idx = static_cast<size_t>(id);
    if (idx >= BUTTON_COUNT) return;

    pressedState_[idx] = pressed;
    Serial.printf("[MockButton] Button %d pressed: %s\n", idx, pressed ? "true" : "false");
}

void MockButton::reset() {
    events_.fill(hal::ButtonEvent::NONE);
    pendingEvents_.fill(hal::ButtonEvent::NONE);
    pressedState_.fill(false);
    Serial.println("[MockButton] Reset");
}

} // namespace mocks
} // namespace pxlcam
