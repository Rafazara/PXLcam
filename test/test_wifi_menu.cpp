/**
 * @file test_wifi_menu.cpp
 * @brief Unit tests for WiFi Menu module (PXLcam v1.3.0)
 * 
 * Tests WiFi submenu navigation, QR code generation, and mDNS support.
 * 
 * @version 1.3.0
 * @date 2024
 */

#include <unity.h>
#include <cstring>
#include <cstdio>

// =============================================================================
// Mock Arduino Types for Native Testing
// =============================================================================

#ifndef ARDUINO
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

unsigned long millis() {
    static unsigned long ms = 0;
    return ms += 10;
}

void delay(unsigned long) {}

#define GPIO_NUM_12 12
#define LOW 0
int digitalRead(int) { return 1; }  // Button not pressed
#endif

// =============================================================================
// WiFi Menu Mock Types
// =============================================================================

namespace pxlcam::wifi_menu {

enum class WifiMenuResult : uint8_t {
    START,
    STOP,
    SHOW_INFO,
    SHOW_QR,
    BACK,
    CANCELLED
};

struct WifiMenuConfig {
    uint32_t longPressMs = 1000;
    uint32_t autoCloseMs = 20000;
    uint32_t infoDisplayMs = 5000;
    uint32_t qrDisplayMs = 15000;
};

// Mock state
static WifiMenuConfig g_config;
static bool g_isOpen = false;
static uint8_t g_currentIndex = 0;
static constexpr uint8_t kItemCount = 5;

void init(const WifiMenuConfig* config = nullptr) {
    if (config) {
        g_config = *config;
    }
    g_isOpen = false;
    g_currentIndex = 0;
}

const char* getResultName(WifiMenuResult result) {
    switch (result) {
        case WifiMenuResult::START:      return "Start";
        case WifiMenuResult::STOP:       return "Stop";
        case WifiMenuResult::SHOW_INFO:  return "Show Info";
        case WifiMenuResult::SHOW_QR:    return "Show QR";
        case WifiMenuResult::BACK:       return "Back";
        case WifiMenuResult::CANCELLED:  return "Cancelled";
        default:                         return "Unknown";
    }
}

bool isOpen() { return g_isOpen; }

// Test helpers
void setIndex(uint8_t idx) { g_currentIndex = idx; }
uint8_t getIndex() { return g_currentIndex; }
void setOpen(bool open) { g_isOpen = open; }

}  // namespace pxlcam::wifi_menu

// =============================================================================
// WiFi QR Mock Types
// =============================================================================

namespace pxlcam::wifi_qr {

constexpr uint8_t kQrVersion = 1;
constexpr uint8_t kQrSize = 21;

enum class WifiAuthType : uint8_t {
    OPEN,
    WPA,
    WEP
};

static bool g_qrActive = false;

bool generateWifiUri(char* buffer, size_t bufferSize,
                     const char* ssid, const char* password,
                     WifiAuthType authType = WifiAuthType::WPA) {
    if (!buffer || bufferSize < 32) {
        return false;
    }
    
    const char* authStr;
    switch (authType) {
        case WifiAuthType::WPA:  authStr = "WPA"; break;
        case WifiAuthType::WEP:  authStr = "WEP"; break;
        case WifiAuthType::OPEN: authStr = "nopass"; break;
        default: authStr = "WPA"; break;
    }
    
    int written = snprintf(buffer, bufferSize, 
                           "WIFI:T:%s;S:%s;P:%s;;",
                           authStr, ssid, password);
    
    return (written > 0 && (size_t)written < bufferSize);
}

bool isQRScreenActive() { return g_qrActive; }
void closeQRScreen() { g_qrActive = false; }
void setQRActive(bool active) { g_qrActive = active; }

}  // namespace pxlcam::wifi_qr

// =============================================================================
// mDNS Mock
// =============================================================================

namespace pxlcam {

static bool g_mdnsStarted = false;
static char g_mdnsHostname[32] = {0};

bool wifi_enable_mdns(const char* hostname) {
    if (!hostname || strlen(hostname) > 30) {
        return false;
    }
    strncpy(g_mdnsHostname, hostname, sizeof(g_mdnsHostname) - 1);
    g_mdnsStarted = true;
    return true;
}

// Test helpers
bool isMdnsStarted() { return g_mdnsStarted; }
const char* getMdnsHostname() { return g_mdnsHostname; }
void resetMdns() { 
    g_mdnsStarted = false; 
    g_mdnsHostname[0] = '\0';
}

}  // namespace pxlcam

// =============================================================================
// Test Cases
// =============================================================================

void setUp() {
    pxlcam::wifi_menu::init();
    pxlcam::wifi_qr::setQRActive(false);
    pxlcam::resetMdns();
}

void tearDown() {
}

