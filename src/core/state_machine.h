/**
 * @file state_machine.h
 * @brief Generic State Machine for PXLcam v1.2.0
 * 
 * Provides a flexible, event-driven state machine with:
 * - State registration and transitions
 * - Event handling with custom handlers
 * - Entry/Exit callbacks per state
 * 
 * @author PXLcam Team
 * @version 1.2.0
 * @date 2024
 */

#ifndef PXLCAM_STATE_MACHINE_H
#define PXLCAM_STATE_MACHINE_H

#include <cstdint>
#include <functional>
#include <map>
#include <vector>

namespace pxlcam {
namespace core {

/**
 * @brief Application states enumeration
 */
enum class State : uint8_t {
    BOOT = 0,       ///< Initial boot state
    IDLE,           ///< Idle/standby state
    MENU,           ///< Menu navigation state
    PREVIEW,        ///< Camera preview state
    CAPTURE,        ///< Photo capture state
    STATE_COUNT     ///< Number of states (sentinel)
};

/**
 * @brief System events enumeration
 */
enum class Event : uint8_t {
    NONE = 0,           ///< No event
    BOOT_COMPLETE,      ///< Boot sequence finished
    BUTTON_PRESS,       ///< Single button press
    BUTTON_LONG_PRESS,  ///< Long button press
    BUTTON_DOUBLE_PRESS,///< Double button press
    MENU_SELECT,        ///< Menu item selected
    MENU_BACK,          ///< Menu back navigation
    CAPTURE_COMPLETE,   ///< Capture finished
    TIMEOUT,            ///< Timeout event
    ERROR,              ///< Error occurred
    EVENT_COUNT         ///< Number of events (sentinel)
};

/**
 * @brief Convert State to string for debugging
 * @param state The state to convert
 * @return const char* String representation
 */
const char* stateToString(State state);

/**
 * @brief Convert Event to string for debugging
 * @param event The event to convert
 * @return const char* String representation
 */
const char* eventToString(Event event);

/**
 * @brief State handler callback types
 */
using StateHandler = std::function<void()>;
using EventHandler = std::function<State(Event)>;

/**
 * @brief State configuration structure
 */
struct StateConfig {
    StateHandler onEnter;       ///< Called when entering state
    StateHandler onExit;        ///< Called when exiting state
    StateHandler onUpdate;      ///< Called every update cycle
    EventHandler onEvent;       ///< Event handler returning next state
};

/**
 * @brief Generic State Machine class
 * 
 * Usage:
 * @code
 * StateMachine sm;
 * sm.registerState(State::BOOT, {
 *     .onEnter = []() { Serial.println("Boot started"); },
 *     .onUpdate = []() { // do boot stuff },
 *     .onEvent = [](Event e) { 
 *         if (e == Event::BOOT_COMPLETE) return State::IDLE;
 *         return State::BOOT;
 *     }
 * });
 * sm.start(State::BOOT);
 * // In loop: sm.update(); sm.handleEvent(event);
 * @endcode
 */
class StateMachine {
public:
    /**
     * @brief Default constructor
     */
    StateMachine();

    /**
     * @brief Destructor
     */
    ~StateMachine() = default;

    /**
     * @brief Register a state with its configuration
     * @param state The state to register
     * @param config State configuration with handlers
     */
    void registerState(State state, const StateConfig& config);

    /**
     * @brief Start the state machine in the given state
     * @param initialState The initial state
     */
    void start(State initialState);

    /**
     * @brief Update the current state (call in main loop)
     */
    void update();

    /**
     * @brief Handle an event, potentially triggering state transition
     * @param event The event to handle
     */
    void handleEvent(Event event);

    /**
     * @brief Force transition to a new state
     * @param newState The state to transition to
     */
    void transitionTo(State newState);

    /**
     * @brief Get current state
     * @return State Current state
     */
    State getCurrentState() const { return currentState_; }

    /**
     * @brief Get previous state
     * @return State Previous state
     */
    State getPreviousState() const { return previousState_; }

    /**
     * @brief Check if state machine is running
     * @return bool True if running
     */
    bool isRunning() const { return running_; }

    /**
     * @brief Stop the state machine
     */
    void stop();

private:
    std::map<State, StateConfig> states_;   ///< Registered states
    State currentState_;                     ///< Current state
    State previousState_;                    ///< Previous state
    bool running_;                           ///< Running flag
};

} // namespace core
} // namespace pxlcam

#endif // PXLCAM_STATE_MACHINE_H
