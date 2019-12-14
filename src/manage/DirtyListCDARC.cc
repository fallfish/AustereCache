#if defined(CDARC)

#include "DirtyList.h"
#include "manage/managemodule.h"

namespace cache {
void DirtyList::flush() {
  alignas(512) uint8_t compressedData[Config::getInstance().getChunkSize()];
  alignas(512) uint8_t decompressedData[Config::getInstance().getChunkSize()];

  if (latestUpdates_.size() >= size_) {
    for (auto pr : latestUpdates_) {
      uint64_t lba = pr.first;
      uint64_t cachedataLocation = pr.second.first;
      uint32_t len = pr.second.second;
      // Read cached compressedData
      IOModule::getInstance().read(CACHE_DEVICE, cachedataLocation, compressedData, len);
      CompressionModule::getInstance().decompress(compressedData, decompressedData, len, Config::getInstance().getChunkSize());
      IOModule::getInstance().write(PRIMARY_DEVICE, lba, decompressedData, Config::getInstance().getChunkSize());
    }
    latestUpdates_.clear();
  }
}

void DirtyList::flushOneBlock(uint64_t weuId, uint32_t len) {
  alignas(512) uint8_t compressedData[Config::getInstance().getChunkSize()];
  alignas(512) uint8_t decompressedData[Config::getInstance().getChunkSize()];

  std::vector<uint64_t> lbasToFlush;
  std::vector<std::pair<uint32_t, uint32_t>> locationsOfLbasToFlush;
  lbasToFlush.clear();
  locationsOfLbasToFlush.clear();
  for (auto pr : latestUpdates_) {
    // Due to that each time a WEU is evicted, all of the chunks reside in the WEU must be flushed
    uint32_t _weuId = pr.second.first >> 32;
    uint32_t offset = pr.second.first; // & 0xffffffff;
    if (_weuId == weuId) {
      lbasToFlush.push_back(pr.first);
      locationsOfLbasToFlush.push_back(std::make_pair(ManageModule::getInstance().weuToCachedataLocation_[weuId]
            + offset, pr.second.second));
    }
  }
  for (uint32_t i = 0; i < lbasToFlush.size(); ++i) {
    uint64_t lba = lbasToFlush[i];
    uint64_t cachedataLocation = locationsOfLbasToFlush[i].first;
    uint32_t len = locationsOfLbasToFlush[i].second;
    // Read cached compressedData
    IOModule::getInstance().read(CACHE_DEVICE, cachedataLocation, compressedData, Config::getInstance().getChunkSize());
    CompressionModule::getInstance().decompress(compressedData, decompressedData, len, Config::getInstance().getChunkSize());
    IOModule::getInstance().write(PRIMARY_DEVICE, lba, decompressedData, Config::getInstance().getChunkSize());
    latestUpdates_.erase(lba);
  }
}
}
#endif
