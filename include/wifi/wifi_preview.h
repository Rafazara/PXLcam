/**
 * @file wifi_preview.h
 * @brief WiFi Preview Pipeline for PXLcam v1.3.0
 * 
 * Provides real-time preview streaming over WiFi using ESP32's
 * built-in WiFi capabilities. Supports MJPEG streaming and
 * WebSocket-based frame delivery.
 * 
 * @version 1.3.0
 * @date 2024
 */

#ifndef WIFI_PREVIEW_H
#define WIFI_PREVIEW_H

#include <Arduino.h>

namespace pxlcam {
namespace wifi {

/**
 * @brief WiFi Preview configuration
 */
struct WifiPreviewConfig {
    const char* ssid;           ///< WiFi SSID for AP mode
    const char* password;       ///< WiFi password
    uint16_t port;              ///< HTTP server port (default: 80)
    uint8_t quality;            ///< JPEG quality (0-100)
    uint8_t frameRate;          ///< Target frame rate
    bool apMode;                ///< true = AP mode, false = STA mode
};

/**
 * @brief Initialize WiFi preview system
 * @param config Configuration parameters
 * @return true on success
 */
bool init(const WifiPreviewConfig& config);

/**
 * @brief Start WiFi preview streaming
 * @return true on success
 */
bool start();

/**
 * @brief Stop WiFi preview streaming
 */
void stop();

/**
 * @brief Check if WiFi preview is active
 * @return true if streaming
 */
bool isActive();

/**
 * @brief Get current client count
 * @return Number of connected clients
 */
uint8_t getClientCount();

/**
 * @brief Get IP address string
 * @return IP address as string
 */
String getIPAddress();

} // namespace wifi
} // namespace pxlcam

#endif // WIFI_PREVIEW_H
