/**
 * @file wifi_qrcode.h
 * @brief WiFi QR Code Generator for PXLcam v1.3.0
 * 
 * Generates and displays QR codes on the OLED for easy WiFi connection.
 * QR code follows the WiFi standard format:
 *   WIFI:T:WPA;S:<ssid>;P:<password>;;
 * 
 * @version 1.3.0
 * @date 2024
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

namespace pxlcam::wifi_qr {

//==============================================================================
// QR Code Configuration
//==============================================================================

/// QR Code version (21x21 is version 1)
constexpr uint8_t kQrVersion = 1;
constexpr uint8_t kQrSize = 21;

/// WiFi authentication types for QR format
enum class WifiAuthType : uint8_t {
    OPEN,   ///< No password
    WPA,    ///< WPA/WPA2
    WEP     ///< Legacy WEP
};

//==============================================================================
// Public API
//==============================================================================

/**
 * @brief Draw WiFi QR code on OLED display
 * 
 * Generates and displays a QR code that phones can scan
 * to automatically connect to the WiFi network.
 * 
 * @param ssid Network name (max 32 chars)
 * @param password Network password (max 64 chars)
 * @param authType Authentication type (default WPA)
 * @return true if QR was generated and displayed
 */
bool drawWifiQRCode(const char* ssid, const char* password, 
                    WifiAuthType authType = WifiAuthType::WPA);

/**
 * @brief Draw QR code screen with connection info
 * 
 * Shows QR code centered with SSID and password text below.
 * 
 * @param ssid Network name
 * @param password Network password
 * @param displayDurationMs How long to show (0 = indefinite)
 * @return true if displayed successfully
 */
bool showQRScreen(const char* ssid, const char* password, 
                  uint32_t displayDurationMs = 15000);

/**
 * @brief Generate WiFi URI string for QR encoding
 * 
 * Format: WIFI:T:WPA;S:<ssid>;P:<password>;;
 * 
 * @param buffer Output buffer
 * @param bufferSize Buffer size
 * @param ssid Network name
 * @param password Network password
 * @param authType Authentication type
 * @return true if string was generated
 */
bool generateWifiUri(char* buffer, size_t bufferSize,
                     const char* ssid, const char* password,
                     WifiAuthType authType = WifiAuthType::WPA);

/**
 * @brief Check if QR code display is currently active
 * @return true if QR screen is showing
 */
bool isQRScreenActive();

/**
 * @brief Close QR code screen
 */
void closeQRScreen();

}  // namespace pxlcam::wifi_qr
