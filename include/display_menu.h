/**
 * @file display_menu.h
 * @brief Modal menu system for PXLcam v1.2.0
 * 
 * Provides a clean, modal menu interface for mode selection.
 * 
 * Features:
 * - Vertical list: [ GameBoy ], [ Night ], [ Normal ]
 * - Navigation: short click → next item, long click (1s) → select
 * - Visual indicator ">" on current item
 * - Fade-in animation on open
 * - Modal operation: blocks AppController until selection
 * 
 * Usage:
 *   MenuResult result = displayMenu::showModal();
 *   switch (result) {
 *     case MODE_GAMEBOY: ...
 *     case MODE_NIGHT: ...
 *     case MODE_NORMAL: ...
 *   }
 */

#pragma once

#include <stdint.h>

namespace pxlcam::menu {

//==============================================================================
// Menu Result Enum - Final selection return values
//==============================================================================

/// Return values from modal menu
enum MenuResult : uint8_t {
    MODE_GAMEBOY = 0,   ///< GameBoy dithering mode selected
    MODE_NIGHT   = 1,   ///< Night vision mode selected
    MODE_NORMAL  = 2,   ///< Normal capture mode selected
    MODE_TIMELAPSE = 3, ///< Timelapse mode selected (v1.3.0)
    MODE_CANCELLED = 4  ///< Menu was cancelled (timeout/back)
};

//==============================================================================
// Menu Configuration
//==============================================================================

/// Menu behavior configuration
struct MenuConfig {
    uint32_t longPressMs = 1000;      ///< Hold duration to select (ms)
    uint32_t autoCloseMs = 15000;     ///< Auto-close timeout (ms), 0=disabled
    bool enableFadeIn = true;         ///< Enable fade-in animation
    uint8_t fadeSteps = 3;            ///< Number of fade steps (1-5)
    uint16_t fadeDelayMs = 50;        ///< Delay between fade steps (ms)
};

//==============================================================================
// Public API
//==============================================================================

/**
 * @brief Initialize menu system
 * @param config Optional configuration (uses defaults if nullptr)
 */
void init(const MenuConfig* config = nullptr);

/**
 * @brief Show modal menu and block until selection
 * 
 * This is the main entry point. Displays the mode selection menu
 * and blocks execution until user makes a selection or menu times out.
 * 
 * Button behavior:
 * - Short press (<1s): Move to next item
 * - Long press (≥1s): Select current item and return
 * 
 * @return MenuResult indicating which mode was selected
 */
MenuResult showModal();

/**
 * @brief Show modal menu starting at specific item
 * @param initialIndex Starting item index (0=GameBoy, 1=Night, 2=Normal)
 * @return MenuResult indicating which mode was selected
 */
MenuResult showModalAt(uint8_t initialIndex);

/**
 * @brief Check if menu is currently open (for external queries)
 * @return true if menu is visible
 */
bool isOpen();

/**
 * @brief Force close menu without selection
 * Sets result to MODE_CANCELLED
 */
void forceClose();

/**
 * @brief Get name string for a menu result
 * @param result Menu result value
 * @return Human-readable name
 */
const char* getResultName(MenuResult result);

/**
 * @brief Convert MenuResult to mode_manager::CaptureMode value
 * @param result Menu result
 * @return Corresponding CaptureMode uint8_t value
 */
uint8_t toCaptureModeValue(MenuResult result);

/**
 * @brief Convert mode_manager::CaptureMode to MenuResult
 * @param modeValue CaptureMode value (0=Normal, 1=GameBoy, 2=Night)
 * @return Corresponding MenuResult
 */
MenuResult fromCaptureModeValue(uint8_t modeValue);

//==============================================================================
// Debug / Testing
//==============================================================================

/**
 * @brief Enable or disable verbose logging
 * @param enable true to enable detailed logs
 */
void setDebugLogging(bool enable);

/**
 * @brief Get number of items in menu
 * @return Item count (always 3)
 */
uint8_t getItemCount();

/**
 * @brief Get currently highlighted index
 * @return Current selection index
 */
uint8_t getCurrentIndex();

}  // namespace pxlcam::menu
