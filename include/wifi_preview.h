/**
 * @file wifi_preview.h
 * @brief WiFi Preview System for PXLcam v1.3.0
 * 
 * Real-time camera preview streaming over WiFi using WebSocket.
 * Allows viewing the camera output on any device with a web browser.
 * 
 * Features:
 * - Access Point (AP) mode for direct connection
 * - Station (STA) mode for existing network
 * - MJPEG streaming for compatibility
 * - WebSocket binary frames for low latency
 * - Multiple simultaneous clients
 * 
 * @version 1.3.0
 * @date 2024
 * 
 * Feature Flag: PXLCAM_FEATURE_WIFI_PREVIEW
 */

#ifndef PXLCAM_WIFI_PREVIEW_H
#define PXLCAM_WIFI_PREVIEW_H

#include <Arduino.h>
#include <stdint.h>

// Feature gate - disabled by default for memory savings
#ifndef PXLCAM_FEATURE_WIFI_PREVIEW
#define PXLCAM_FEATURE_WIFI_PREVIEW 0
#endif

#if PXLCAM_FEATURE_WIFI_PREVIEW

namespace pxlcam {

// =============================================================================
// WiFi Preview Configuration
// =============================================================================

/**
 * @brief WiFi operation mode
 */
enum class WifiMode : uint8_t {
    OFF = 0,        ///< WiFi disabled
    AP,             ///< Access Point mode (camera creates network)
    STA,            ///< Station mode (camera joins existing network)
    AP_STA          ///< Both modes active
};

/**
 * @brief Stream format for preview
 */
enum class StreamFormat : uint8_t {
    MJPEG,          ///< Motion JPEG (HTTP multipart)
    WEBSOCKET_BIN,  ///< WebSocket binary frames (low latency)
    WEBSOCKET_B64   ///< WebSocket base64 (more compatible)
};

/**
 * @brief WiFi Preview configuration
 */
struct WifiPreviewConfig {
    // Network settings
    WifiMode mode;                  ///< WiFi operation mode
    char ssid[32];                  ///< SSID (AP name or network to join)
    char password[64];              ///< Password
    uint8_t channel;                ///< WiFi channel (AP mode)
    
    // Server settings
    uint16_t httpPort;              ///< HTTP server port (default: 80)
    uint16_t wsPort;                ///< WebSocket port (default: 81)
    
    // Stream settings
    StreamFormat format;            ///< Stream format
    uint8_t quality;                ///< JPEG quality (0-100)
    uint8_t targetFps;              ///< Target frame rate
    uint8_t maxClients;             ///< Maximum simultaneous clients
    
    // Defaults
    WifiPreviewConfig()
        : mode(WifiMode::AP)
        , channel(1)
        , httpPort(80)
        , wsPort(81)
        , format(StreamFormat::MJPEG)
        , quality(50)
        , targetFps(15)
        , maxClients(4)
    {
        strncpy(ssid, "PXLcam", sizeof(ssid));
        strncpy(password, "pxlcam1234", sizeof(password));
    }
};

/**
 * @brief WiFi Preview status
 */
struct WifiPreviewStatus {
    bool initialized;       ///< System initialized
    bool connected;         ///< WiFi connected (STA) or active (AP)
    bool streaming;         ///< Currently streaming
    uint8_t clientCount;    ///< Connected clients
    uint32_t framesServed;  ///< Total frames sent
    uint32_t bytesServed;   ///< Total bytes sent
    float currentFps;       ///< Current streaming FPS
    char ipAddress[16];     ///< IP address string
};

// =============================================================================
// WiFi Preview Class
// =============================================================================

/**
 * @brief WiFi Preview Controller
 * 
 * Manages WiFi connectivity and camera preview streaming.
 * Uses ESP32's built-in WiFi and async web server.
 */
class WifiPreview {
public:
    /**
     * @brief Get singleton instance
     */
    static WifiPreview& instance();
    
    /**
     * @brief Initialize WiFi preview system
     * 
     * Sets up WiFi hardware and web server but does not start streaming.
     * 
     * @param config Configuration parameters
     * @return true on success
     */
    bool init(const WifiPreviewConfig& config = WifiPreviewConfig());
    
    /**
     * @brief Start WiFi and streaming
     * 
     * @return true on success
     */
    bool start();
    
    /**
     * @brief Stop streaming and disconnect WiFi
     */
    void stop();
    
    /**
     * @brief Check if streaming is active
     * 
     * @return true if streaming
     */
    bool isActive() const;
    
    /**
     * @brief Send a frame to all connected clients
     * 
     * @param frameData JPEG frame data
     * @param frameSize Frame size in bytes
     * @return Number of clients that received the frame
     */
    uint8_t sendFrame(const uint8_t* frameData, size_t frameSize);
    
    /**
     * @brief Process WiFi events (call in loop)
     * 
     * Handles client connections, disconnections, and requests.
     */
    void tick();
    
    /**
     * @brief Get current status
     * 
     * @return Status structure
     */
    WifiPreviewStatus getStatus() const;
    
    /**
     * @brief Get IP address as string
     * 
     * @return IP address or "0.0.0.0" if not connected
     */
    String getIPAddress() const;
    
    /**
     * @brief Get connected client count
     * 
     * @return Number of clients
     */
    uint8_t getClientCount() const;
    
    /**
     * @brief Set stream quality
     * 
     * @param quality JPEG quality (0-100)
     */
    void setQuality(uint8_t quality);
    
    /**
     * @brief Set target FPS
     * 
     * @param fps Target frames per second
     */
    void setTargetFps(uint8_t fps);
    
private:
    WifiPreview();
    ~WifiPreview();
    
    // Non-copyable
    WifiPreview(const WifiPreview&) = delete;
    WifiPreview& operator=(const WifiPreview&) = delete;
    
    // Private implementation
    struct Impl;
    Impl* m_impl;
};

// =============================================================================
// Convenience Functions
// =============================================================================

/**
 * @brief Quick start WiFi preview in AP mode
 * 
 * @param ssid Network name (default: "PXLcam")
 * @param password Password (default: "pxlcam1234")
 * @return true on success
 */
bool wifi_preview_start_ap(const char* ssid = "PXLcam", const char* password = "pxlcam1234");

/**
 * @brief Quick start WiFi preview in STA mode
 * 
 * @param ssid Network to join
 * @param password Network password
 * @return true on success
 */
bool wifi_preview_start_sta(const char* ssid, const char* password);

/**
 * @brief Stop WiFi preview
 */
void wifi_preview_stop();

/**
 * @brief Check if WiFi preview is running
 * 
 * @return true if active
 */
bool wifi_preview_is_active();

// =============================================================================
// TODOs for v1.3.0 Implementation
// =============================================================================

// TODO: Implement WiFi AP mode with captive portal
// TODO: Implement MJPEG streaming endpoint
// TODO: Implement WebSocket binary streaming
// TODO: Add web UI for camera control
// TODO: Implement OTA firmware update via WiFi
// TODO: Add QR code display for easy connection
// TODO: Implement WiFi credentials NVS storage
// TODO: Add mDNS support (pxlcam.local)

} // namespace pxlcam

#endif // PXLCAM_FEATURE_WIFI_PREVIEW

#endif // PXLCAM_WIFI_PREVIEW_H
