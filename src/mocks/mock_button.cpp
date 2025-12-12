/**
 * @file mock_button.cpp
 * @brief Mock Button Implementation for PXLcam v1.2.0
 * 
 * Implements timing-based button detection:
 * - Short press (<1s): PRESS event on release
 * - Long press (1s): LONG_PRESS event while held
 * - Hold (2s): HOLD event while held
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
    , longPressThreshold_(ButtonTiming::LONG_PRESS_MS)
    , doublePressThreshold_(300)
    , holdThreshold_(ButtonTiming::HOLD_MS)
{
    events_.fill(hal::ButtonEvent::NONE);
    pendingEvents_.fill(hal::ButtonEvent::NONE);
    
    for (auto& state : buttonStates_) {
        state.currentState = false;
        state.previousState = false;
        state.pressStartTime = 0;
        state.longPressTriggered = false;
        state.holdTriggered = false;
    }
}

bool MockButton::init() {
    Serial.println("[MockButton] Initialized with timing detection");
    Serial.printf("[MockButton] Long press: %lums, Hold: %lums\n", 
                  longPressThreshold_, holdThreshold_);
    initialized_ = true;
    reset();
    return true;
}

void MockButton::update() {
    if (!initialized_) return;

    uint32_t now = millis();

    for (size_t i = 0; i < BUTTON_COUNT; ++i) {
        auto& state = buttonStates_[i];
        
        // Check for direct event injection first
        if (pendingEvents_[i] != hal::ButtonEvent::NONE) {
            events_[i] = pendingEvents_[i];
            pendingEvents_[i] = hal::ButtonEvent::NONE;
            continue;
        }
        
        // Button just pressed
        if (state.currentState && !state.previousState) {
            state.pressStartTime = now;
            state.longPressTriggered = false;
            state.holdTriggered = false;
            Serial.printf("[MockButton] Button %zu pressed\n", i);
        }
        
        // Button held down - check thresholds
        if (state.currentState && state.pressStartTime > 0) {
            uint32_t duration = now - state.pressStartTime;
            
            // Check for HOLD (2s) - highest priority
            if (duration >= holdThreshold_ && !state.holdTriggered) {
                events_[i] = hal::ButtonEvent::HOLD;
                state.holdTriggered = true;
                Serial.printf("[MockButton] Button %zu HOLD detected (%lums)\n", i, duration);
            }
            // Check for LONG_PRESS (1s)
            else if (duration >= longPressThreshold_ && !state.longPressTriggered && !state.holdTriggered) {
                events_[i] = hal::ButtonEvent::LONG_PRESS;
                state.longPressTriggered = true;
                Serial.printf("[MockButton] Button %zu LONG_PRESS detected (%lums)\n", i, duration);
            }
        }
        
        // Button just released
        if (!state.currentState && state.previousState) {
            uint32_t duration = now - state.pressStartTime;
            
            // Only generate PRESS if we didn't trigger LONG_PRESS or HOLD
            if (!state.longPressTriggered && !state.holdTriggered && duration >= ButtonTiming::DEBOUNCE_MS) {
                events_[i] = hal::ButtonEvent::PRESS;
                Serial.printf("[MockButton] Button %zu SHORT PRESS detected (%lums)\n", i, duration);
            }
            
            state.pressStartTime = 0;
            state.longPressTriggered = false;
            state.holdTriggered = false;
            Serial.printf("[MockButton] Button %zu released\n", i);
        }
        
        state.previousState = state.currentState;
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
    
    return buttonStates_[idx].currentState;
}

void MockButton::clearEvents() {
    events_.fill(hal::ButtonEvent::NONE);
    pendingEvents_.fill(hal::ButtonEvent::NONE);
}

void MockButton::setLongPressThreshold(uint32_t ms) {
    longPressThreshold_ = ms;
    Serial.printf("[MockButton] Long press threshold set to %lums\n", ms);
}

void MockButton::setDoublePressThreshold(uint32_t ms) {
    doublePressThreshold_ = ms;
}

void MockButton::setButtonState(hal::ButtonId id, bool pressed) {
    size_t idx = static_cast<size_t>(id);
    if (idx >= BUTTON_COUNT) return;

    buttonStates_[idx].currentState = pressed;
}

void MockButton::simulateEvent(hal::ButtonId id, hal::ButtonEvent event) {
    size_t idx = static_cast<size_t>(id);
    if (idx >= BUTTON_COUNT) return;

    pendingEvents_[idx] = event;
    Serial.printf("[MockButton] Simulated event: Button %zu -> %d\n", idx, static_cast<int>(event));
}

void MockButton::simulatePress(hal::ButtonId id, bool pressed) {
    setButtonState(id, pressed);
}

void MockButton::reset() {
    events_.fill(hal::ButtonEvent::NONE);
    pendingEvents_.fill(hal::ButtonEvent::NONE);
    
    for (auto& state : buttonStates_) {
        state.currentState = false;
        state.previousState = false;
        state.pressStartTime = 0;
        state.longPressTriggered = false;
        state.holdTriggered = false;
    }
    
    Serial.println("[MockButton] Reset");
}

uint32_t MockButton::getPressDuration(hal::ButtonId id) const {
    size_t idx = static_cast<size_t>(id);
    if (idx >= BUTTON_COUNT) return 0;
    
    const auto& state = buttonStates_[idx];
    if (!state.currentState || state.pressStartTime == 0) return 0;
    
    return millis() - state.pressStartTime;
}

} // namespace mocks
} // namespace pxlcam
