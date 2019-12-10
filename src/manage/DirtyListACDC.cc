#ifdef ACDC
#include "DirtyList.h"

namespace cache {

    void DirtyList::flush() {
      alignas(512) uint8_t compressedData[Config::getInstance().getChunkSize()];
      alignas(512) uint8_t uncompressedData[Config::getInstance().getChunkSize()];
      alignas(512) Metadata metadata{};

      // Case 1: The length of the dirty list exceed a limit.
      // Requirement: the global lock must be held
      if (latestUpdates_.size() >= size_) {
        std::vector<std::pair<std::pair<uint64_t, uint64_t>, std::pair<uint64_t, uint32_t>>> toFlush;
        {
          std::lock_guard<std::mutex> l(listMutex_);
          for (auto pr : latestUpdates_) {
            uint64_t lba = pr.first;
            uint64_t cachedataLocation = pr.second.first;
            uint64_t metadataLocation = (cachedataLocation - 32LL * 
                Config::getInstance().getMetadataSize() * Config::getInstance().getnFpBuckets()
                ) / Config::getInstance().getSectorSize() * Config::getInstance().getMetadataSize();
            uint32_t len = pr.second.second;
            toFlush.emplace_back(std::make_pair(lba, cachedataLocation), std::make_pair(metadataLocation, len));
          }
        }

        for (auto pr : toFlush) {
            uint64_t lba = pr.first.first;
            uint64_t cachedataLocation = pr.first.second;
            uint64_t metadataLocation = pr.second.first;
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

        {
          std::lock_guard<std::mutex> l(listMutex_);
          for (auto pr : toFlush) {
            uint64_t lba = pr.first.first;
            uint64_t cachedataLocation = pr.first.second;
            uint32_t len = pr.second.second;
            if (latestUpdates_.find(lba) != latestUpdates_.end()) {
              if (latestUpdates_[lba].first == cachedataLocation &&
                  latestUpdates_[lba].second == len) {
                latestUpdates_.erase(lba);
              }
            }
          }
        }
        return ;
      }

      std::vector<uint64_t> lbasToFlush;
      // Case 1: We have a newly evicted block.
      while (!evictedBlocks_.empty()) {
        uint64_t cachedataLocation = evictedBlocks_.front().cachedataLocation_;
        uint64_t metadataLocation =
          (cachedataLocation - 32LL * 
           Config::getInstance().getMetadataSize() * Config::getInstance().getnFpBuckets()
          ) / Config::getInstance().getSectorSize() * Config::getInstance().getMetadataSize();
        uint32_t len = evictedBlocks_.front().len_;
        evictedBlocks_.pop_front();

        lbasToFlush.clear();
        // Read chunk metadata (compressed length)
        IOModule::getInstance().read(CACHE_DEVICE, metadataLocation, &metadata, Config::getInstance().getMetadataSize());
        for (int i = 0; i < metadata.numLBAs_; ++i) {
          uint64_t lba = metadata.LBAs_[i];
          {
            std::lock_guard<std::mutex> l(listMutex_);
            if (latestUpdates_.find(lba) != latestUpdates_.end()) {
              if (latestUpdates_[lba].first == cachedataLocation) {
                if (latestUpdates_[lba].second != len) {
                  std::cout << lba << std::endl;
                  std::cout << latestUpdates_[lba].second << " " << len << std::endl;
                }
                lbasToFlush.push_back(lba);
              }

            }
          }
        }
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
        }
        {
          // Hold the lock to avoid race
          for (auto lba : lbasToFlush) {
            latestUpdates_.erase(lba);
          }
        }
      }

    }
}
#endif
