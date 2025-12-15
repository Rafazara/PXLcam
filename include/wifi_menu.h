/**
 * @file wifi_menu.h
 * @brief WiFi Preview Submenu for PXLcam v1.3.0
 * 
 * Provides dedicated submenu for WiFi Preview control:
 * - Start/Stop WiFi Preview
 * - Show connection info (IP, password)
 * - Display QR code for easy connection
 * 
 * @version 1.3.0
 * @date 2024
 */

#pragma once

#include <stdint.h>

namespace pxlcam::wifi_menu {

//==============================================================================
// WiFi Menu Result Enum
//==============================================================================

/// Actions returned from WiFi submenu
enum class WifiMenuResult : uint8_t {
    START,          ///< Start WiFi Preview AP
    STOP,           ///< Stop WiFi Preview
    SHOW_INFO,      ///< Display SSID/Password/IP info
    SHOW_QR,        ///< Display QR code for connection
    BACK,           ///< Return to main menu
    CANCELLED       ///< Menu was cancelled (timeout)
};

//==============================================================================
// WiFi Menu Configuration
//==============================================================================

/// WiFi menu display settings
struct WifiMenuConfig {
    uint32_t longPressMs = 1000;      ///< Hold duration to select (ms)
    uint32_t autoCloseMs = 20000;     ///< Auto-close timeout (ms)
    uint32_t infoDisplayMs = 5000;    ///< How long to show info screen
    uint32_t qrDisplayMs = 15000;     ///< How long to show QR code
};

//==============================================================================
// Public API
//==============================================================================

/**
 * @brief Initialize WiFi menu system
 * @param config Optional configuration
 */
void init(const WifiMenuConfig* config = nullptr);

/**
 * @brief Show WiFi submenu (blocking modal)
 * 
 * Displays menu options:
 *   • Iniciar WiFi
 *   • Parar WiFi  
 *   • Info (IP/Senha)
 *   • QR Code
 *   • Voltar
 * 
 * @return WifiMenuResult indicating user selection
 */
WifiMenuResult showMenu();

/**
 * @brief Draw WiFi info screen (SSID, Password, IP)
 * @param ssid Network name
 * @param password Network password
 * @param ipAddress Current IP address (or "N/A")
 * @param isActive Whether WiFi is currently active
 */
void drawInfoScreen(const char* ssid, const char* password, 
                    const char* ipAddress, bool isActive);

/**
 * @brief Draw WiFi status overlay for idle screen
 * 
 * Shows compact WiFi indicator when preview is active.
 * Format: "WiFi: 192.168.4.1 (2)"
 *         IP address and client count
 * 
 * @param ipAddress Current IP address
 * @param clientCount Number of connected clients
 */
void drawStatusOverlay(const char* ipAddress, uint8_t clientCount);

/**
 * @brief Draw "WiFi Starting..." screen
 */
void drawStartingScreen();

/**
 * @brief Draw "WiFi Stopped" confirmation
 */
void drawStoppedScreen();

/**
 * @brief Get result name as string
 * @param result Menu result
 * @return Human-readable name
 */
const char* getResultName(WifiMenuResult result);

/**
 * @brief Check if WiFi menu is currently open
 * @return true if menu visible
 */
bool isOpen();

}  // namespace pxlcam::wifi_menu
