#pragma once
/**
 * @file ui_menu.h
 * @brief OLED menu system for PXLcam v1.2.0
 * 
 * Provides a simple navigation menu for mode selection and settings.
 * Uses single button for navigation: tap to select, hold to confirm/back.
 */

#include <stdint.h>
#include "mode_manager.h"

namespace pxlcam::ui {

/// Menu states
enum class MenuState : uint8_t {
    Hidden = 0,         ///< Menu not visible
    ModeSelect,         ///< Mode selection screen
    Confirmation,       ///< Confirm mode change
    Settings,           ///< Settings submenu (future)
    Info                ///< Info/about screen
};

/// Menu action results
enum class MenuAction : uint8_t {
    None = 0,
    ModeChanged,
    MenuClosed,
    BackPressed
};

/// Initialize menu system
void init();

/// Check if menu is currently visible
bool isMenuVisible();

/// Show the mode selection menu
void showModeMenu();

/// Hide menu and return to normal view
void hideMenu();

/// Handle button tap (short press)
/// @return Action result
MenuAction handleTap();

/// Handle button hold (long press)
/// @return Action result
MenuAction handleHold();

/// Update menu display (call in main loop when menu visible)
void updateDisplay();

/// Get currently highlighted menu item index
int getSelectedIndex();

/// Set selected menu item
/// @param index Item index (0 = Normal, 1 = GameBoy, 2 = Night)
void setSelectedIndex(int index);

/// Draw mode selection screen
void drawModeSelectScreen();

/// Draw confirmation dialog
/// @param modeName Name of mode being confirmed
void drawConfirmScreen(const char* modeName);

/// Draw error screen with message
/// @param title Error title
/// @param message Error details
/// @param canRetry If true, show retry option
void drawErrorScreen(const char* title, const char* message, bool canRetry = true);

/// Draw success screen with message
/// @param title Success title
/// @param message Success details
/// @param durationMs How long to show (0 = until dismissed)
void drawSuccessScreen(const char* title, const char* message, uint16_t durationMs = 1500);

/// Draw progress screen
/// @param title Operation title
/// @param progress Progress percentage (0-100)
void drawProgressScreen(const char* title, uint8_t progress);

/// Draw info screen with device stats
void drawInfoScreen();

/// Get current menu state
MenuState getMenuState();

}  // namespace pxlcam::ui
