/**
 * @file state_machine.cpp
 * @brief Generic State Machine implementation for PXLcam v1.2.0
 * 
 * @author PXLcam Team
 * @version 1.2.0
 * @date 2024
 */

#include "state_machine.h"
#include <Arduino.h>

namespace pxlcam {
namespace core {

const char* stateToString(State state) {
    switch (state) {
        case State::BOOT:       return "BOOT";
        case State::IDLE:       return "IDLE";
        case State::MENU:       return "MENU";
        case State::PREVIEW:    return "PREVIEW";
        case State::CAPTURE:    return "CAPTURE";
        default:                return "UNKNOWN";
    }
}

const char* eventToString(Event event) {
    switch (event) {
        case Event::NONE:               return "NONE";
        case Event::BOOT_COMPLETE:      return "BOOT_COMPLETE";
        case Event::BUTTON_PRESS:       return "BUTTON_PRESS";
        case Event::BUTTON_LONG_PRESS:  return "BUTTON_LONG_PRESS";
        case Event::BUTTON_HOLD:        return "BUTTON_HOLD";
        case Event::BUTTON_DOUBLE_PRESS:return "BUTTON_DOUBLE_PRESS";
        case Event::MENU_NAV:           return "MENU_NAV";
        case Event::MENU_SELECT:        return "MENU_SELECT";
        case Event::MENU_BACK:          return "MENU_BACK";
        case Event::CAPTURE_COMPLETE:   return "CAPTURE_COMPLETE";
        case Event::TIMEOUT:            return "TIMEOUT";
        case Event::ERROR:              return "ERROR";
        default:                        return "UNKNOWN";
    }
}

StateMachine::StateMachine()
    : currentState_(State::BOOT)
    , previousState_(State::BOOT)
    , running_(false)
{
}

void StateMachine::registerState(State state, const StateConfig& config) {
    states_[state] = config;
    Serial.printf("[SM] State registered: %s\n", stateToString(state));
}

void StateMachine::start(State initialState) {
    if (states_.find(initialState) == states_.end()) {
        Serial.printf("[SM] ERROR: State %s not registered!\n", stateToString(initialState));
        return;
    }

    currentState_ = initialState;
    previousState_ = initialState;
    running_ = true;

    Serial.printf("[SM] Starting in state: %s\n", stateToString(currentState_));

    // Call onEnter for initial state
    auto& config = states_[currentState_];
    if (config.onEnter) {
        config.onEnter();
    }
}

void StateMachine::update() {
    if (!running_) return;

    auto it = states_.find(currentState_);
    if (it != states_.end() && it->second.onUpdate) {
        it->second.onUpdate();
    }
}

void StateMachine::handleEvent(Event event) {
    if (!running_ || event == Event::NONE) return;

    Serial.printf("[SM] Event: %s in state %s\n", 
                  eventToString(event), stateToString(currentState_));

    auto it = states_.find(currentState_);
    if (it != states_.end() && it->second.onEvent) {
        State nextState = it->second.onEvent(event);
        if (nextState != currentState_) {
            transitionTo(nextState);
        }
    }
}

void StateMachine::transitionTo(State newState) {
    if (!running_) return;

    if (states_.find(newState) == states_.end()) {
        Serial.printf("[SM] ERROR: Cannot transition to unregistered state %s\n", 
                      stateToString(newState));
        return;
    }

    Serial.printf("[SM] Transition: %s -> %s\n", 
                  stateToString(currentState_), stateToString(newState));

    // Call onExit for current state
    auto& currentConfig = states_[currentState_];
    if (currentConfig.onExit) {
        currentConfig.onExit();
    }

    // Update state
    previousState_ = currentState_;
    currentState_ = newState;

    // Call onEnter for new state
    auto& newConfig = states_[currentState_];
    if (newConfig.onEnter) {
        newConfig.onEnter();
    }
}

void StateMachine::stop() {
    if (!running_) return;

    Serial.println("[SM] Stopping state machine");

    // Call onExit for current state
    auto it = states_.find(currentState_);
    if (it != states_.end() && it->second.onExit) {
        it->second.onExit();
    }

    running_ = false;
}

} // namespace core
} // namespace pxlcam
