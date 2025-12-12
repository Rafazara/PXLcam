/**
 * @file mock_storage.h
 * @brief Mock Storage Implementation for PXLcam v1.2.0
 * 
 * In-memory storage simulation for testing without hardware.
 * Data persists only during runtime.
 * 
 * @author PXLcam Team
 * @version 1.2.0
 * @date 2024
 */

#ifndef PXLCAM_MOCK_STORAGE_H
#define PXLCAM_MOCK_STORAGE_H

#include "../hal/hal_storage.h"
#include <map>
#include <vector>
#include <string>

namespace pxlcam {
namespace mocks {

/**
 * @brief Mock storage implementation
 * 
 * Simulates storage using in-memory data structure.
 * Useful for testing settings persistence without SD card.
 * 
 * @code
 * MockStorage storage;
 * storage.init();
 * storage.write("config", &data, sizeof(data));
 * storage.read("config", &buffer, sizeof(buffer), &bytesRead);
 * @endcode
 */
class MockStorage : public hal::IStorage {
public:
    /**
     * @brief Constructor
     * @param totalSize Simulated total storage size (default 1MB)
     */
    explicit MockStorage(size_t totalSize = 1024 * 1024);

    /**
     * @brief Destructor
     */
    ~MockStorage() override = default;

    // IStorage interface implementation
    hal::StorageResult init() override;
    hal::StorageResult deinit() override;
    bool isReady() override;
    hal::StorageType getType() override;
    hal::StorageResult write(const char* key, const void* data, size_t size) override;
    hal::StorageResult read(const char* key, void* data, size_t size, size_t* bytesRead) override;
    bool exists(const char* key) override;
    hal::StorageResult remove(const char* key) override;
    size_t getTotalSize() override;
    size_t getFreeSize() override;
    hal::StorageResult format() override;

    /**
     * @brief Get number of stored items
     * @return size_t Item count
     */
    size_t getItemCount() const { return data_.size(); }

    /**
     * @brief Simulate storage failure for testing
     * @param fail True to simulate failures
     */
    void setSimulateFailure(bool fail) { simulateFailure_ = fail; }

    /**
     * @brief Check if failure simulation is enabled
     * @return bool True if simulating failures
     */
    bool isSimulatingFailure() const { return simulateFailure_; }

    /**
     * @brief Dump storage contents to Serial (debug)
     */
    void dump() const;

private:
    bool initialized_;
    size_t totalSize_;
    size_t usedSize_;
    bool simulateFailure_;

    std::map<std::string, std::vector<uint8_t>> data_;
};

} // namespace mocks
} // namespace pxlcam

#endif // PXLCAM_MOCK_STORAGE_H
