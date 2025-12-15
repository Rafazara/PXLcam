/**
 * @file wifi_preview.cpp
 * @brief WiFi Preview implementation for PXLcam v1.3.0
 * 
 * Implements:
 * - ESP32 WiFi Access Point mode
 * - HTTP server with MJPEG streaming
 * - Web interface for camera preview
 * 
 * @version 1.3.0
 * @date 2024
 */

#include "wifi_preview.h"

#if PXLCAM_FEATURE_WIFI_PREVIEW

#include "logging.h"

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <esp_camera.h>

namespace pxlcam {

namespace {

constexpr const char* kLogTag = "wifi_preview";

// =============================================================================
// HTML Page
// =============================================================================

const char kIndexHtml[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>PXLcam Preview</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: #1a1a2e;
            color: #eee;
            min-height: 100vh;
            display: flex;
            flex-direction: column;
            align-items: center;
            padding: 20px;
        }
        h1 {
            color: #00d4ff;
            margin-bottom: 10px;
            font-size: 1.8em;
        }
        .version {
            color: #888;
            font-size: 0.9em;
            margin-bottom: 20px;
        }
        .stream-container {
            background: #0f0f23;
            border: 2px solid #00d4ff;
            border-radius: 8px;
            padding: 10px;
            max-width: 100%;
        }
        #stream {
            display: block;
            max-width: 100%;
            width: 320px;
            height: auto;
            border-radius: 4px;
        }
        .status {
            margin-top: 15px;
            padding: 10px 20px;
            background: #16213e;
            border-radius: 4px;
            font-size: 0.9em;
        }
        .status span {
            color: #00ff88;
        }
        .controls {
            margin-top: 20px;
            display: flex;
            gap: 10px;
        }
        button {
            padding: 10px 20px;
            font-size: 1em;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            background: #00d4ff;
            color: #1a1a2e;
            font-weight: bold;
            transition: all 0.2s;
        }
        button:hover {
            background: #00ff88;
        }
        button:active {
            transform: scale(0.98);
        }
        .footer {
            margin-top: auto;
            padding-top: 30px;
            color: #666;
            font-size: 0.8em;
        }
    </style>
</head>
<body>
    <h1>📷 PXLcam</h1>
    <p class="version">v1.3.0 WiFi Preview</p>
    
    <div class="stream-container">
        <img id="stream" src="/stream" alt="Camera Stream">
    </div>
    
    <div class="status">
        Status: <span id="status">Conectado</span>
    </div>
    
    <div class="controls">
        <button onclick="location.reload()">Atualizar</button>
    </div>
    
    <p class="footer">Desenvolvido por PXLcam Team</p>
    
    <script>
        const img = document.getElementById('stream');
        const status = document.getElementById('status');
        
        img.onerror = function() {
            status.textContent = 'Desconectado';
            status.style.color = '#ff4444';
            setTimeout(() => { img.src = '/stream?' + Date.now(); }, 2000);
        };
        img.onload = function() {
            status.textContent = 'Conectado';
            status.style.color = '#00ff88';
        };
    </script>
</body>
</html>
)rawliteral";

} // anonymous namespace

// =============================================================================
// WifiPreview Implementation
// =============================================================================

struct WifiPreview::Impl {
    WifiPreviewConfig config;
    WifiPreviewStatus status;
    WebServer* server;
    bool initialized;
    bool streaming;
    uint32_t lastFrameTime;
    uint32_t frameInterval;
    
    Impl() : server(nullptr), initialized(false), streaming(false),
             lastFrameTime(0), frameInterval(67) {} // ~15fps
};

WifiPreview& WifiPreview::instance() {
    static WifiPreview instance;
    return instance;
}

WifiPreview::WifiPreview() : m_impl(new Impl()) {
    memset(&m_impl->status, 0, sizeof(WifiPreviewStatus));
}

WifiPreview::~WifiPreview() {
    stop();
    delete m_impl->server;
    delete m_impl;
}

bool WifiPreview::init(const WifiPreviewConfig& config) {
    if (m_impl->initialized) {
        PXLCAM_LOGW_TAG(kLogTag, "Already initialized");
        return true;
    }
    
    m_impl->config = config;
    m_impl->frameInterval = 1000 / config.targetFps;
    
    PXLCAM_LOGI_TAG(kLogTag, "WiFi Preview initialized (mode=%d, port=%d)",
                    static_cast<int>(config.mode), config.httpPort);
    
    m_impl->initialized = true;
    return true;
}

