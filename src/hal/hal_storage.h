/**
 * @file hal_storage.h
 * @brief Hardware Abstraction Layer - Storage Interface for PXLcam v1.2.0
 * 
 * Abstract interface for persistent storage operations.
 * Implementations can be SD card, NVS, or mocked for testing.
 * 
 * @author PXLcam Team
 * @version 1.2.0
 * @date 2024
 */

#ifndef PXLCAM_HAL_STORAGE_H
#define PXLCAM_HAL_STORAGE_H

#include <cstdint>
#include <cstddef>

namespace pxlcam {
namespace hal {

/**
 * @brief Storage operation result codes
 */
enum class StorageResult : uint8_t {
    OK = 0,             ///< Operation successful
    ERROR_INIT,         ///< Initialization failed
    ERROR_NOT_FOUND,    ///< File/key not found
    ERROR_FULL,         ///< Storage full
    ERROR_READ,         ///< Read error
    ERROR_WRITE,        ///< Write error
    ERROR_FORMAT,       ///< Format/corruption error
    ERROR_BUSY,         ///< Storage busy
    ERROR_TIMEOUT       ///< Operation timeout
};

/**
 * @brief Storage type enumeration
 */
enum class StorageType : uint8_t {
    NVS = 0,    ///< Non-volatile storage (settings)
    SD_CARD,    ///< SD card (images)
    SPIFFS,     ///< SPI Flash File System
    MOCK        ///< Mock storage for testing
};

/**
 * @brief Abstract storage interface
 * 
 * Interface for persistent storage abstraction.
 * Implement this for hardware or mock storage.
 */
class IStorage {
public:
    virtual ~IStorage() = default;

    /**
     * @brief Initialize storage
     * @return StorageResult Result code
     */
    virtual StorageResult init() = 0;

    /**
     * @brief Deinitialize storage
     * @return StorageResult Result code
     */
    virtual StorageResult deinit() = 0;

    /**
     * @brief Check if storage is initialized and ready
     * @return bool True if ready
     */
    virtual bool isReady() = 0;

    /**
     * @brief Get storage type
     * @return StorageType Type of storage
     */
    virtual StorageType getType() = 0;

    /**
     * @brief Write data to storage
     * @param key Key/filename
     * @param data Data buffer
     * @param size Data size in bytes
     * @return StorageResult Result code
     */
    virtual StorageResult write(const char* key, const void* data, size_t size) = 0;

    /**
     * @brief Read data from storage
     * @param key Key/filename
     * @param data Data buffer
     * @param size Buffer size
     * @param bytesRead Actual bytes read (output)
     * @return StorageResult Result code
     */
    virtual StorageResult read(const char* key, void* data, size_t size, size_t* bytesRead) = 0;

    /**
     * @brief Check if key/file exists
     * @param key Key/filename
     * @return bool True if exists
     */
    virtual bool exists(const char* key) = 0;

    /**
     * @brief Delete key/file
     * @param key Key/filename
     * @return StorageResult Result code
     */
    virtual StorageResult remove(const char* key) = 0;

    /**
     * @brief Get total storage size
     * @return size_t Size in bytes
     */
    virtual size_t getTotalSize() = 0;

    /**
     * @brief Get free storage space
     * @return size_t Free space in bytes
     */
    virtual size_t getFreeSize() = 0;

    /**
     * @brief Format storage (erase all data)
     * @return StorageResult Result code
     */
    virtual StorageResult format() = 0;
};

/**
 * @brief Convert StorageResult to string
 * @param result The result to convert
 * @return const char* String representation
 */
inline const char* storageResultToString(StorageResult result) {
    switch (result) {
        case StorageResult::OK:             return "OK";
        case StorageResult::ERROR_INIT:     return "INIT_ERROR";
        case StorageResult::ERROR_NOT_FOUND:return "NOT_FOUND";
        case StorageResult::ERROR_FULL:     return "STORAGE_FULL";
        case StorageResult::ERROR_READ:     return "READ_ERROR";
        case StorageResult::ERROR_WRITE:    return "WRITE_ERROR";
        case StorageResult::ERROR_FORMAT:   return "FORMAT_ERROR";
        case StorageResult::ERROR_BUSY:     return "BUSY";
        case StorageResult::ERROR_TIMEOUT:  return "TIMEOUT";
        default:                            return "UNKNOWN";
    }
}

} // namespace hal
} // namespace pxlcam

#endif // PXLCAM_HAL_STORAGE_H
