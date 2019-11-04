#ifdef ACDC
#include "DirtyList.h"

namespace cache {

    void DirtyList::flush() {
      alignas(512) uint8_t compressedData[Config::getInstance().getChunkSize()];
      alignas(512) uint8_t uncompressedData[Config::getInstance().getChunkSize()];
      alignas(512) Metadata metadata{};

      std::vector<uint64_t> lbasToFlush;
      // Case 1: We have a newly evicted block.
      while (!evictedBlocks_.empty()) {
        uint64_t cachedataLocation = evictedBlocks_.front().cachedataLocation_;
        uint64_t metadataLocation =
          (cachedataLocation - 32LL *
                               Config::getInstance().getMetadataSize() *
                               (1u << Config::getInstance().getnBitsPerFpBucketId())
          ) / Config::getInstance().getSectorSize() * (1u << Config::getInstance().getnBitsPerFpBucketId());
        uint32_t len = evictedBlocks_.front().len_;
        evictedBlocks_.pop_front();

        lbasToFlush.clear();
        for (auto update : latestUpdates_) {
          if (update.second.first == cachedataLocation) {
            assert(update.second.second == len);
            lbasToFlush.push_back(update.first);
          }
        }
        // Read chunk metadata (compressed length)
        IOModule::getInstance().read(CACHE_DEVICE, metadataLocation, &metadata, Config::getInstance().getMetadataSize());
        // Read cached data
        if (len == Config::getInstance().getChunkSize()) {
          IOModule::getInstance().read(CACHE_DEVICE, cachedataLocation, uncompressedData, len);
        } else {
          IOModule::getInstance().read(CACHE_DEVICE, cachedataLocation, compressedData, len);
          // Decompress cached data
          memset(uncompressedData, 0, 32768);
          compressionModule_->decompress(compressedData, uncompressedData,
                                         metadata.compressedLen_, Config::getInstance().getChunkSize());
        }
        for (auto lba : lbasToFlush) {
          IOModule::getInstance().write(PRIMARY_DEVICE, lba, uncompressedData,
                                        Config::getInstance().getChunkSize());
          latestUpdates_.erase(lba);
        }
      }

      // Case 2: The length of the dirty list exceed a limit.
      if (latestUpdates_.size() >= size_) {
        for (auto pr : latestUpdates_) {
          uint64_t lba = pr.first;
          uint64_t cachedataLocation = pr.second.first;
          uint64_t metadataLocation = (cachedataLocation - 32LL *
                                                           Config::getInstance().getMetadataSize() *
                                                           (1u << Config::getInstance().getnBitsPerFpBucketId())
                                      ) / Config::getInstance().getSectorSize() * (1u << Config::getInstance().getnBitsPerFpBucketId());
          uint32_t len = pr.second.second;

          // Read chunk metadata (compressed length)
          IOModule::getInstance().read(CACHE_DEVICE, metadataLocation, &metadata, Config::getInstance().getMetadataSize());
          // Read cached data
          if (len == Config::getInstance().getChunkSize()) {
            IOModule::getInstance().read(CACHE_DEVICE, cachedataLocation, uncompressedData, len);
          } else {
            IOModule::getInstance().read(CACHE_DEVICE, cachedataLocation, compressedData, len);
            // Decompress cached data
            memset(uncompressedData, 0, Config::getInstance().getChunkSize());
            compressionModule_->decompress(compressedData, uncompressedData,
                                           metadata.compressedLen_, Config::getInstance().getChunkSize());
          }
          IOModule::getInstance().write(PRIMARY_DEVICE, lba, uncompressedData,
                                        Config::getInstance().getChunkSize());
        }
        latestUpdates_.clear();
      }
    }
}
#endif
