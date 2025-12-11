#pragma once
/**
 * @file nvs_store.h
 * @brief Non-Volatile Storage wrapper for PXLcam v1.2.0
 * 
 * Provides persistent storage for user settings using ESP32 NVS.
 * All settings are stored in the "pxlcam" namespace.
 */

#include <stdint.h>

namespace pxlcam::nvs {

/// NVS key identifiers
namespace keys {
    constexpr const char* kCaptureMode = "cap_mode";
    constexpr const char* kLastFileNum = "file_num";
    constexpr const char* kBrightness = "brightness";
    constexpr const char* kContrast = "contrast";
    constexpr const char* kFirstBoot = "first_boot";
}

/// Initialize NVS subsystem
/// @return true if NVS initialized successfully
bool init();

/// Check if NVS is initialized
bool isInitialized();

/// Store uint8_t value
/// @param key NVS key
/// @param value Value to store
/// @return true if stored successfully
bool writeU8(const char* key, uint8_t value);

/// Read uint8_t value
/// @param key NVS key
/// @param defaultValue Value to return if key not found
/// @return Stored value or default
uint8_t readU8(const char* key, uint8_t defaultValue = 0);

/// Store uint32_t value
/// @param key NVS key
/// @param value Value to store
/// @return true if stored successfully
bool writeU32(const char* key, uint32_t value);

/// Read uint32_t value
/// @param key NVS key
/// @param defaultValue Value to return if key not found
/// @return Stored value or default
uint32_t readU32(const char* key, uint32_t defaultValue = 0);

/// Store int8_t value
/// @param key NVS key
/// @param value Value to store
/// @return true if stored successfully
bool writeI8(const char* key, int8_t value);

/// Read int8_t value
/// @param key NVS key
/// @param defaultValue Value to return if key not found
/// @return Stored value or default
int8_t readI8(const char* key, int8_t defaultValue = 0);

/// Check if key exists in NVS
/// @param key NVS key to check
/// @return true if key exists
bool exists(const char* key);

/// Erase a specific key
/// @param key NVS key to erase
/// @return true if erased successfully
bool erase(const char* key);

/// Erase all PXLcam NVS data
/// @return true if erased successfully
bool eraseAll();

/// Commit pending changes (called automatically by write functions)
/// @return true if commit successful
bool commit();

}  // namespace pxlcam::nvs
