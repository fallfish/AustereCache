#include "common.h"
#include "common/config.h"
#include "common/stats.h"
#include "manage/dirtylist.h"

#include <iostream>
#include <cassert>
#include <csignal>

namespace cache {
    void SpaceAllocator::recycle(uint64_t location) {
      freeLocation_ = location;
    }

    uint64_t SpaceAllocator::allocate() {
      uint64_t allocatedLocation;
      if (nextLocation_ != capacity_) {
        allocatedLocation = nextLocation_;
        nextLocation_ += chunkSize_;
      } else {
        allocatedLocation = freeLocation_;
      }
      return allocatedLocation;
    }

    bool WEUAllocator::hasRecycled(uint32_t weuId) {
      return evictedWEUIds.find(weuId) != evictedWEUIds.end();
    }

    void WEUAllocator::recycle(uint32_t weuId) {
      evictedWEUIds.insert(weuId);
    }

    bool WEUAllocator::isCurrentWEUFull(uint32_t length) {
      return (length + currentOffset > weuSize_);
    }

    void WEUAllocator::allocate(uint32_t &weuId, uint32_t &offset, uint32_t length) {
      if (length + currentOffset > weuSize_)
      {
        weuId_ += 1;
        currentOffset = 0;
      }
      weuId = weuId_;
      offset = currentOffset;
      currentOffset += length;
    }
}