bool WifiPreview::start() {
    if (!m_impl->initialized) {
        PXLCAM_LOGE_TAG(kLogTag, "Not initialized");
        return false;
    }
    
    if (m_impl->streaming) {
        PXLCAM_LOGW_TAG(kLogTag, "Already streaming");
        return true;
    }
    
    // Start WiFi AP
    PXLCAM_LOGI_TAG(kLogTag, "Starting WiFi AP: %s", m_impl->config.ssid);
    
    WiFi.mode(WIFI_AP);
    WiFi.softAP(m_impl->config.ssid, m_impl->config.password, m_impl->config.channel);
    
    IPAddress ip = WiFi.softAPIP();
    snprintf(m_impl->status.ipAddress, sizeof(m_impl->status.ipAddress),
             "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    
    PXLCAM_LOGI_TAG(kLogTag, "AP started, IP: %s", m_impl->status.ipAddress);
    
    // Create HTTP server
    m_impl->server = new WebServer(m_impl->config.httpPort);
    
    // Setup routes
    m_impl->server->on("/", HTTP_GET, [this]() {
        m_impl->server->send_P(200, "text/html", kIndexHtml);
    });
    
    m_impl->server->on("/stream", HTTP_GET, [this]() {
        handleStream();
    });
    
    m_impl->server->on("/capture", HTTP_GET, [this]() {
        handleCapture();
    });
    
    m_impl->server->on("/status", HTTP_GET, [this]() {
        handleStatus();
    });
    
    m_impl->server->begin();
    PXLCAM_LOGI_TAG(kLogTag, "HTTP server started on port %d", m_impl->config.httpPort);
    
    m_impl->streaming = true;
    m_impl->status.initialized = true;
    m_impl->status.connected = true;
    m_impl->status.streaming = true;
    
    return true;
}

void WifiPreview::stop() {
    if (m_impl->server) {
        m_impl->server->stop();
        delete m_impl->server;
        m_impl->server = nullptr;
    }
    
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    
    m_impl->streaming = false;
    m_impl->status.connected = false;
    m_impl->status.streaming = false;
    
    PXLCAM_LOGI_TAG(kLogTag, "WiFi Preview stopped");
}

bool WifiPreview::isActive() const {
    return m_impl->streaming;
}

void WifiPreview::tick() {
    if (!m_impl->streaming || !m_impl->server) return;
    
    m_impl->server->handleClient();
    m_impl->status.clientCount = WiFi.softAPgetStationNum();
}

uint8_t WifiPreview::sendFrame(const uint8_t* frameData, size_t frameSize) {
    if (!m_impl->streaming) return 0;
    
    m_impl->status.framesServed++;
    m_impl->status.bytesServed += frameSize;
    
    return m_impl->status.clientCount;
}

WifiPreviewStatus WifiPreview::getStatus() const {
    return m_impl->status;
}

String WifiPreview::getIPAddress() const {
    return String(m_impl->status.ipAddress);
}

uint8_t WifiPreview::getClientCount() const {
    return m_impl->status.clientCount;
}

void WifiPreview::setQuality(uint8_t quality) {
    m_impl->config.quality = quality;
}

void WifiPreview::setTargetFps(uint8_t fps) {
    m_impl->config.targetFps = fps;
    m_impl->frameInterval = 1000 / fps;
}

// =============================================================================
// HTTP Handlers
// =============================================================================

void WifiPreview::handleStream() {
    WiFiClient client = m_impl->server->client();
    
    // Send MJPEG headers
    String response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
    client.print(response);
    
    PXLCAM_LOGI_TAG(kLogTag, "MJPEG stream started for client");
    
    while (client.connected()) {
        camera_fb_t* fb = esp_camera_fb_get();
        if (!fb) {
            PXLCAM_LOGE_TAG(kLogTag, "Camera capture failed");
            delay(100);
            continue;
        }
        
        // Send frame
        client.printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", fb->len);
        client.write(fb->buf, fb->len);
        client.print("\r\n");
        
        m_impl->status.framesServed++;
        m_impl->status.bytesServed += fb->len;
        
        esp_camera_fb_return(fb);
        
        // Rate limiting
        delay(m_impl->frameInterval);
        
        // Check for client disconnect
        if (!client.connected()) break;
    }
    
    PXLCAM_LOGI_TAG(kLogTag, "MJPEG stream ended");
}

void WifiPreview::handleCapture() {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        m_impl->server->send(500, "text/plain", "Camera capture failed");
        return;
    }
    
    m_impl->server->sendHeader("Content-Disposition", "inline; filename=capture.jpg");
    m_impl->server->send_P(200, "image/jpeg", (const char*)fb->buf, fb->len);
    
    esp_camera_fb_return(fb);
}

void WifiPreview::handleStatus() {
    String json = "{";
    json += "\"streaming\":" + String(m_impl->streaming ? "true" : "false") + ",";
    json += "\"clients\":" + String(m_impl->status.clientCount) + ",";
    json += "\"frames\":" + String(m_impl->status.framesServed) + ",";
    json += "\"bytes\":" + String(m_impl->status.bytesServed) + ",";
    json += "\"ip\":\"" + String(m_impl->status.ipAddress) + "\"";
    json += "}";
    
    m_impl->server->send(200, "application/json", json);
}

// =============================================================================
// Convenience Functions
// =============================================================================

bool wifi_preview_start_ap(const char* ssid, const char* password) {
    WifiPreviewConfig config;
    config.mode = WifiMode::AP;
    strncpy(config.ssid, ssid, sizeof(config.ssid) - 1);
    strncpy(config.password, password, sizeof(config.password) - 1);
    
    if (!WifiPreview::instance().init(config)) {
        return false;
    }
    
    return WifiPreview::instance().start();
}

bool wifi_preview_start_sta(const char* ssid, const char* password) {
    // TODO: Implement STA mode
    PXLCAM_LOGW_TAG(kLogTag, "STA mode not yet implemented");
    return false;
}

void wifi_preview_stop() {
    WifiPreview::instance().stop();
}

bool wifi_preview_is_active() {
    return WifiPreview::instance().isActive();
}

bool wifi_enable_mdns(const char* hostname) {
    if (!WifiPreview::instance().isActive()) {
        PXLCAM_LOGW_TAG(kLogTag, "mDNS requires WiFi to be active");
        return false;
    }
    
    if (!MDNS.begin(hostname)) {
        PXLCAM_LOGE_TAG(kLogTag, "mDNS failed to start");
        return false;
    }
    
    // Add HTTP service
    MDNS.addService("http", "tcp", 80);
    
    PXLCAM_LOGI_TAG(kLogTag, "mDNS started: %s.local", hostname);
    return true;
}

} // namespace pxlcam

#endif // PXLCAM_FEATURE_WIFI_PREVIEW
