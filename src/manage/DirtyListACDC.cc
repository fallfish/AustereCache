#ifdef ACDC
#include "DirtyList.h"
#include "metadata/frequentslots.h"

namespace cache {

    void DirtyList::flush() {
      alignas(512) uint8_t compressedData[Config::getInstance().getChunkSize()];
      alignas(512) uint8_t uncompressedData[Config::getInstance().getChunkSize()];
      alignas(512) Metadata metadata{};

      if (latestUpdates_.size() >= size_) {
        std::vector<std::pair<std::pair<uint64_t, uint64_t>, std::pair<uint64_t, uint32_t>>> toFlush;
        {
          std::lock_guard<std::mutex> l(listMutex_);
          for (auto pr : latestUpdates_) {
            uint64_t lba = pr.first;
            uint64_t cachedataLocation = pr.second.first;
            uint64_t metadataLocation = FPIndex::cachedataLocationToMetadataLocation(cachedataLocation);
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
      }
    }

    void DirtyList::flushOneBlock(uint64_t cachedataLocation, uint32_t len) {
      alignas(512) uint8_t compressedData[Config::getInstance().getChunkSize()];
      alignas(512) uint8_t uncompressedData[Config::getInstance().getChunkSize()];
      alignas(512) Metadata metadata{};

      std::vector<uint64_t> lbasToFlush;
      // Case 1: We have a newly evicted block.
      uint64_t metadataLocation = FPIndex::cachedataLocationToMetadataLocation(cachedataLocation);

      lbasToFlush.clear();

      // Read chunk metadata (compressed length)
      IOModule::getInstance().read(CACHE_DEVICE, metadataLocation, &metadata, Config::getInstance().getMetadataSize());
      std::set<uint64_t> lbas;
      if (metadata.numLBAs_ > MAX_NUM_LBAS_PER_CACHED_CHUNK) {
        uint64_t fpHash = Chunk::computeFingerprintHash(metadata.fingerprint_);
        lbas = FrequentSlots::getInstance().getLbas(fpHash);
      }
      for (int i = 0; i < std::min(MAX_NUM_LBAS_PER_CACHED_CHUNK, metadata.numLBAs_); ++i) {
        uint64_t lba = metadata.LBAs_[i];
        lbas.insert(lba);
      }
      for (auto lba : lbas) {
        {
          std::lock_guard<std::mutex> l(listMutex_);
          if (latestUpdates_.find(lba) != latestUpdates_.end()) {
            if (latestUpdates_[lba].first == cachedataLocation) {
              if (latestUpdates_[lba].second != len) {
                std::cout << "Not match!" << std::endl;
                std::cout << lbas.size() << metadata.numLBAs_ << std::endl;
                std::cout << lba << std::endl;
                std::cout << latestUpdates_[lba].second << " " << len << std::endl;
                assert(0);
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
        std::lock_guard<std::mutex> l(listMutex_);
        for (auto lba : lbasToFlush) {
          if (latestUpdates_[lba].first == cachedataLocation) {
            latestUpdates_.erase(lba);
          }
        }
      }
    }
}
#endif
