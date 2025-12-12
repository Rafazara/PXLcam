/**
 * @file mock_button.h
 * @brief Mock Button Implementation for PXLcam v1.2.0
 * 
 * Software-based button simulation for testing without hardware.
 * Allows programmatic triggering of button events.
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
 * @brief Mock button implementation
 * 
 * Simulates button hardware for testing.
 * Use simulateEvent() to trigger button events programmatically.
 * 
 * @code
 * MockButton buttons;
 * buttons.init();
 * buttons.simulateEvent(ButtonId::SHUTTER, ButtonEvent::PRESS);
 * buttons.update();
 * auto event = buttons.getEvent(ButtonId::SHUTTER);
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
     * @brief Simulate a button event (for testing)
     * @param id Button identifier
     * @param event Event to simulate
     */
    void simulateEvent(hal::ButtonId id, hal::ButtonEvent event);

    /**
     * @brief Simulate button press state
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

private:
    static constexpr size_t BUTTON_COUNT = static_cast<size_t>(hal::ButtonId::BUTTON_COUNT);

    bool initialized_;
    uint32_t longPressThreshold_;
    uint32_t doublePressThreshold_;

    std::array<hal::ButtonEvent, BUTTON_COUNT> events_;
    std::array<hal::ButtonEvent, BUTTON_COUNT> pendingEvents_;
    std::array<bool, BUTTON_COUNT> pressedState_;
};

} // namespace mocks
} // namespace pxlcam

#endif // PXLCAM_MOCK_BUTTON_H
