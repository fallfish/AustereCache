#include "cacheDedupCommon.h"
#include "common/config.h"
#include "common/stats.h"
#include "manage/DirtyList.h"

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

    bool Fingerprint::operator<(const Fingerprint &fp) const {
      return memcmp(v_, fp.v_, 20) < 0;
    }

    bool Fingerprint::operator==(const Fingerprint &fp) const {
      for (uint32_t i = 0; i < 20 / 4; ++i) {
        if (((uint32_t*)v_)[i] != ((uint32_t*)fp.v_)[i])
          return false;
      }
      return true;
    }

    Fingerprint::Fingerprint() {
      memset(v_, 0, sizeof(v_));
    }

    Fingerprint::Fingerprint(uint8_t *v) {
      memcpy(v_, v, 20);
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
