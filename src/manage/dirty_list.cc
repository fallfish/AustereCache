#include "common/config.h"
#include "dirty_list.h"

#include <csignal>


namespace cache {

  DirtyList::DirtyList()
  {
    size_ = 1024;
  }

  void DirtyList::setCompressionModule(std::shared_ptr<CompressionModule> compressionModule)
  {
    compressionModule_ = std::move(compressionModule);
  }

  void DirtyList::setIOModule(std::shared_ptr<IOModule> ioModule)
  {
    ioModule_ = std::move(ioModule);
  }

  DirtyList* DirtyList::getInstance() {
    if (instance == nullptr) {
      instance = new DirtyList();
    }
    return instance;
  }
  void DirtyList::release() {
    if (instance != nullptr) {
      delete instance;
    }
  }

  void DirtyList::add_latest_update(uint64_t lba, uint64_t cachedataLocation, uint32_t len)
  {
    // TODO: persist the dirty list
    latestUpdates_[lba] = std::make_pair(cachedataLocation, len);
    if (latestUpdates_.size() >= size_) {
      flush();
    }
  }

  /*
   * Description:
   *   Add a newly evicted block into the dirty list
   *   TODO: To avoid rewrite the same space (the ssd space been allocated to new request)
   *         We could persist to a special area in SSD and sync to HDD asynchronously.
   *          
   */
  void DirtyList::add_evicted_block(uint64_t ssd_data_location, uint32_t len)
  {
    //std::cout << "Add evicted block! location: " << ssd_data_location << std::endl;
    EvictedBlock evicted_block;
    evicted_block.cachedataLocation_ = ssd_data_location;
    evicted_block.len_ = len;

    evictedBlocks_.push_back(evicted_block);

    flush();
  }

  /* 
   * Description:
   *   Flush the dirty data into the HDD.
   *   Flush should be triggered under two cases.
   *   1. We come across a newly evicted block.
   *   2. The length of the dirty list exceeds a limit.
   *   Firstly, check against the dirty list whether this block containing dirty data
   *   Secondly, if there is a dirty data block, read it from ssd
   *   Thirdly, write it to the hdd
   */
#if defined(DLRU) || defined(DARC)
  void DirtyList::flush() {
    alignas(512) uint8_t data[Config::getInstance()->getChunkSize()];
    while (evictedBlocks_.size() != 0) {
      uint64_t cachedataLocation = evictedBlocks_.front().cachedataLocation_;
      uint32_t len = evictedBlocks_.front().len_;
      evictedBlocks_.pop_front();

      std::vector<uint64_t> lbas_to_flush;
      lbas_to_flush.clear();
      for (auto pr : latestUpdates_) {
        if (pr.second.first == cachedataLocation) {
          assert(pr.second.second == len);
          lbas_to_flush.push_back(pr.first);
        }
      }
      // Read cached data
      ioModule_->read(CACHE_DEVICE, cachedataLocation, data, Config::getInstance()->getChunkSize());

      for (auto lba : lbas_to_flush) {
        ioModule_->write(PRIMARY_DEVICE, lba, data, Config::getInstance()->getChunkSize());
        latestUpdates_.erase(lba);
      }
    }

    if (latestUpdates_.size() >= size_) {
      for (auto pr : latestUpdates_) {
        uint64_t lba = pr.first;
        uint64_t cachedataLocation = pr.second.first;
        // Read cached data
        ioModule_->read(CACHE_DEVICE, cachedataLocation, data, Config::getInstance()->getChunkSize());
        ioModule_->write(PRIMARY_DEVICE, lba, data, Config::getInstance()->getChunkSize());
      }
      latestUpdates_.clear();
    }
  }
#else
  void DirtyList::flush() {
    alignas(512) uint8_t compressedData[Config::getInstance()->getChunkSize()];
    alignas(512) uint8_t uncompressedData[Config::getInstance()->getChunkSize()];
    alignas(512) Metadata metadata;
    uint32_t sector_size = Config::getInstance()->getSectorSize();

    // Case 1: We have a newly evicted block.
    while (evictedBlocks_.size() != 0) {
      uint64_t ssd_data_location = evictedBlocks_.front().cachedataLocation_;
      uint64_t metadata_location = (ssd_data_location - 32LL *
                                                          Config::getInstance()->getMetadataSize() *
                                                          Config::getInstance()->getnBitsPerFPBucketId()
          ) / Config::getInstance()->getSectorSize() * Config::getInstance()->getnBitsPerFPBucketId();
      uint32_t len = evictedBlocks_.front().len_;
      evictedBlocks_.pop_front();

      std::vector<uint64_t> lbas_to_flush;
      lbas_to_flush.clear();
      bool k = 0;
      for (auto pr : latestUpdates_) {
        if (pr.second.first == ssd_data_location) {
          assert(pr.second.second == len);
          lbas_to_flush.push_back(pr.first);
        }
      }
      // Read chunk metadata (compressed length)
      ioModule_->read(CACHE_DEVICE, metadata_location, &metadata, Config::getInstance()->getMetadataSize());
      // Read cached data
      ioModule_->read(CACHE_DEVICE, ssd_data_location, compressedData, len * Config::getInstance()->getSectorSize());
      // Decompress cached data
      memset(uncompressedData, 0, 32768);
      compressionModule_->decompress(compressedData, uncompressedData,
                                     metadata.compressedLen_, Config::getInstance()->getChunkSize());

      for (auto lba : lbas_to_flush) {
        ioModule_->write(PRIMARY_DEVICE, lba, uncompressedData,
                         Config::getInstance()->getChunkSize());
        latestUpdates_.erase(lba);
      }
    }

    // Case 2: The length of the dirty list exceed a limit.
    if (latestUpdates_.size() >= size_) {
      for (auto pr : latestUpdates_) {
        uint64_t lba = pr.first;
        uint64_t ssd_data_location = pr.second.first;
        uint64_t metadata_location = (ssd_data_location - 32LL *
                                                            Config::getInstance()->getMetadataSize() *
                                                            Config::getInstance()->getnBitsPerFPBucketId()
            ) / Config::getInstance()->getSectorSize() * Config::getInstance()->getnBitsPerFPBucketId();

        // Read cached metadata (compressed length)
        ioModule_->read(CACHE_DEVICE, metadata_location, &metadata, Config::getInstance()->getMetadataSize());
        // Read cached data
        ioModule_->read(CACHE_DEVICE, ssd_data_location, compressedData, pr.second.second *
                                                              Config::getInstance()->getSectorSize());
        // Decompress cached data
        compressionModule_->decompress(compressedData, uncompressedData,
                                       metadata.compressedLen_, Config::getInstance()->getChunkSize());
        // Write uncompressed data into HDD
        ioModule_->write(PRIMARY_DEVICE, lba, uncompressedData, Config::getInstance()->getChunkSize());
      }
      latestUpdates_.clear();
    }
  }
#endif
}