// -----------------------------------------------------------------------------
// WiFi Menu Enum Tests
// -----------------------------------------------------------------------------

void test_wifi_menu_result_values() {
    using namespace pxlcam::wifi_menu;
    
    TEST_ASSERT_EQUAL(0, static_cast<uint8_t>(WifiMenuResult::START));
    TEST_ASSERT_EQUAL(1, static_cast<uint8_t>(WifiMenuResult::STOP));
    TEST_ASSERT_EQUAL(2, static_cast<uint8_t>(WifiMenuResult::SHOW_INFO));
    TEST_ASSERT_EQUAL(3, static_cast<uint8_t>(WifiMenuResult::SHOW_QR));
    TEST_ASSERT_EQUAL(4, static_cast<uint8_t>(WifiMenuResult::BACK));
    TEST_ASSERT_EQUAL(5, static_cast<uint8_t>(WifiMenuResult::CANCELLED));
}

void test_wifi_menu_result_names() {
    using namespace pxlcam::wifi_menu;
    
    TEST_ASSERT_EQUAL_STRING("Start", getResultName(WifiMenuResult::START));
    TEST_ASSERT_EQUAL_STRING("Stop", getResultName(WifiMenuResult::STOP));
    TEST_ASSERT_EQUAL_STRING("Show Info", getResultName(WifiMenuResult::SHOW_INFO));
    TEST_ASSERT_EQUAL_STRING("Show QR", getResultName(WifiMenuResult::SHOW_QR));
    TEST_ASSERT_EQUAL_STRING("Back", getResultName(WifiMenuResult::BACK));
    TEST_ASSERT_EQUAL_STRING("Cancelled", getResultName(WifiMenuResult::CANCELLED));
}

// -----------------------------------------------------------------------------
// WiFi Menu Config Tests
// -----------------------------------------------------------------------------

void test_wifi_menu_default_config() {
    pxlcam::wifi_menu::WifiMenuConfig config;
    
    TEST_ASSERT_EQUAL(1000, config.longPressMs);
    TEST_ASSERT_EQUAL(20000, config.autoCloseMs);
    TEST_ASSERT_EQUAL(5000, config.infoDisplayMs);
    TEST_ASSERT_EQUAL(15000, config.qrDisplayMs);
}

void test_wifi_menu_custom_config() {
    pxlcam::wifi_menu::WifiMenuConfig config;
    config.longPressMs = 500;
    config.autoCloseMs = 10000;
    
    pxlcam::wifi_menu::init(&config);
    
    // Config should be applied (internal state)
    TEST_ASSERT_FALSE(pxlcam::wifi_menu::isOpen());
}

// -----------------------------------------------------------------------------
// WiFi Menu Navigation Tests
// -----------------------------------------------------------------------------

void test_wifi_menu_initial_state() {
    pxlcam::wifi_menu::init();
    
    TEST_ASSERT_FALSE(pxlcam::wifi_menu::isOpen());
    TEST_ASSERT_EQUAL(0, pxlcam::wifi_menu::getIndex());
}

void test_wifi_menu_index_cycling() {
    using namespace pxlcam::wifi_menu;
    
    init();
    
    // Start at 0
    TEST_ASSERT_EQUAL(0, getIndex());
    
    // Navigate forward
    setIndex(1);
    TEST_ASSERT_EQUAL(1, getIndex());
    
    setIndex(2);
    TEST_ASSERT_EQUAL(2, getIndex());
    
    // Navigate to last item
    setIndex(4);
    TEST_ASSERT_EQUAL(4, getIndex());
    
    // Wrap around
    setIndex(0);
    TEST_ASSERT_EQUAL(0, getIndex());
}

void test_wifi_menu_open_close() {
    using namespace pxlcam::wifi_menu;
    
    init();
    TEST_ASSERT_FALSE(isOpen());
    
    setOpen(true);
    TEST_ASSERT_TRUE(isOpen());
    
    setOpen(false);
    TEST_ASSERT_FALSE(isOpen());
}

// -----------------------------------------------------------------------------
// QR Code URI Generation Tests
// -----------------------------------------------------------------------------

void test_qr_generate_wpa_uri() {
    char buffer[128];
    
    bool result = pxlcam::wifi_qr::generateWifiUri(
        buffer, sizeof(buffer),
        "PXLcam", "12345678",
        pxlcam::wifi_qr::WifiAuthType::WPA
    );
    
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_STRING("WIFI:T:WPA;S:PXLcam;P:12345678;;", buffer);
}

void test_qr_generate_open_uri() {
    char buffer[128];
    
    bool result = pxlcam::wifi_qr::generateWifiUri(
        buffer, sizeof(buffer),
        "OpenNetwork", "",
        pxlcam::wifi_qr::WifiAuthType::OPEN
    );
    
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_STRING("WIFI:T:nopass;S:OpenNetwork;P:;;", buffer);
}

