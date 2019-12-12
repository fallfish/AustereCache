#if defined(DLRU) || defined(DARC) || defined(BUCKETDLRU)

#include "DirtyList.h"

namespace cache {
void DirtyList::flush() {
  alignas(512) uint8_t data[Config::getInstance().getChunkSize()];

  if (latestUpdates_.size() >= size_) {
    for (auto pr : latestUpdates_) {
      uint64_t lba = pr.first;
      uint64_t cachedataLocation = pr.second.first;
      // Read cached data
      IOModule::getInstance().read(CACHE_DEVICE, cachedataLocation, data, Config::getInstance().getChunkSize());
      IOModule::getInstance().write(PRIMARY_DEVICE, lba, data, Config::getInstance().getChunkSize());
    }
    latestUpdates_.clear();
  }
}

void DirtyList::flushOneBlock(uint64_t cachedataLocation, uint32_t len) {
  alignas(512) uint8_t data[Config::getInstance().getChunkSize()];

  std::vector<uint64_t> lbas_to_flush;
  lbas_to_flush.clear();
  for (auto pr : latestUpdates_) {
    if (pr.second.first == cachedataLocation) {
      assert(pr.second.second == len);
      lbas_to_flush.push_back(pr.first);
    }
  }
  // Read cached data
  IOModule::getInstance().read(CACHE_DEVICE, cachedataLocation, data, Config::getInstance().getChunkSize());

  for (auto lba : lbas_to_flush) {
    IOModule::getInstance().write(PRIMARY_DEVICE, lba, data, Config::getInstance().getChunkSize());
    latestUpdates_.erase(lba);
  }
}
}
#endif
