/**
 * @file mock_storage.cpp
 * @brief Mock Storage Implementation for PXLcam v1.2.0
 * 
 * @author PXLcam Team
 * @version 1.2.0
 * @date 2024
 */

#include "mock_storage.h"
#include <Arduino.h>
#include <cstring>

namespace pxlcam {
namespace mocks {

MockStorage::MockStorage(size_t totalSize)
    : initialized_(false)
    , totalSize_(totalSize)
    , usedSize_(0)
    , simulateFailure_(false)
{
}

hal::StorageResult MockStorage::init() {
    if (simulateFailure_) {
        Serial.println("[MockStorage] Init failed (simulated)");
        return hal::StorageResult::ERROR_INIT;
    }

    initialized_ = true;
    usedSize_ = 0;
    data_.clear();
    
    Serial.printf("[MockStorage] Initialized (%.2f KB total)\n", totalSize_ / 1024.0f);
    return hal::StorageResult::OK;
}

hal::StorageResult MockStorage::deinit() {
    initialized_ = false;
    Serial.println("[MockStorage] Deinitialized");
    return hal::StorageResult::OK;
}

bool MockStorage::isReady() {
    return initialized_ && !simulateFailure_;
}

hal::StorageType MockStorage::getType() {
    return hal::StorageType::MOCK;
}

hal::StorageResult MockStorage::write(const char* key, const void* data, size_t size) {
    if (!initialized_) return hal::StorageResult::ERROR_INIT;
    if (simulateFailure_) return hal::StorageResult::ERROR_WRITE;
    if (key == nullptr || data == nullptr) return hal::StorageResult::ERROR_WRITE;

    // Check space (accounting for existing data being replaced)
    size_t existingSize = 0;
    auto it = data_.find(key);
    if (it != data_.end()) {
        existingSize = it->second.size();
    }

    if (usedSize_ - existingSize + size > totalSize_) {
        Serial.printf("[MockStorage] Write failed: storage full\n");
        return hal::StorageResult::ERROR_FULL;
    }

    // Store data
    std::vector<uint8_t> buffer(size);
    std::memcpy(buffer.data(), data, size);
    
    usedSize_ -= existingSize;
    data_[key] = std::move(buffer);
    usedSize_ += size;

    Serial.printf("[MockStorage] Write: '%s' (%zu bytes)\n", key, size);
    return hal::StorageResult::OK;
}

hal::StorageResult MockStorage::read(const char* key, void* data, size_t size, size_t* bytesRead) {
    if (!initialized_) return hal::StorageResult::ERROR_INIT;
    if (simulateFailure_) return hal::StorageResult::ERROR_READ;
    if (key == nullptr || data == nullptr) return hal::StorageResult::ERROR_READ;

    auto it = data_.find(key);
    if (it == data_.end()) {
        Serial.printf("[MockStorage] Read failed: '%s' not found\n", key);
        return hal::StorageResult::ERROR_NOT_FOUND;
    }

    size_t copySize = std::min(size, it->second.size());
    std::memcpy(data, it->second.data(), copySize);
    
    if (bytesRead) {
        *bytesRead = copySize;
    }

    Serial.printf("[MockStorage] Read: '%s' (%zu bytes)\n", key, copySize);
    return hal::StorageResult::OK;
}

bool MockStorage::exists(const char* key) {
    if (!initialized_ || key == nullptr) return false;
    return data_.find(key) != data_.end();
}

hal::StorageResult MockStorage::remove(const char* key) {
    if (!initialized_) return hal::StorageResult::ERROR_INIT;
    if (simulateFailure_) return hal::StorageResult::ERROR_WRITE;
    if (key == nullptr) return hal::StorageResult::ERROR_WRITE;

    auto it = data_.find(key);
    if (it == data_.end()) {
        return hal::StorageResult::ERROR_NOT_FOUND;
    }

    usedSize_ -= it->second.size();
    data_.erase(it);
    
    Serial.printf("[MockStorage] Removed: '%s'\n", key);
    return hal::StorageResult::OK;
}

size_t MockStorage::getTotalSize() {
    return totalSize_;
}

size_t MockStorage::getFreeSize() {
    return totalSize_ - usedSize_;
}

hal::StorageResult MockStorage::format() {
    if (simulateFailure_) return hal::StorageResult::ERROR_FORMAT;

    data_.clear();
    usedSize_ = 0;
    
    Serial.println("[MockStorage] Formatted");
    return hal::StorageResult::OK;
}

void MockStorage::dump() const {
    Serial.println("[MockStorage] Contents:");
    Serial.printf("  Total: %zu bytes, Used: %zu bytes, Free: %zu bytes\n",
                  totalSize_, usedSize_, totalSize_ - usedSize_);
    Serial.printf("  Items: %zu\n", data_.size());
    
    for (const auto& item : data_) {
        Serial.printf("  - '%s': %zu bytes\n", item.first.c_str(), item.second.size());
    }
}

} // namespace mocks
} // namespace pxlcam
