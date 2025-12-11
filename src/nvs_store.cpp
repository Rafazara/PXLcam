/**
 * @file nvs_store.cpp
 * @brief Non-Volatile Storage implementation for PXLcam v1.2.0
 */

#include "nvs_store.h"
#include "logging.h"

#include <nvs_flash.h>
#include <nvs.h>

namespace pxlcam::nvs {

namespace {

constexpr const char* kLogTag = "pxlcam-nvs";
constexpr const char* kNamespace = "pxlcam";

nvs_handle_t g_nvsHandle = 0;
bool g_initialized = false;

}  // anonymous namespace

bool init() {
    if (g_initialized) {
        return true;
    }
    
    // Initialize NVS flash
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        PXLCAM_LOGW_TAG(kLogTag, "NVS partition needs erase, reformatting...");
        err = nvs_flash_erase();
        if (err != ESP_OK) {
            PXLCAM_LOGE_TAG(kLogTag, "NVS flash erase failed: %s", esp_err_to_name(err));
            return false;
        }
        err = nvs_flash_init();
    }
    
    if (err != ESP_OK) {
        PXLCAM_LOGE_TAG(kLogTag, "NVS flash init failed: %s", esp_err_to_name(err));
        return false;
    }
    
    // Open NVS handle
    err = nvs_open(kNamespace, NVS_READWRITE, &g_nvsHandle);
    if (err != ESP_OK) {
        PXLCAM_LOGE_TAG(kLogTag, "NVS open failed: %s", esp_err_to_name(err));
        return false;
    }
    
    g_initialized = true;
    PXLCAM_LOGI_TAG(kLogTag, "NVS initialized (namespace: %s)", kNamespace);
    return true;
}

bool isInitialized() {
    return g_initialized;
}

bool writeU8(const char* key, uint8_t value) {
    if (!g_initialized || !key) return false;
    
    esp_err_t err = nvs_set_u8(g_nvsHandle, key, value);
    if (err != ESP_OK) {
        PXLCAM_LOGE_TAG(kLogTag, "Write U8 failed [%s]: %s", key, esp_err_to_name(err));
        return false;
    }
    
    return commit();
}

uint8_t readU8(const char* key, uint8_t defaultValue) {
    if (!g_initialized || !key) return defaultValue;
    
    uint8_t value = defaultValue;
    esp_err_t err = nvs_get_u8(g_nvsHandle, key, &value);
    
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return defaultValue;
    }
    
    if (err != ESP_OK) {
        PXLCAM_LOGW_TAG(kLogTag, "Read U8 failed [%s]: %s", key, esp_err_to_name(err));
        return defaultValue;
    }
    
    return value;
}

bool writeU32(const char* key, uint32_t value) {
    if (!g_initialized || !key) return false;
    
    esp_err_t err = nvs_set_u32(g_nvsHandle, key, value);
    if (err != ESP_OK) {
        PXLCAM_LOGE_TAG(kLogTag, "Write U32 failed [%s]: %s", key, esp_err_to_name(err));
        return false;
    }
    
    return commit();
}

uint32_t readU32(const char* key, uint32_t defaultValue) {
    if (!g_initialized || !key) return defaultValue;
    
    uint32_t value = defaultValue;
    esp_err_t err = nvs_get_u32(g_nvsHandle, key, &value);
    
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return defaultValue;
    }
    
    if (err != ESP_OK) {
        PXLCAM_LOGW_TAG(kLogTag, "Read U32 failed [%s]: %s", key, esp_err_to_name(err));
        return defaultValue;
    }
    
    return value;
}

bool writeI8(const char* key, int8_t value) {
    if (!g_initialized || !key) return false;
    
    esp_err_t err = nvs_set_i8(g_nvsHandle, key, value);
    if (err != ESP_OK) {
        PXLCAM_LOGE_TAG(kLogTag, "Write I8 failed [%s]: %s", key, esp_err_to_name(err));
        return false;
    }
    
    return commit();
}

int8_t readI8(const char* key, int8_t defaultValue) {
    if (!g_initialized || !key) return defaultValue;
    
    int8_t value = defaultValue;
    esp_err_t err = nvs_get_i8(g_nvsHandle, key, &value);
    
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return defaultValue;
    }
    
    if (err != ESP_OK) {
        PXLCAM_LOGW_TAG(kLogTag, "Read I8 failed [%s]: %s", key, esp_err_to_name(err));
        return defaultValue;
    }
    
    return value;
}

bool exists(const char* key) {
    if (!g_initialized || !key) return false;
    
    uint8_t dummy;
    esp_err_t err = nvs_get_u8(g_nvsHandle, key, &dummy);
    return err == ESP_OK;
}

bool erase(const char* key) {
    if (!g_initialized || !key) return false;
    
    esp_err_t err = nvs_erase_key(g_nvsHandle, key);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        PXLCAM_LOGE_TAG(kLogTag, "Erase key failed [%s]: %s", key, esp_err_to_name(err));
        return false;
    }
    
    return commit();
}

bool eraseAll() {
    if (!g_initialized) return false;
    
    esp_err_t err = nvs_erase_all(g_nvsHandle);
    if (err != ESP_OK) {
        PXLCAM_LOGE_TAG(kLogTag, "Erase all failed: %s", esp_err_to_name(err));
        return false;
    }
    
    PXLCAM_LOGI_TAG(kLogTag, "All NVS data erased");
    return commit();
}

bool commit() {
    if (!g_initialized) return false;
    
    esp_err_t err = nvs_commit(g_nvsHandle);
    if (err != ESP_OK) {
        PXLCAM_LOGE_TAG(kLogTag, "NVS commit failed: %s", esp_err_to_name(err));
        return false;
    }
    
    return true;
}

}  // namespace pxlcam::nvs
