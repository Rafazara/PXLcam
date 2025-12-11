#pragma once
/**
 * @file nvs_store.h
 * @brief Non-Volatile Storage module for PXLcam v1.2.0
 * 
 * Provides persistent storage for user settings using ESP32 Preferences (NVS).
 * Uses namespace "pxlcam" for all stored values.
 * 
 * Usage:
 *   nvsStoreInit();
 *   saveMode(MODE_GAMEBOY);
 *   uint8_t mode = loadModeOrDefault(MODE_NORMAL);
 */

#include <stdint.h>

namespace pxlcam::nvs {

//==============================================================================
// Primary API (Simplified)
//==============================================================================

/**
 * @brief Initialize NVS storage subsystem
 * 
 * Opens the "pxlcam" namespace in read/write mode.
 * Logs error if initialization fails.
 * Safe to call multiple times (no-op if already initialized).
 */
void nvsStoreInit();

/**
 * @brief Save capture mode to NVS
 * @param mode Mode value (0=Normal, 1=GameBoy, 2=Night)
 * 
 * Persists immediately to flash. Logs on failure.
 */
void saveMode(uint8_t mode);

/**
 * @brief Load capture mode from NVS with fallback
 * @param fallback Value to return if key not found or read fails
 * @return Stored mode value or fallback
 */
uint8_t loadModeOrDefault(uint8_t fallback);

//==============================================================================
// NVS Key Constants
//==============================================================================

/// NVS key identifiers
namespace keys {
    constexpr const char* kCaptureMode = "mode";
    constexpr const char* kLastFileNum = "file_num";
    constexpr const char* kBrightness = "brightness";
    constexpr const char* kContrast = "contrast";
    constexpr const char* kFirstBoot = "first_boot";
}

//==============================================================================
// Extended API
//==============================================================================

/**
 * @brief Check if NVS is initialized and ready
 * @return true if storage is available
 */
bool isInitialized();

/**
 * @brief Initialize NVS subsystem (legacy compatibility)
 * @return true if NVS initialized successfully
 */
bool init();

/**
 * @brief Store uint8_t value
 * @param key NVS key
 * @param value Value to store
 * @return true if stored successfully
 */
bool writeU8(const char* key, uint8_t value);

/**
 * @brief Read uint8_t value
 * @param key NVS key
 * @param defaultValue Value to return if key not found
 * @return Stored value or default
 */
uint8_t readU8(const char* key, uint8_t defaultValue = 0);

/**
 * @brief Store uint32_t value
 * @param key NVS key
 * @param value Value to store
 * @return true if stored successfully
 */
bool writeU32(const char* key, uint32_t value);

/**
 * @brief Read uint32_t value
 * @param key NVS key
 * @param defaultValue Value to return if key not found
 * @return Stored value or default
 */
uint32_t readU32(const char* key, uint32_t defaultValue = 0);

/**
 * @brief Store int8_t value
 * @param key NVS key
 * @param value Value to store
 * @return true if stored successfully
 */
bool writeI8(const char* key, int8_t value);

/**
 * @brief Read int8_t value
 * @param key NVS key
 * @param defaultValue Value to return if key not found
 * @return Stored value or default
 */
int8_t readI8(const char* key, int8_t defaultValue = 0);

/**
 * @brief Check if key exists in NVS
 * @param key NVS key to check
 * @return true if key exists
 */
bool exists(const char* key);

/**
 * @brief Erase a specific key
 * @param key NVS key to erase
 * @return true if erased successfully
 */
bool erase(const char* key);

/**
 * @brief Erase all PXLcam NVS data
 * @return true if erased successfully
 */
bool eraseAll();

}  // namespace pxlcam::nvs