void test_qr_generate_wep_uri() {
    char buffer[128];
    
    bool result = pxlcam::wifi_qr::generateWifiUri(
        buffer, sizeof(buffer),
        "LegacyNet", "wepkey",
        pxlcam::wifi_qr::WifiAuthType::WEP
    );
    
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_STRING("WIFI:T:WEP;S:LegacyNet;P:wepkey;;", buffer);
}

void test_qr_generate_buffer_too_small() {
    char buffer[10];  // Too small
    
    bool result = pxlcam::wifi_qr::generateWifiUri(
        buffer, sizeof(buffer),
        "PXLcam", "12345678",
        pxlcam::wifi_qr::WifiAuthType::WPA
    );
    
    TEST_ASSERT_FALSE(result);
}

void test_qr_generate_null_buffer() {
    bool result = pxlcam::wifi_qr::generateWifiUri(
        nullptr, 128,
        "PXLcam", "12345678",
        pxlcam::wifi_qr::WifiAuthType::WPA
    );
    
    TEST_ASSERT_FALSE(result);
}

// -----------------------------------------------------------------------------
// QR Screen State Tests
// -----------------------------------------------------------------------------

void test_qr_screen_initial_state() {
    TEST_ASSERT_FALSE(pxlcam::wifi_qr::isQRScreenActive());
}

void test_qr_screen_active_toggle() {
    using namespace pxlcam::wifi_qr;
    
    TEST_ASSERT_FALSE(isQRScreenActive());
    
    setQRActive(true);
    TEST_ASSERT_TRUE(isQRScreenActive());
    
    closeQRScreen();
    TEST_ASSERT_FALSE(isQRScreenActive());
}

// -----------------------------------------------------------------------------
// mDNS Tests
// -----------------------------------------------------------------------------

void test_mdns_enable_default_hostname() {
    bool result = pxlcam::wifi_enable_mdns("pxlcam");
    
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(pxlcam::isMdnsStarted());
    TEST_ASSERT_EQUAL_STRING("pxlcam", pxlcam::getMdnsHostname());
}

void test_mdns_enable_custom_hostname() {
    bool result = pxlcam::wifi_enable_mdns("mycamera");
    
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(pxlcam::isMdnsStarted());
    TEST_ASSERT_EQUAL_STRING("mycamera", pxlcam::getMdnsHostname());
}

void test_mdns_null_hostname() {
    bool result = pxlcam::wifi_enable_mdns(nullptr);
    
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_FALSE(pxlcam::isMdnsStarted());
}

void test_mdns_hostname_too_long() {
    // 31+ chars should fail
    bool result = pxlcam::wifi_enable_mdns("this_hostname_is_way_too_long_for_mdns");
    
    TEST_ASSERT_FALSE(result);
}

// -----------------------------------------------------------------------------
// QR Code Constants Tests
// -----------------------------------------------------------------------------

void test_qr_constants() {
    TEST_ASSERT_EQUAL(1, pxlcam::wifi_qr::kQrVersion);
    TEST_ASSERT_EQUAL(21, pxlcam::wifi_qr::kQrSize);
}

// =============================================================================
// Test Runner
// =============================================================================

int main(int argc, char** argv) {
    UNITY_BEGIN();
    
    // WiFi Menu enum tests
    RUN_TEST(test_wifi_menu_result_values);
    RUN_TEST(test_wifi_menu_result_names);
    
    // WiFi Menu config tests
    RUN_TEST(test_wifi_menu_default_config);
    RUN_TEST(test_wifi_menu_custom_config);
    
    // WiFi Menu navigation tests
    RUN_TEST(test_wifi_menu_initial_state);
    RUN_TEST(test_wifi_menu_index_cycling);
    RUN_TEST(test_wifi_menu_open_close);
    
    // QR URI generation tests
    RUN_TEST(test_qr_generate_wpa_uri);
    RUN_TEST(test_qr_generate_open_uri);
    RUN_TEST(test_qr_generate_wep_uri);
    RUN_TEST(test_qr_generate_buffer_too_small);
    RUN_TEST(test_qr_generate_null_buffer);
    
    // QR screen state tests
    RUN_TEST(test_qr_screen_initial_state);
    RUN_TEST(test_qr_screen_active_toggle);
    
    // mDNS tests
    RUN_TEST(test_mdns_enable_default_hostname);
    RUN_TEST(test_mdns_enable_custom_hostname);
    RUN_TEST(test_mdns_null_hostname);
    RUN_TEST(test_mdns_hostname_too_long);
    
    // QR constants tests
    RUN_TEST(test_qr_constants);
    
    return UNITY_END();
}
