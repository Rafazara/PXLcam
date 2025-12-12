/**
 * @file nvs_store.cpp
 * @brief Non-Volatile Storage implementation using Preferences for PXLcam v1.2.0
 * 
 * Uses Arduino Preferences library (ESP32 NVS wrapper) for simpler API.
 */

#include "nvs_store.h"
#include "logging.h"

#include <Preferences.h>

namespace pxlcam::nvs {

namespace {

constexpr const char* kLogTag = "nvs";
constexpr const char* kNamespace = "pxlcam";

Preferences g_prefs;
bool g_initialized = false;

}  // anonymous namespace

//==============================================================================
// Primary API Implementation
//==============================================================================

void nvsStoreInit() {
    if (g_initialized) {
        PXLCAM_LOGI_TAG(kLogTag, "NVS already initialized");
        return;
    }
    
    // Open namespace in read/write mode
    bool success = g_prefs.begin(kNamespace, false);
    
    if (!success) {
        PXLCAM_LOGE_TAG(kLogTag, "ERRO: Falha ao abrir NVS namespace '%s'", kNamespace);
        return;
    }
    
    g_initialized = true;
    
    // Log initialization success with stats
    size_t freeEntries = g_prefs.freeEntries();
    PXLCAM_LOGI_TAG(kLogTag, "NVS inicializado (namespace: %s, free: %d entries)", 
                    kNamespace, freeEntries);
}

void saveMode(uint8_t mode) {
    if (!g_initialized) {
        PXLCAM_LOGE_TAG(kLogTag, "ERRO: NVS nao inicializado, modo nao salvo");
        return;
    }
    
    // Validate mode value
    if (mode > 2) {
        PXLCAM_LOGW_TAG(kLogTag, "Modo invalido %d, ignorando", mode);
        return;
    }
    
    size_t written = g_prefs.putUChar(keys::kCaptureMode, mode);
    
    if (written == 0) {
        PXLCAM_LOGE_TAG(kLogTag, "ERRO: Falha ao salvar modo %d", mode);
    } else {
        PXLCAM_LOGI_TAG(kLogTag, "Modo salvo: %d", mode);
    }
}

uint8_t loadModeOrDefault(uint8_t fallback) {
    if (!g_initialized) {
        PXLCAM_LOGW_TAG(kLogTag, "NVS nao inicializado, retornando fallback: %d", fallback);
        return fallback;
    }
    
    // Check if key exists first
    if (!g_prefs.isKey(keys::kCaptureMode)) {
        PXLCAM_LOGI_TAG(kLogTag, "Chave 'mode' nao existe, usando fallback: %d", fallback);
        return fallback;
    }
    
    uint8_t mode = g_prefs.getUChar(keys::kCaptureMode, fallback);
    
    // Validate loaded value
    if (mode > 2) {
        PXLCAM_LOGW_TAG(kLogTag, "Modo carregado invalido (%d), usando fallback: %d", mode, fallback);
        return fallback;
    }
    
    PXLCAM_LOGI_TAG(kLogTag, "Modo carregado: %d", mode);
    return mode;
}

//==============================================================================
// Extended API Implementation
//==============================================================================

bool isInitialized() {
    return g_initialized;
}

bool init() {
    nvsStoreInit();
    return g_initialized;
}

bool writeU8(const char* key, uint8_t value) {
    if (!g_initialized || !key) {
        PXLCAM_LOGE_TAG(kLogTag, "writeU8 falhou: NVS=%d, key=%s", g_initialized, key ? key : "null");
        return false;
    }
    
    size_t written = g_prefs.putUChar(key, value);
    
    if (written == 0) {
        PXLCAM_LOGE_TAG(kLogTag, "Falha ao escrever U8 [%s]=%d", key, value);
        return false;
    }
    
    PXLCAM_LOGD_TAG(kLogTag, "U8 escrito [%s]=%d", key, value);
    return true;
}

uint8_t readU8(const char* key, uint8_t defaultValue) {
    if (!g_initialized || !key) {
        return defaultValue;
    }
    
    return g_prefs.getUChar(key, defaultValue);
}

bool writeU32(const char* key, uint32_t value) {
    if (!g_initialized || !key) {
        PXLCAM_LOGE_TAG(kLogTag, "writeU32 falhou: NVS=%d, key=%s", g_initialized, key ? key : "null");
        return false;
    }
    
    size_t written = g_prefs.putULong(key, value);
    
    if (written == 0) {
        PXLCAM_LOGE_TAG(kLogTag, "Falha ao escrever U32 [%s]=%lu", key, value);
        return false;
    }
    
    PXLCAM_LOGD_TAG(kLogTag, "U32 escrito [%s]=%lu", key, value);
    return true;
}

uint32_t readU32(const char* key, uint32_t defaultValue) {
    if (!g_initialized || !key) {
        return defaultValue;
    }
    
    return g_prefs.getULong(key, defaultValue);
}

bool writeI8(const char* key, int8_t value) {
    if (!g_initialized || !key) {
        PXLCAM_LOGE_TAG(kLogTag, "writeI8 falhou: NVS=%d, key=%s", g_initialized, key ? key : "null");
        return false;
    }
    
    size_t written = g_prefs.putChar(key, value);
    
    if (written == 0) {
        PXLCAM_LOGE_TAG(kLogTag, "Falha ao escrever I8 [%s]=%d", key, value);
        return false;
    }
    
    PXLCAM_LOGD_TAG(kLogTag, "I8 escrito [%s]=%d", key, value);
    return true;
}

int8_t readI8(const char* key, int8_t defaultValue) {
    if (!g_initialized || !key) {
        return defaultValue;
    }
    
    return g_prefs.getChar(key, defaultValue);
}

bool exists(const char* key) {
    if (!g_initialized || !key) {
        return false;
    }
    
    return g_prefs.isKey(key);
}

bool erase(const char* key) {
    if (!g_initialized || !key) {
        PXLCAM_LOGE_TAG(kLogTag, "erase falhou: NVS=%d, key=%s", g_initialized, key ? key : "null");
        return false;
    }
    
    bool success = g_prefs.remove(key);
    
    if (success) {
        PXLCAM_LOGI_TAG(kLogTag, "Chave removida: %s", key);
    } else {
        PXLCAM_LOGW_TAG(kLogTag, "Falha ao remover chave: %s", key);
    }
    
    return success;
}

bool eraseAll() {
    if (!g_initialized) {
        PXLCAM_LOGE_TAG(kLogTag, "eraseAll falhou: NVS nao inicializado");
        return false;
    }
    
    bool success = g_prefs.clear();
    
    if (success) {
        PXLCAM_LOGI_TAG(kLogTag, "Todos os dados NVS apagados");
    } else {
        PXLCAM_LOGE_TAG(kLogTag, "Falha ao apagar dados NVS");
    }
    
    return success;
}

}  // namespace pxlcam::nvs
