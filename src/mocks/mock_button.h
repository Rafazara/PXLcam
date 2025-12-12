/**
 * @file mock_button.h
 * @brief Mock Button Implementation for PXLcam v1.2.0
 * 
 * Software-based button simulation for testing without hardware.
 * Supports timing-based press detection:
 * - Short press (<1s) → PRESS event
 * - Long press (1s) → LONG_PRESS event  
 * - Hold (2s) → HOLD event
 * 
 * @author PXLcam Team
 * @version 1.2.0
 * @date 2024
 */

#ifndef PXLCAM_MOCK_BUTTON_H
#define PXLCAM_MOCK_BUTTON_H

#include "../hal/hal_button.h"
#include <array>

namespace pxlcam {
namespace mocks {

/**
 * @brief Button timing thresholds (milliseconds)
 */
namespace ButtonTiming {
    constexpr uint32_t DEBOUNCE_MS = 50;       ///< Debounce time
    constexpr uint32_t LONG_PRESS_MS = 1000;   ///< Long press threshold (1s)
    constexpr uint32_t HOLD_MS = 2000;         ///< Hold threshold (2s)
}

/**
 * @brief Mock button implementation with timing
 * 
 * Simulates button hardware with proper timing detection.
 * For single-button navigation:
 * - Short press: Navigate to next item
 * - Long press (1s): Select item
 * - Hold (2s): Return to idle
 * 
 * @code
 * MockButton button;
 * button.init();
 * 
 * // Simulate button press (call in loop)
 * button.setButtonState(true);  // Button down
 * // ... time passes ...
 * button.setButtonState(false); // Button up
 * 
 * // Check events
 * auto event = button.getEvent(ButtonId::SHUTTER);
 * @endcode
 */
class MockButton : public hal::IButton {
public:
    /**
     * @brief Constructor
     */
    MockButton();

    /**
     * @brief Destructor
     */
    ~MockButton() override = default;

    // IButton interface implementation
    bool init() override;
    void update() override;
    hal::ButtonEvent getEvent(hal::ButtonId id) override;
    bool isPressed(hal::ButtonId id) override;
    void clearEvents() override;
    void setLongPressThreshold(uint32_t ms) override;
    void setDoublePressThreshold(uint32_t ms) override;

    /**
     * @brief Set button physical state (for timing detection)
     * @param id Button identifier
     * @param pressed True if button is currently pressed
     */
    void setButtonState(hal::ButtonId id, bool pressed);

    /**
     * @brief Simulate a button event directly (bypasses timing)
     * @param id Button identifier
     * @param event Event to simulate
     */
    void simulateEvent(hal::ButtonId id, hal::ButtonEvent event);

    /**
     * @brief Simulate button press state (legacy)
     * @param id Button identifier
     * @param pressed True if pressed
     */
    void simulatePress(hal::ButtonId id, bool pressed);

    /**
     * @brief Clear all simulated events and states
     */
    void reset();

    /**
     * @brief Check if mock is initialized
     * @return bool True if initialized
     */
    bool isInitialized() const { return initialized_; }

    /**
     * @brief Get press duration for a button
     * @param id Button identifier
     * @return uint32_t Duration in milliseconds (0 if not pressed)
     */
    uint32_t getPressDuration(hal::ButtonId id) const;

    /**
     * @brief Set hold threshold (for returning to idle)
     * @param ms Threshold in milliseconds
     */
    void setHoldThreshold(uint32_t ms) { holdThreshold_ = ms; }

private:
    static constexpr size_t BUTTON_COUNT = static_cast<size_t>(hal::ButtonId::BUTTON_COUNT);

    /**
     * @brief Internal button state tracking
     */
    struct ButtonState {
        bool currentState;      ///< Current physical state
        bool previousState;     ///< Previous physical state
        uint32_t pressStartTime; ///< When button was pressed
        bool longPressTriggered; ///< Long press already triggered
        bool holdTriggered;     ///< Hold already triggered
    };

    bool initialized_;
    uint32_t longPressThreshold_;
    uint32_t doublePressThreshold_;
    uint32_t holdThreshold_;

    std::array<hal::ButtonEvent, BUTTON_COUNT> events_;
    std::array<hal::ButtonEvent, BUTTON_COUNT> pendingEvents_;
    std::array<ButtonState, BUTTON_COUNT> buttonStates_;
};

} // namespace mocks
} // namespace pxlcam

#endif // PXLCAM_MOCK_BUTTON_H
