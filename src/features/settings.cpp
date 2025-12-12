/**
 * @file settings.cpp
 * @brief Settings Management Implementation for PXLcam v1.2.0
 * 
 * @author PXLcam Team
 * @version 1.2.0
 * @date 2024
 */

#include "settings.h"
#include <Arduino.h>
#include <cstring>

namespace pxlcam {
namespace features {

Settings::Settings(hal::IStorage* storage)
    : storage_(storage)
    , initialized_(false)
    , firstBoot_(true)
{
}

bool Settings::init() {
    if (!storage_) {
        Serial.println("[Settings] ERROR: No storage provided");
        return false;
    }

    if (!storage_->isReady()) {
        Serial.println("[Settings] ERROR: Storage not ready");
        return false;
    }

    // Check for first boot marker
    uint8_t marker;
    size_t bytesRead;
    auto result = storage_->read(SettingsKey::FIRST_BOOT, &marker, sizeof(marker), &bytesRead);
    
    if (result == hal::StorageResult::OK && bytesRead == sizeof(marker) && marker == 0xAA) {
        firstBoot_ = false;
        Serial.println("[Settings] Previous settings detected");
    } else {
        firstBoot_ = true;
        Serial.println("[Settings] First boot detected");
    }

    initialized_ = true;
    Serial.println("[Settings] Initialized");
    return true;
}

bool Settings::load() {
    if (!initialized_ || !storage_) {
        Serial.println("[Settings] Cannot load: not initialized");
        return false;
    }

    auto& ctx = core::AppContext::instance();
    size_t bytesRead;
    bool anyLoaded = false;

    // Load camera mode
    uint8_t modeValue;
    if (storage_->read(SettingsKey::CAMERA_MODE, &modeValue, sizeof(modeValue), &bytesRead) == hal::StorageResult::OK) {
        ctx.setMode(static_cast<core::CameraMode>(modeValue));
        anyLoaded = true;
        Serial.printf("[Settings] Loaded mode: %s\n", ctx.getModeString());
    }

    // Load palette
    uint8_t paletteValue;
    if (storage_->read(SettingsKey::PALETTE, &paletteValue, sizeof(paletteValue), &bytesRead) == hal::StorageResult::OK) {
        ctx.setPalette(static_cast<core::Palette>(paletteValue));
        anyLoaded = true;
        Serial.printf("[Settings] Loaded palette: %s\n", ctx.getPaletteString());
    }

    // Load exposure settings
    core::ExposureSettings exposure;
    if (storage_->read(SettingsKey::EXPOSURE, &exposure, sizeof(exposure), &bytesRead) == hal::StorageResult::OK 
        && bytesRead == sizeof(exposure)) {
        ctx.setExposure(exposure);
        anyLoaded = true;
        Serial.println("[Settings] Loaded exposure settings");
    }

    // Load system config
    core::SystemConfig config;
    if (storage_->read(SettingsKey::SYSTEM_CONFIG, &config, sizeof(config), &bytesRead) == hal::StorageResult::OK 
        && bytesRead == sizeof(config)) {
        ctx.setConfig(config);
        anyLoaded = true;
        Serial.println("[Settings] Loaded system config");
    }

    if (!anyLoaded && firstBoot_) {
        Serial.println("[Settings] No settings found, using defaults");
    }

    return true;
}

bool Settings::save() {
    if (!initialized_ || !storage_) {
        Serial.println("[Settings] Cannot save: not initialized");
        return false;
    }

    auto& ctx = core::AppContext::instance();
    bool allSaved = true;

    // Save camera mode
    uint8_t modeValue = static_cast<uint8_t>(ctx.getMode());
    if (storage_->write(SettingsKey::CAMERA_MODE, &modeValue, sizeof(modeValue)) != hal::StorageResult::OK) {
        Serial.println("[Settings] Failed to save camera mode");
        allSaved = false;
    }

    // Save palette
    uint8_t paletteValue = static_cast<uint8_t>(ctx.getPalette());
    if (storage_->write(SettingsKey::PALETTE, &paletteValue, sizeof(paletteValue)) != hal::StorageResult::OK) {
        Serial.println("[Settings] Failed to save palette");
        allSaved = false;
    }

    // Save exposure settings
    const auto& exposure = ctx.getExposure();
    if (storage_->write(SettingsKey::EXPOSURE, &exposure, sizeof(exposure)) != hal::StorageResult::OK) {
        Serial.println("[Settings] Failed to save exposure");
        allSaved = false;
    }

    // Save system config
    const auto& config = ctx.getConfig();
    if (storage_->write(SettingsKey::SYSTEM_CONFIG, &config, sizeof(config)) != hal::StorageResult::OK) {
        Serial.println("[Settings] Failed to save system config");
        allSaved = false;
    }

    if (allSaved) {
        Serial.println("[Settings] All settings saved");
    }

    return allSaved;
}

bool Settings::resetToDefaults() {
    if (!initialized_) return false;

    auto& ctx = core::AppContext::instance();
    ctx.init();  // Reset to defaults

    // Clear storage
    if (storage_) {
        storage_->remove(SettingsKey::CAMERA_MODE);
        storage_->remove(SettingsKey::PALETTE);
        storage_->remove(SettingsKey::EXPOSURE);
        storage_->remove(SettingsKey::SYSTEM_CONFIG);
        storage_->remove(SettingsKey::FIRST_BOOT);
    }

    firstBoot_ = true;
    Serial.println("[Settings] Reset to defaults");
    return true;
}

void Settings::markFirstBootComplete() {
    if (!initialized_ || !storage_) return;

    uint8_t marker = 0xAA;
    if (storage_->write(SettingsKey::FIRST_BOOT, &marker, sizeof(marker)) == hal::StorageResult::OK) {
        firstBoot_ = false;
        Serial.println("[Settings] First boot marked complete");
    }
}

bool Settings::saveCameraMode(core::CameraMode mode) {
    if (!initialized_ || !storage_) return false;
    
    uint8_t value = static_cast<uint8_t>(mode);
    return storage_->write(SettingsKey::CAMERA_MODE, &value, sizeof(value)) == hal::StorageResult::OK;
}

bool Settings::savePalette(core::Palette palette) {
    if (!initialized_ || !storage_) return false;
    
    uint8_t value = static_cast<uint8_t>(palette);
    return storage_->write(SettingsKey::PALETTE, &value, sizeof(value)) == hal::StorageResult::OK;
}

bool Settings::saveExposure(const core::ExposureSettings& exposure) {
    if (!initialized_ || !storage_) return false;
    
    return storage_->write(SettingsKey::EXPOSURE, &exposure, sizeof(exposure)) == hal::StorageResult::OK;
}

bool Settings::saveSystemConfig(const core::SystemConfig& config) {
    if (!initialized_ || !storage_) return false;
    
    return storage_->write(SettingsKey::SYSTEM_CONFIG, &config, sizeof(config)) == hal::StorageResult::OK;
}

bool Settings::isAvailable() const {
    return initialized_ && storage_ && storage_->isReady();
}

uint8_t Settings::calculateChecksum(const void* data, size_t size) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint8_t sum = 0;
    for (size_t i = 0; i < size; i++) {
        sum ^= bytes[i];
    }
    return sum;
}

} // namespace features
} // namespace pxlcam
