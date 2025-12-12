/**
 * @file hal_button.h
 * @brief Hardware Abstraction Layer - Button Interface for PXLcam v1.2.0
 * 
 * Abstract interface for button input handling.
 * Implementations can be hardware-based or mocked for testing.
 * 
 * @author PXLcam Team
 * @version 1.2.0
 * @date 2024
 */

#ifndef PXLCAM_HAL_BUTTON_H
#define PXLCAM_HAL_BUTTON_H

#include <cstdint>

namespace pxlcam {
namespace hal {

/**
 * @brief Button event types
 */
enum class ButtonEvent : uint8_t {
    NONE = 0,       ///< No event
    PRESS,          ///< Single press
    RELEASE,        ///< Button released
    LONG_PRESS,     ///< Long press (>500ms)
    DOUBLE_PRESS,   ///< Double press
    HOLD            ///< Button held down
};

/**
 * @brief Button identifiers
 */
enum class ButtonId : uint8_t {
    SHUTTER = 0,    ///< Main shutter/select button
    MODE,           ///< Mode/menu button
    UP,             ///< Navigation up
    DOWN,           ///< Navigation down
    BUTTON_COUNT    ///< Number of buttons (sentinel)
};

/**
 * @brief Abstract button interface
 * 
 * Interface for button hardware abstraction.
 * Implement this for hardware or mock buttons.
 */
class IButton {
public:
    virtual ~IButton() = default;

    /**
     * @brief Initialize the button hardware
     * @return bool True if successful
     */
    virtual bool init() = 0;

    /**
     * @brief Update button state (call in main loop)
     */
    virtual void update() = 0;

    /**
     * @brief Get current button event
     * @param id Button identifier
     * @return ButtonEvent Current event
     */
    virtual ButtonEvent getEvent(ButtonId id) = 0;

    /**
     * @brief Check if button is currently pressed
     * @param id Button identifier
     * @return bool True if pressed
     */
    virtual bool isPressed(ButtonId id) = 0;

    /**
     * @brief Clear pending events
     */
    virtual void clearEvents() = 0;

    /**
     * @brief Set long press threshold in milliseconds
     * @param ms Threshold in milliseconds
     */
    virtual void setLongPressThreshold(uint32_t ms) = 0;

    /**
     * @brief Set double press threshold in milliseconds
     * @param ms Threshold in milliseconds
     */
    virtual void setDoublePressThreshold(uint32_t ms) = 0;
};

} // namespace hal
} // namespace pxlcam

#endif // PXLCAM_HAL_BUTTON_H
